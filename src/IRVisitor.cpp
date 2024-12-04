#include "IRVisitor.h"

#include "Expr.h"
#include "Stmt.h"
#include "Type.h"

namespace bonsai {

namespace {

template<typename T>
void visit_list(IRVisitor *v, const std::vector<T> nodes) {
    for (const auto & node : nodes) {
        node.accept(v);
    }
}

template<typename T>
void visit_map(IRVisitor *v, const std::map<std::string, T> fields) {
    for (const auto& [key, value] : fields) {
        value.accept(v);
    }
}


}

void IRVisitor::visit(const Int_t *) {
}

void IRVisitor::visit(const Float_t *) {
}

void IRVisitor::visit(const Bool_t *) {
}

void IRVisitor::visit(const Ptr_t *node) {
    node->etype.accept(this);
}

void IRVisitor::visit(const Vector_t *node) {
    node->etype.accept(this);
}

void IRVisitor::visit(const Struct_t *node) {
    visit_map(this, node->fields);
}


void IRVisitor::visit(const IntImm *) {
}

void IRVisitor::visit(const FloatImm *) {
}

void IRVisitor::visit(const Var *) {
}

void IRVisitor::visit(const BinOp *node) {
    node->a.accept(this);
    node->b.accept(this);
}

void IRVisitor::visit(const Broadcast *node) {
    node->value.accept(this);
}

void IRVisitor::visit(const VectorReduce *node) {
    node->value.accept(this);
}

void IRVisitor::visit(const Ramp *node) {
    node->base.accept(this);
    node->stride.accept(this);
}

void IRVisitor::visit(const Build *node) {
    visit_list(this, node->values);
}

void IRVisitor::visit(const Access *node) {
    node->value.accept(this);
}


void IRVisitor::visit(const Return *node) {
    node->value.accept(this);
}

void IRVisitor::visit(const Store *node) {
    if (node->index.defined()) {
        node->index.accept(this);
    }
    node->value.accept(this);
}

void IRVisitor::visit(const LetStmt *node) {
    node->value.accept(this);
    node->body.accept(this);
}

void IRVisitor::visit(const IfElse *node) {
    node->cond.accept(this);
    node->then_body.accept(this);
    if (node->else_body.defined()) {
        node->else_body.accept(this);
    }
}

void IRVisitor::visit(const Sequence *node) {
    visit_list(this, node->stmts);
}

} // namespace bonsai
