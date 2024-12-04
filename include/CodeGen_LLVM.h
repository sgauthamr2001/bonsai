#pragma once

/** \file
 *
 * Defines the base-class for all architecture-specific code
 * generators that use llvm.
 */

#include <memory>

#include "IRVisitor.h"
#include "LLVMIncl.h"
#include "Scope.h"


namespace bonsai {

struct CodeGen_LLVM : public IRVisitor {
    CodeGen_LLVM();

    // TODO: all entry points must set function!

    // TODO: remove, just for simple testing.
    void print_expr_function(const Expr &expr);
    void print_stmt_function(const Stmt &stmt);
protected:
    virtual void optimize_module();

    llvm::Value *codegen_expr(const Expr &expr);
    void codegen_stmt(const Stmt &stmt);
    llvm::Type *codegen_type(const Type &type);

    llvm::Value *codegen_buffer_pointer(const std::string &buffer, const Type &type, const Expr &idx);
    void add_tbaa_metadata(llvm::Instruction *inst, const std::string &buffer, const Expr &index);

    /** Get a unique name for the actual block of memory that an
     * allocate node uses. Used so that alias analysis understands
     * when multiple Allocate nodes shared the same memory. */
    virtual std::string get_allocation_name(const std::string &n) {
        return n;
    }

    // Types
    virtual void visit(const Int_t *) override;
    virtual void visit(const Float_t *) override;
    virtual void visit(const Bool_t *) override;
    virtual void visit(const Ptr_t *) override;
    virtual void visit(const Vector_t *) override;
    virtual void visit(const Struct_t *) override;
    // Expressions
    virtual void visit(const IntImm *) override;
    virtual void visit(const FloatImm *) override;
    virtual void visit(const Var *) override;
    virtual void visit(const BinOp *) override;
    virtual void visit(const Broadcast *) override;
    virtual void visit(const VectorReduce *) override;
    // Stmts
    virtual void visit(const Return *) override;
    virtual void visit(const Store *) override;
    virtual void visit(const LetStmt *) override;
    virtual void visit(const IfElse *) override;
    // default behavior is good enough
    // virtual void visit(const Sequence *) override;


    // Local state for codegen() impls.
    llvm::Value *value = nullptr;
    llvm::Type *type = nullptr;
    llvm::Function *function = nullptr;

    // Global LLVM state
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    Scope<llvm::Value *> scope;

    /** Some useful llvm types */
    // @{
    llvm::Type *void_t, *i1_t, *i8_t, *i16_t, *i32_t, *i64_t, *f16_t, *f32_t, *f64_t;
    // llvm::StructType *halide_buffer_t_type,
    //     *type_t_type,
    //     *dimension_t_type,
    //     *metadata_t_type,
    //     *argument_t_type,
    //     *scalar_value_t_type,
    //     *device_interface_t_type,
    //     *pseudostack_slot_t_type,
    //     *semaphore_t_type;

    // @}
};


} //  namespace bonsai
