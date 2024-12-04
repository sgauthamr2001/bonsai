#include "IRPrinter.h"

#include <vector>
#include <sstream>

#include "Expr.h"
#include "Stmt.h"
#include "Type.h"

namespace bonsai {

std::string to_string(const Expr &expr) {
    std::ostringstream oss;
    oss << expr;
    return oss.str();
}

std::ostream &operator<<(std::ostream& os, const Expr &expr) {
    if (expr.defined()) {
        IRPrinter printer(os);
        printer.print(expr);
    } else {
        os << "(undef-expr)";
    }
    return os;
}

std::string to_string(const Type &type) {
    std::ostringstream oss;
    oss << type;
    return oss.str();
}

std::ostream &operator<<(std::ostream& os, const Type &type) {
    if (type.defined()) {
        IRPrinter printer(os);
        printer.print(type);
    } else {
        os << "(undef-type)";
    }
    return os;
}

std::string to_string(const Stmt &stmt) {
    std::ostringstream oss;
    oss << stmt;
    return oss.str();
}

std::ostream &operator<<(std::ostream& os, const Stmt &stmt) {
    if (stmt.defined()) {
        IRPrinter printer(os);
        printer.print(stmt);
    } else {
        os << "(undef-stmt)";
    }
    return os;
}

std::ostream &operator<<(std::ostream &stream, const Indentation &indentation) {
    for (int i = 0; i < indentation.indent; i++) {
        stream << " ";
    }
    return stream;
}


void IRPrinter::print(const Type &type) {
    type->accept(this);
}

void IRPrinter::print(const Expr &expr) {
    // ScopedValue<bool> old(implicit_parens, false);
    bool temp = implicit_parens;
    implicit_parens = false;
    expr.accept(this);
    implicit_parens = temp;
}

void IRPrinter::print_no_parens(const Expr &expr) {
    ScopedValue<bool> old(implicit_parens, true);
    expr.accept(this);
}

void IRPrinter::print_expr_list(const std::vector<Expr> &exprs) {
    for (size_t i = 0; i < exprs.size(); i++) {
        print_no_parens(exprs[i]);
        if (i < exprs.size() - 1) {
            os << ", ";
        }
    }
}

void IRPrinter::print(const Stmt &stmt) {
    stmt->accept(this);
}

void IRPrinter::visit(const Int_t *node) {
    os << "i" << node->bits;
}

void IRPrinter::visit(const Float_t *node) {
    os << "f" << node->bits;
}

void IRPrinter::visit(const Bool_t *node) {
    os << "bool";
}

void IRPrinter::visit(const Ptr_t *node) {
    os << "(";
    print(node->etype);
    os << "*)";
}

void IRPrinter::visit(const Vector_t *node) {
    print(node->etype);
    os << "x" << node->lanes;
}

void IRPrinter::visit(const Struct_t *node) {
    os << "struct " << node->name << "{ ";
    // TODO: intended verbosity?
    // TODO: lift to a print_map function?
    bool first = true;
    for (const auto& [key, value] : node->fields) {
        if (!first) {
            os << "; ";
        }
        first = false;
        // TODO: flip? if easier to read.
        os << key << " : ";
        print(value);
    }
    os << " }";
}


void IRPrinter::visit(const IntImm *node) {
    os << "(";
    print(node->type);
    os << ")";
    os << node->value;
}


void IRPrinter::visit(const FloatImm *node) {
    switch (node->type.bits()) {
    case 64:
        os << node->value;
        break;
    case 32:
        os << node->value << "f";
        break;
    case 16:
        os << node->value << "h";
        break;
    default:
        throw std::runtime_error("Bad bit-width for float");
        // internal_error << "Bad bit-width for float: " << op->type << "\n";
    }
}

void IRPrinter::visit(const Var *node) {
    if (!known_type.contains(node->name)) {
        os << "(";
        print(node->type);
        os << ")";
    }
    os << node->name;
}

void IRPrinter::open() {
    if (!implicit_parens) {
        os << "(";
    }
}

void IRPrinter::close() {
    if (!implicit_parens) {
        os << ")";
    }
}

std::string to_string(const BinOp::OpType &op) {
    switch (op) {
        case BinOp::Add: return "+";
        case BinOp::Mul: return "*";
        case BinOp::Div: return "/";
        case BinOp::Sub: return "-";
        case BinOp::Eq: return "==";
        case BinOp::Le: return "<=";
        case BinOp::Lt: return "<";
    }
}

void IRPrinter::visit(const BinOp *node) {
    open();
    print(node->a);
    os << " ";
    // TODO: handle min/max/etc.
    os << to_string(node->op);
    os << " ";
    print(node->b);
    close();
}

void IRPrinter::visit(const Broadcast *node) {
    os << "x" << node->lanes << "(";
    print_no_parens(node->value);
    os << ")";
}

std::string to_string(const VectorReduce::OpType &op) {
    switch (op) {
        case VectorReduce::Add: return "+";
        case VectorReduce::Mul: return "*";
        case VectorReduce::Min: return "min";
        case VectorReduce::Max: return "max";
    }
}

void IRPrinter::visit(const VectorReduce *node) {
    // TODO: print type?
    os << "reduce<" << to_string(node->op) << ">(";
    print_no_parens(node->value);
    os << ")";
}

void IRPrinter::visit(const Ramp *node) {
    // TODO: print type?
    os << "ramp(";
    print_no_parens(node->base);
    os << ", ";
    print_no_parens(node->stride);
    os << ", " << node->lanes << ")";
}


void IRPrinter::visit(const Return *node) {
    os << get_indent();
    os << "return ";
    print_no_parens(node->value);
    os << "\n";
}

void IRPrinter::visit(const Store *node) {
    os << get_indent();
    os << node->name << "[";
    if (node->index.defined()) {
        print_no_parens(node->index);
    }
    os << "] = ";
    print_no_parens(node->value);
    os << "\n";
}

void IRPrinter::visit(const LetStmt *node) {
    ScopedBinding<> bind(known_type, node->name);
    os << get_indent() << "let " << node->name << " = ";
    print_no_parens(node->value);
    os << " in\n";
    print(node->body);
}

void IRPrinter::visit(const IfElse *node) {
    os << get_indent();
    while (true) {
        os << "if (";
        print_no_parens(node->cond);
        os << ") {\n";
        indent++;
        print(node->then_body);
        indent--;

        if (!node->else_body.defined()) {
            break;
        }

        if (const IfElse *nested_if = node->else_body.as<IfElse>()) {
            os << get_indent() << "} else ";
            node = nested_if;
        } else {
            os << get_indent() << "} else {\n";
            indent++;
            print(node->else_body);
            indent--;
            break;
        }
    }

    os << get_indent() << "}\n";
}

void IRPrinter::visit(const Sequence *node) {
    for (const auto &stmt : node->stmts) {
        stmt.accept(this);
    }
}

}  // namespace bonsai
