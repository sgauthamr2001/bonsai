#include "Lower/TypeInference.h"

#include "IR/TypeEnforcement.h"
#include "IR/IRVisitor.h"

#include "Error.h"
#include "Scope.h"

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

ir::Function infer_types(const ir::Function &fnotypes, const ir::Program &program) {
    ir::Function ftypes;
    internal_error << "TODO: implement function type inference";
    return ftypes;
}

}  // namespace

ir::Program infer_types(const ir::Program &program) {
    program.dump(std::cout);
    ir::Program new_program;
    new_program.externs = program.externs;
    new_program.types = program.types;
    ir::global_enable_type_enforcement();

    std::vector<std::string> topo_order = func_topological_order(program);
    for (const auto &f : topo_order) {
        new_program.funcs[f] = infer_types(program.funcs.at(f), new_program);
    }

    return new_program;
}

}  // namespace parser
}  // namespace bonsai
