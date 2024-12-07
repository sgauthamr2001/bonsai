#include "IR/Stmt.h"

#include "IR/IRPrinter.h"

namespace bonsai {
namespace ir {

Stmt Return::make(Expr value) {
    if (!value.defined()) {
        throw std::runtime_error("Undefined value in Return::make");
    }
    Return *node = new Return;
    node->value = std::move(value);
    return node;
}

Stmt Store::make(std::string name, Expr index, Expr value) {
    if (name.empty()) {
        throw std::runtime_error("Empty name in Store::make");
    }
    if (!value.defined()) {
        throw std::runtime_error("Undefined value in Store::make");
    }
    Store *node = new Store;
    node->name = std::move(name);
    node->index = std::move(index);
    node->value = std::move(value);
    return node;
}

Stmt LetStmt::make(std::string name, Expr value, Stmt body) {
    if (name.empty()) {
        throw std::runtime_error("Empty name in LetStmt::make");
    }
    if (!value.defined()) {
        throw std::runtime_error("Undefined value in LetStmt::make");
    }
    if (!body.defined()) {
        throw std::runtime_error("Undefined body in LetStmt::make");
    }
    LetStmt *node = new LetStmt;
    node->name = std::move(name);
    node->value = std::move(value);
    node->body = std::move(body);
    return node;
}

Stmt IfElse::make(Expr cond, Stmt then_body, Stmt else_body) {
    if (!cond.defined()) {
        throw std::runtime_error("Undefined condition in IfElse::make");
    }
    if (!cond.type().is_bool()) {
        throw std::runtime_error("Non-boolean condition in IfElse::make: " + to_string(cond));
    }
    if (!then_body.defined()) {
        throw std::runtime_error("Undefined then_body in IfElse::make");
    }
    IfElse *node = new IfElse;
    node->cond = std::move(cond);
    node->then_body = std::move(then_body);
    node->else_body = std::move(else_body);
    return node;
}

Stmt Sequence::make(std::vector<Stmt> stmts) {
    if (stmts.size() == 0) {
        throw std::runtime_error("Empty stmts in Sequence::make");
    }
    for (const auto &s : stmts) {
        if (!s.defined()) {
            throw std::runtime_error("Undefined stmt in Sequence::make");
        }
    }
    Sequence *node = new Sequence;
    node->stmts = std::move(stmts);
    return node;
}

}  // namespace ir
}  // namespace bonsai
