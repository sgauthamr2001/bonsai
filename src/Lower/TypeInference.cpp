#include "Lower/TypeInference.h"

#include "IR/Analysis.h"
#include "IR/TypeEnforcement.h"
#include "IR/IRPrinter.h"
#include "IR/IRVisitor.h"

#include "Error.h"
#include "Scope.h"
#include "Utils.h"

#include <map>
#include <set>
#include <string>

namespace bonsai {
namespace lower {

namespace {

struct UndefCallGraphBuilder : public ir::IRVisitor {
    // TODO: this does NOT work for interfaces!
    std::set<std::string> undef_calls;
    bool in_call = false;

    void clear() {
        undef_calls.clear();
        in_call = false;
    }

    void visit(const ir::Call *node) override {
        internal_assert(!in_call) << "Nested call, how can that happen?" << node;
        in_call = true;
        node->func.accept(this);
        internal_assert(in_call) << "Somehow un-nested call, how can that happen?" << node;
        in_call = false;

        // possibly gather call values from arguments.
        for (const auto& a : node->args) {
            a.accept(this);
        }        
    }

    void visit(const ir::Access *node) override {
        internal_assert(!in_call) << "TODO: support call graph through interface" << node;
        IRVisitor::visit(node);
    }

    void visit(const ir::Var *node) override {
        if (in_call) {
            internal_assert(!node->type.defined() || node->type.is<ir::Function_t>())
                << "Somehow in call but Var is not a function type: " << node;
            if (node->type.is<ir::Function_t>()) {
                internal_assert(node->type.as<ir::Function_t>()->ret_type.defined())
                    << "TODO: must have changed Function_t to allow undefined ret_type, need to update call graph builder!" << node;
            }
            if (!node->type.defined()) {
                undef_calls.insert(node->name);
            }
        }
    }
};


std::map<std::string, std::set<std::string>> build_undef_call_graph(const ir::Program &program) {
    UndefCallGraphBuilder builder;
    std::map<std::string, std::set<std::string>> undef_call_graph;
    for (const auto &f : program.funcs) {
        // TODO: do we need this for funcs with defined ret_types? probably not.
        if (f.second.ret_type.defined()) {
            undef_call_graph[f.first] = {}; // can be evaluated in any order.
        } else {
            f.second.body.accept(&builder);
            undef_call_graph[f.first] = std::move(builder.undef_calls);
            builder.clear();
        }
    }
    return undef_call_graph;
}

std::vector<std::string> func_topological_order(const ir::Program &program) {
    // Return the order that type inference should run in.
    const std::map<std::string, std::set<std::string>> undef_call_graph = build_undef_call_graph(program);
    // DFS-based topological sorting, https://en.wikipedia.org/wiki/Topological_sorting#Depth-first_search

    std::vector<std::string> order;
    std::set<std::string> seen;
    std::set<std::string> visiting;

    std::function<void(const std::string &)> visit = [&](const std::string &fname) -> void {
        if (seen.contains(fname)) return;
        internal_assert(!visiting.contains(fname))
            << "Type inference found a cycle containing function: " << fname
            << "\nYou may need to specify return types on one or more functions to break the cycle";
        visiting.insert(fname);
        for (const auto &gname : undef_call_graph.at(fname)) {
            visit(gname);
        }
        visiting.erase(fname);
        seen.insert(fname);
        order.push_back(fname);
    };

    for (const auto &f : program.funcs) {
        visit(f.first);
    }

    return order;
}

ir::Stmt replace_undef_calls(const ir::Stmt &stmt, const std::map<std::string, ir::Type> &func_types) {
    struct ReplaceUndefCalls : public ir::IRMutator {
        ReplaceUndefCalls(const std::map<std::string, ir::Type> &_func_types) : func_types(_func_types) {}
    private:
        const std::map<std::string, ir::Type> &func_types;
    public:
        ir::Expr visit(const ir::Var *node) override {
            // TODO: this assumes func names nver conflict with variables, which
            // should be enforced with scopes in the parser. That is not currently the case.
            if (!node->type.defined()) {
                if (func_types.contains(node->name)) {
                    return ir::Var::make(func_types.at(node->name), node->name);
                } else {
                    // TODO: we could error here, but there might be vars that we just don't know the type of yet.
                    // for now, leave this node, the type might be inferred later (e.g. by return coercion)
                    return node;
                }
            } else {
                return node;
            }
        }
    };

    ReplaceUndefCalls replacer(func_types);
    return replacer.mutate(stmt);
}

ir::Stmt coerce_return_types(const ir::Stmt &stmt, const ir::Type &ret_type) {
    struct CoerceReturnTypes : public ir::IRMutator {
        CoerceReturnTypes(const ir::Type &_ret_type) : ret_type(_ret_type) {}
    private:
        const ir::Type &ret_type;
    public:
        ir::Stmt visit(const ir::Return *node) override {
            // TODO: may need to back-propagate information to variable declarations...
            if (!node->value.type().defined()) {
                // TODO: support is_castable!
                internal_assert(is_const(node->value))
                    << "Cannot coerce value: " << node->value << " into return type: " << ret_type;
                return ir::Return::make(constant_cast(ret_type, node->value));
            } else {
                return ir::IRMutator::visit(node);
            }
        }
    };

    CoerceReturnTypes coercer(ret_type);
    return coercer.mutate(stmt);
}

bool has_undef_expr_types(const ir::Stmt &stmt) {
    // We use IRMutator to override mutate() instead of all Expr methods in IRVisitor
    struct FindUndefTypes : public ir::IRMutator {
        bool undef_types = false;
        ir::Expr mutate(const ir::Expr &expr) override {
            undef_types = undef_types || !expr.type().defined();
            // TODO: record all undefined exprs for error message?
            if (undef_types) {
                return expr;
            } else {
                return IRMutator::mutate(expr);
            }
        }

        ir::Stmt mutate(const ir::Stmt &stmt) override {
            if (undef_types) {
                return stmt;
            } else {
                return IRMutator::mutate(stmt);
            }
        }
    };

    FindUndefTypes finder;
    finder.mutate(stmt);
    return finder.undef_types;
}

ir::Function infer_types(const ir::Function &fnotypes, const ir::Program &program, const std::map<std::string, ir::Type> &func_types) {
    ir::Function ftypes;
    ftypes.name = fnotypes.name;
    ftypes.args = fnotypes.args;

    // First, try to use function types inferred so far to replace undefined call sites.
    ftypes.body = replace_undef_calls(fnotypes.body, func_types);

    // If we know the return type (due to annotations), try to coerce all returns to it.
    // If we don't know from annotations, try to infer from some return type, then coerce.
    ftypes.ret_type = fnotypes.ret_type.defined() ? fnotypes.ret_type : ir::get_return_type(fnotypes.body);
    ftypes.body = coerce_return_types(fnotypes.body, ftypes.ret_type);

    // TODO: is there more that we can do?

    internal_assert(!has_undef_expr_types(ftypes.body))
        << "Type inference failed to infer all types of: " << fnotypes.body
        << "Inferred: " << ftypes.body;
    return ftypes;
}

}  // namespace

ir::Program infer_types(const ir::Program &program) {
    // program.dump(std::cout);
    ir::Program new_program;
    new_program.externs = program.externs;
    new_program.types = program.types;
    ir::global_enable_type_enforcement();

    std::vector<std::string> topo_order = func_topological_order(program);
    std::map<std::string, ir::Type> func_types;

    for (const auto &f : topo_order) {
        new_program.funcs[f] = infer_types(program.funcs.at(f), new_program, func_types);
        {
            const size_t n_args = new_program.funcs[f].args.size();
            std::vector<ir::Type> arg_types(n_args);
            for (size_t i = 0; i < n_args; i++) {
                arg_types[i] = new_program.funcs[f].args[i].type;
            }
            func_types[f] = ir::Function_t::make(new_program.funcs[f].ret_type, arg_types);
        }
    }

    // std::cout << "\n\nInferred types:\n";
    // new_program.dump(std::cout);

    return new_program;
}

}  // namespace parser
}  // namespace bonsai
