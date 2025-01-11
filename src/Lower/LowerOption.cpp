#include "Lower/Canonicalize.h"

#include "IR/Equality.h"
#include "IR/Mutator.h"

#include "Error.h"
#include "Utils.h"

namespace bonsai {
namespace lower {

namespace {

struct RewriteOptions : public ir::Mutator {
    ir::Type Bool = ir::Bool_t::make();
    std::map<ir::Type, ir::Type, ir::TypeLessThan> rewrite_map;
    // number of option etypes rewritten
    size_t counter = 0;
    // whether an expression is being interpreted as a boolean value or not.
    bool as_bool = false;

    ir::Type construct_option_struct(const ir::Type &etype) {
        std::string struct_name = "?option" + std::to_string(counter++);
        ir::Struct_t::Map fields;
        fields.emplace_back("value", etype);
        fields.emplace_back("set", Bool);
        return ir::Struct_t::make(struct_name, fields);
    }

    ir::Type mutate(const ir::Type &type) override {
        ir::Type rec = ir::Mutator::mutate(type);
        if (rec.is<ir::Option_t>()) {
            const ir::Type &etype = rec.as<ir::Option_t>()->etype;
            auto iter = rewrite_map.find(etype);
            if (iter != rewrite_map.end()) {
                return iter->second;
            }
            ir::Type repl = construct_option_struct(etype);
            rewrite_map[etype] = repl;
            return repl;
        } else {
            return rec;
        }
    }

    // Why are these necessary?? This is dumb.
    using ir::Mutator::mutate;
    // ir::Expr mutate(const ir::Expr &expr) override {
    //     return ir::Mutator::mutate(expr);
    // }
    // ir::Stmt mutate(const ir::Stmt &stmt) override {
    //     return ir::Mutator::mutate(stmt);
    // }

    ir::Expr visit(const ir::Build *node) override {
        ir::Expr expr = ir::Mutator::visit(node);
        node = expr.as<ir::Build>();
        internal_assert(node);
        if (node->type.is<ir::Option_t>()) {
            ir::Type new_type = mutate(node->type);
            if (node->values.empty()) {
                // Build an "empty" struct - this sets the bool `set` to false by default.
                return ir::Build::make(std::move(new_type), {});
            } else {
                internal_assert(node->values.size() == 1) << "Error in lowering build of Option_t, expected one argument by received: " << node->values.size();
                return ir::Build::make(std::move(new_type), {node->values[0], ir::BoolImm::make(true)});
            }
        } else {
            return expr;
        }
    }

    // rewrite cast<option>(value) -> build<struct_option>(value)
    ir::Expr visit(const ir::Cast *node) override {
        ir::Expr expr = ir::Mutator::visit(node);
        node = expr.as<ir::Cast>();
        internal_assert(node);
        if (node->type.is<ir::Option_t>()) {
            ir::Type new_type = mutate(node->type);
            return ir::Build::make(std::move(new_type), {node->value, ir::BoolImm::make(true)});
        } else {
            return expr;
        }
    }

    // TODO: which other relevant nodes are there?
    // TODO: if coercion and option dereferencing.
};

ir::Type lower_option(const ir::Type &type) {
    return RewriteOptions().mutate(type);
}

ir::Expr lower_option(const ir::Expr &expr) {
    return RewriteOptions().mutate(expr);
}

ir::Stmt lower_option(const ir::Stmt &stmt) {
    return RewriteOptions().mutate(stmt);
}

bool contains_option(const ir::Type &type) {
    return true;
}

}  // namespace

ir::Program lower_option(const ir::Program &program) {
    ir::Program new_program;

    // Can externs have option types? I don't think so.
    for (const auto &[name, type] : program.externs) {
        internal_assert(!contains_option(type))
            << "Lowering failure, found option type in extern: " << name << " with type: " << type;
    }
    new_program.externs = program.externs;

    for (const auto &[t, type] : program.types) {
        new_program.types[t] = lower_option(type);
    }

    for (const auto &[f, func] : program.funcs) {
        std::vector<ir::Function::Argument> args(func.args.size());
        for (size_t i = 0; i < args.size(); i++) {
            const auto &arg = func.args[i];
            args[i] = ir::Function::Argument{arg.name, lower_option(arg.type), lower_option(arg.default_value)};
        }
        ir::Type ret_type = lower_option(func.ret_type);
        ir::Stmt body = lower_option(func.body);

        new_program.funcs[f] = ir::Function(func.name, std::move(args), std::move(ret_type), std::move(body));
    }

    new_program.main_body = lower_option(program.main_body);
    return new_program;
}

}  // namespace parser
}  // namespace bonsai
