#pragma once

namespace bonsai {

// Types
struct Type;
struct Int_t;
struct Float_t;
struct Bool_t;
struct Ptr_t;
struct Vector_t;
struct Struct_t;

// Exprs
struct Expr;
struct IntImm;
struct FloatImm;
struct Var;
struct BinOp;
struct Broadcast;
struct VectorReduce;
struct Ramp;

// Stmts
struct Stmt;
struct Return;
struct Store;
struct LetStmt;
struct IfElse;
struct Sequence;

}  // namespace bonsai
