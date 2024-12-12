#include "IR/Analysis.h"

#include "IR/IREquality.h"
#include "IR/IRPrinter.h"
#include "IR/IRVisitor.h"

#include <set>

namespace bonsai {
namespace ir {

namespace {

struct GatherFreeVars : public IRVisitor {
    // return in seen-order
    std::vector<std::pair<std::string, Type>> free_vars;
    // no duplicates
    std::set<std::string> seen_vars;

    void visit(const Var *node) override {
        if (seen_vars.count(node->name) == 0) {
            free_vars.emplace_back(node->name, node->type);
            seen_vars.insert(node->name);
        }
    }

    void visit(const Store *node) override {
        if (seen_vars.count(node->name) == 0) {
            Type ptr_t = Ptr_t::make(node->value.type());
            free_vars.emplace_back(node->name, std::move(ptr_t));
            seen_vars.insert(node->name);
        }
        // TODO: consider implications on recursive definition.
        IRVisitor::visit(node);
    }

    void visit(const LetStmt *node) override {
        // TODO: use Scope + ScopedBinding?
        seen_vars.insert(node->name);
        node->body.accept(this);
        seen_vars.erase(node->name);
    }
};


struct AlwaysReturns : public IRVisitor {
    bool returns = false;
    void visit(const Return *) override {
        returns = true;
    }

    void visit(const Store *) override {
        returns = false;
    }

    void visit(const LetStmt *node) override {
        node->body.accept(this);
    }

    void visit(const IfElse *node) override {
        if (node->else_body.defined()) {
            node->then_body.accept(this);
            if (returns) {
                node->else_body.accept(this);
            }
        } else {
            returns = false;
        }
    }

    void visit(const Sequence *node) override {
        for (size_t i = 0; i < node->stmts.size() - 1; i++) {
            const Stmt &stmt = node->stmts[i];
            stmt.accept(this);
            if (returns) {
                throw std::runtime_error("Sequence always returns in the middle: " + to_string(node));
            }
        }
        node->stmts.back().accept(this);
    }
};

struct ReturnType : public IRVisitor {
    Type type;

    void visit(const Return *node) override {
        type = node->value.type();
    }

    void visit(const Store *) override {
        type = Type();
    }

    void visit(const LetStmt *node) override {
        node->body.accept(this);
    }

    void visit(const IfElse *node) override {
        node->then_body.accept(this);
        Type then_type = std::move(type);
        type = Type();
        if (node->else_body.defined()) {
            node->else_body.accept(this);
        }
        if (then_type.defined() && type.defined()) {
            // TODO: structural equality.
            if (!equals(then_type, type)) {
                throw std::runtime_error("IfElse returns two separate types:\n" + to_string(node));
            }
        } else if (then_type.defined()) {
            type = then_type;
        }
    }

    void visit(const Sequence *node) override {
        Type prev_type;
        for (size_t i = 0; i < node->stmts.size(); i++) {
            const Stmt &stmt = node->stmts[i];
            type = Type();
            stmt.accept(this);
            if (type.defined()) {
                if (prev_type.defined() && !equals(type, prev_type)) {
                    throw std::runtime_error("Sequence returns two separate types:\n" + to_string(node));
                }
                prev_type = type;
            }
        }
        type = prev_type; // any possible return type.
    }
};

struct GatherStructTypes : public IRVisitor {
    // return in seen-order
    std::vector<const Struct_t *> struct_types;
    // no duplicates
    std::set<std::string> seen_structs;

    void visit(const Struct_t *node) override {
        if (seen_structs.count(node->name) == 0) {
            struct_types.push_back(node);
            seen_structs.insert(node->name);
        }
    }

    // TODO: override all Expr/Stmt ops that might interact
    // with a Struct_t and make sure the IRVisitor recurses

    void visit(const Build *node) override {
        node->type.accept(this);
        IRVisitor::visit(node);
    }

    void visit(const Access *node) override {
        node->value.type().accept(this);
        IRVisitor::visit(node);
    }
};

}  // namespace


std::vector<std::pair<std::string, Type>> gather_free_vars(const Expr &expr) {
    GatherFreeVars gather;
    expr.accept(&gather);
    return std::move(gather.free_vars);
}

std::vector<std::pair<std::string, Type>> gather_free_vars(const Stmt &stmt) {
    GatherFreeVars gather;
    stmt.accept(&gather);
    return std::move(gather.free_vars);
}

bool always_returns(const Stmt &stmt) {
    AlwaysReturns check;
    stmt.accept(&check);
    return check.returns;
}

Type get_return_type(const Stmt &stmt) {
    assert(always_returns(stmt));
    ReturnType getter;
    stmt.accept(&getter);
    assert(getter.type.defined());
    return getter.type;
}

std::vector<const Struct_t *> gather_struct_types(const Stmt &stmt) {
    GatherStructTypes gather;
    stmt.accept(&gather);
    return std::move(gather.struct_types);
}

bool is_constant_expr(const Expr &expr) {
    // TODO: constant fold first?
    if (expr.is<IntImm>() ||
        // expr.is<UIntImm>() ||
        expr.is<FloatImm>()) {
        return true;
    } else if (expr.is<Broadcast>()) {
        return is_constant_expr(expr.as<Broadcast>()->value);
    } else if (expr.is<VectorReduce>()) {
        return is_constant_expr(expr.as<VectorReduce>()->value);
    } else if (expr.is<Ramp>()) {
        return is_constant_expr(expr.as<Ramp>()->base) && is_constant_expr(expr.as<Ramp>()->stride);
    } else if (expr.is<Build>()) {
        for (const auto& value : expr.as<Build>()->values) {
            if (!is_constant_expr(value)) {
                return false;
            }
        }
        return true;
    } else if (expr.is<Access>()) {
        // TODO: this is unnecessarily restrictive for now,
        // should only be checking if the accessed field is a constant.
        return is_constant_expr(expr.as<Access>()->value);
    } else {
        // TODO: Intrinsic, Lambda, GeomOp, SetOp, Call (constant folding)
        throw std::runtime_error("is_constant_expr() called on: " + to_string(expr));
    }
}

}  // namespace ir
}  // namespace bonsai
