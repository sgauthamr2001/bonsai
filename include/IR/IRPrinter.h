#pragma once

#include <iostream>
#include <string>

#include "Expr.h"
#include "IRVisitor.h"
#include "Scope.h"

namespace bonsai {
namespace ir {

std::string to_string(const Expr &expr);
std::ostream &operator<<(std::ostream& os, const Expr &expr);

std::string to_string(const Type &type);
std::ostream &operator<<(std::ostream& os, const Type &type);

std::string to_string(const Stmt &stmt);
std::ostream &operator<<(std::ostream& os, const Stmt &stmt);

std::string to_string(const BinOp::OpType &op);
std::string to_string(const VectorReduce::OpType &op);
std::string to_string(const Intrinsic::OpType &op);
std::string to_string(const GeomOp::OpType &op);
std::string to_string(const SetOp::OpType &op);

struct Indentation {
    int indent;
};
std::ostream &operator<<(std::ostream &os, const Indentation &);

struct IRPrinter : public IRVisitor {
    explicit IRPrinter(std::ostream &_os) : os(_os) {}

    void print(const Type &type);
    void print_type_list(const std::vector<Type> &types);
    void print(const Expr &expr);
    void print_no_parens(const Expr &expr);
    void print_expr_list(const std::vector<Expr> &exprs);
    void print(const Stmt &stmt);

    // Types
    void visit(const Int_t *) override;
    void visit(const UInt_t *) override;
    void visit(const Float_t *) override;
    void visit(const Bool_t *) override;
    void visit(const Ptr_t *) override;
    void visit(const Vector_t *) override;
    void visit(const Struct_t *) override;
    void visit(const Tuple_t *) override;
    void visit(const Option_t *) override;
    void visit(const Set_t *) override;
    void visit(const Function_t *) override;
    // Exprs
    void visit(const IntImm *) override;
    void visit(const FloatImm *) override;
    void visit(const Var *) override;
    void print(const BinOp::OpType &op);
    void visit(const BinOp *) override;
    void visit(const Broadcast *) override;
    void print(const VectorReduce::OpType &op);
    void visit(const VectorReduce *) override;
    void visit(const Ramp *) override;
    void visit(const Build *) override;
    void visit(const Access *) override;
    void visit(const Intrinsic *) override;
    void visit(const Lambda *) override;
    void visit(const GeomOp *) override;
    void visit(const SetOp *) override;
    void visit(const Call *) override;
    // Stmts
    void visit(const Return *) override;
    void visit(const Store *) override;
    void visit(const LetStmt *) override;
    void visit(const IfElse *) override;
    void visit(const Sequence *) override;
protected:
    /** The stream on which we're outputting */
    std::ostream &os;

    Indentation get_indent() const {
        return Indentation{indent};
    }

    /** The current indentation level, useful for pretty-printing
     * statements */
    int indent = 0;

    /** Certain expressions do not need parens around them, e.g. the
     * args to a call are already separated by commas and a
     * surrounding set of parens. */
    bool implicit_parens = false;

    /** Either emits "(" or "", depending on the value of implicit_parens */
    void open();

    /** Either emits ")" or "", depending on the value of implicit_parens */
    void close();

    /** The symbols whose types can be inferred from values printed
     * already. */
    Scope<> known_type;
    // TODO: stuff for indenting and whatever for Stmts
};

}  // namespace ir
}  // namespace bonsai
