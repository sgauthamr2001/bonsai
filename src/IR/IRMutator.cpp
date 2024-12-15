#include "IR/IRMutator.h"

#include "IR/Expr.h"
#include "IR/Stmt.h"
#include "IR/Type.h"

namespace bonsai {
namespace ir {

namespace {

template<typename T>
std::pair<std::vector<T>, bool> visit_list(IRMutator *v, const std::vector<T> &l) {
    bool not_changed = true;
    const size_t n = l.size();
    std::vector<T> _l(n);
    for (size_t i = 0; i < n; i++) {
        _l[i] = v->mutate(l[i]);
        not_changed = not_changed && _l[i].same_as(l[i]);
    }
    return {std::move(_l), not_changed};
}
}  // namespace

Type IRMutator::mutate(const Type &type) {
    return type.defined() ? type.get()->mutate_type(this) : Type();
}

Expr IRMutator::mutate(const Expr &expr) {
    return expr.defined() ? expr.get()->mutate_expr(this) : Expr();
}

Stmt IRMutator::mutate(const Stmt &stmt) {
    return stmt.defined() ? stmt.get()->mutate_stmt(this) : Stmt();
}

Type IRMutator::visit(const Int_t *node) {
    return node;
}

Type IRMutator::visit(const UInt_t *node) {
    return node;
}

Type IRMutator::visit(const Float_t *node) {
    return node;
}

Type IRMutator::visit(const Bool_t *node) {
    return node;
}

Type IRMutator::visit(const Ptr_t *node) {
    Type etype = mutate(node->etype);
    if (etype.same_as(node->etype)) {
        return node;
    } else {
        return Ptr_t::make(std::move(etype));
    }
}

Type IRMutator::visit(const Vector_t *node) {
    Type etype = mutate(node->etype);
    if (etype.same_as(node->etype)) {
        return node;
    } else {
        return Vector_t::make(std::move(etype), node->lanes);
    }
}

Type IRMutator::visit(const Struct_t *node) {
    Struct_t::Map fields = node->fields; // copy
    bool changed = false;
    // TODO: lift into helper func?
    for (auto& [key, value] : fields) {
        Type t = mutate(value);
        if (!t.same_as(value)) {
            changed = true;
            value = t; // edits mapped value
        }
    }

    if (!changed) {
        return node;
    } else {
        return Struct_t::make(node->name, std::move(fields));
    }
}

Type IRMutator::visit(const Tuple_t *node) {
    auto [etypes, not_changed] = visit_list(this, node->etypes);
    if (not_changed) {
        return node;
    } else {
        return Tuple_t::make(std::move(etypes));
    }
}

Type IRMutator::visit(const Option_t *node) {
    Type etype = mutate(node->etype);
    if (etype.same_as(node->etype)) {
        return node;
    } else {
        return Option_t::make(std::move(etype));
    }
}

Type IRMutator::visit(const Set_t *node) {
    Type etype = mutate(node->etype);
    if (etype.same_as(node->etype)) {
        return node;
    } else {
        return Set_t::make(std::move(etype));
    }
}

Type IRMutator::visit(const Function_t *node) {
    Type ret_type = mutate(node->ret_type);
    auto [arg_types, not_changed] = visit_list(this, node->arg_types);
    if (ret_type.same_as(node->ret_type) && not_changed) {
        return node;
    } else {
        return Function_t::make(std::move(ret_type), std::move(arg_types));
    }
}


Expr IRMutator::visit(const IntImm *node) {
    return node;
}

Expr IRMutator::visit(const UIntImm *node) {
    return node;
}

Expr IRMutator::visit(const FloatImm *node) {
    return node;
}

Expr IRMutator::visit(const Var *node) {
    return node;
}

Expr IRMutator::visit(const BinOp *node) {
    Expr a = mutate(node->a);
    Expr b = mutate(node->b);
    if (a.same_as(node->a) && b.same_as(node->b)) {
        return node;
    } else {
        return BinOp::make(node->op, std::move(a), std::move(b));
    }
}

Expr IRMutator::visit(const Broadcast *node) {
    Expr value = mutate(node->value);
    if (value.same_as(node->value)) {
        return node;
    } else {
        return Broadcast::make(node->lanes, std::move(value));
    }
}

Expr IRMutator::visit(const VectorReduce *node) {
    Expr value = mutate(node->value);
    if (value.same_as(node->value)) {
        return node;
    } else {
        return VectorReduce::make(node->op, std::move(value));
    }
}

Expr IRMutator::visit(const Ramp *node) {
    Expr base = mutate(node->base);
    Expr stride = mutate(node->stride);
    if (base.same_as(node->base) &&
        stride.same_as(node->stride)) {
        return node;
    }
    return Ramp::make(std::move(base), std::move(stride), node->lanes);
}

Expr IRMutator::visit(const Build *node) {
    auto [values, not_changed] = visit_list(this, node->values);
    if (not_changed) {
        return node;
    } else {
        // TODO: can mutation ever change underlying type?
        // If so, need to explicitly handle imo.
        return Build::make(node->type, std::move(values));
    }
}

Expr IRMutator::visit(const Access *node) {
    Expr value = mutate(node->value);
    if (value.same_as(node->value)) {
        return node;
    } else {
        return Access::make(node->field, std::move(value));
    }
}

Expr IRMutator::visit(const Intrinsic *node) {
    Expr value = mutate(node->value);
    if (value.same_as(node->value)) {
        return node;
    } else {
        return Intrinsic::make(node->op, std::move(value));
    }
}

Expr IRMutator::visit(const Lambda *node) {
    Expr value = mutate(node->value);
    if (value.same_as(node->value)) {
        return node;
    } else {
        return Lambda::make(node->args, std::move(value));
    }
}

Expr IRMutator::visit(const GeomOp *node) {
    Expr a = mutate(node->a);
    Expr b = mutate(node->b);
    if (a.same_as(node->a) && b.same_as(node->b)) {
        return node;
    } else {
        return GeomOp::make(node->op, std::move(a), std::move(b));
    }
}


Expr IRMutator::visit(const SetOp *node) {
    Expr a = mutate(node->a);
    Expr b = mutate(node->b);
    if (a.same_as(node->a) && b.same_as(node->b)) {
        return node;
    } else {
        return SetOp::make(node->op, std::move(a), std::move(b));
    }
}

Expr IRMutator::visit(const Call *node) {
    Expr func = mutate(node->func);
    auto [args, not_changed] = visit_list(this, node->args);
    if (func.same_as(node->func) && not_changed) {
        return node;
    } else {
        return Call::make(std::move(func), std::move(args));
    }
}


Stmt IRMutator::visit(const Return *node) {
    Expr value = mutate(node->value);
    if (value.same_as(node->value)) {
        return node;
    } else {
        return Return::make(std::move(value));
    }
}

Stmt IRMutator::visit(const Store *node) {
    Expr index = mutate(node->index);
    Expr value = mutate(node->value);
    if (index.same_as(node->index) && value.same_as(node->value)) {
        return node;
    } else {
        return Store::make(node->name, std::move(index), std::move(value));
    }
}

Stmt IRMutator::visit(const LetStmt *node) {
    Expr value = mutate(node->value);
    Stmt body = mutate(node->body);
    if (value.same_as(node->value) && body.same_as(node->body)) {
        return node;
    } else {
        return LetStmt::make(node->name, std::move(value), std::move(body));
    }
}

Stmt IRMutator::visit(const IfElse *node) {
    Expr cond = mutate(node->cond);
    Stmt then_body = mutate(node->then_body);
    Stmt else_body = mutate(node->else_body);
    if (cond.same_as(node->cond) &&
        then_body.same_as(node->then_body) &&
        else_body.same_as(node->else_body)) {
        return node;
    } else {
        return IfElse::make(std::move(cond), std::move(then_body), std::move(else_body));
    }
}

Stmt IRMutator::visit(const Sequence *node) {
    auto [stmts, not_changed] = visit_list(this, node->stmts);
    if (not_changed) {
        return node;
    } else {
        return Sequence::make(std::move(stmts));
    }
}

}  // namespace ir
}  // namespace bonsai
