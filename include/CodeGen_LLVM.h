#pragma once

/** \file
 *
 * Defines the base-class for all architecture-specific code
 * generators that use llvm.
 */

#include <memory>

#include "IR/IRVisitor.h"
#include "LLVMIncl.h"
#include "Scope.h"


namespace bonsai {

struct CodeGen_LLVM : public ir::IRVisitor {
    CodeGen_LLVM();

    // TODO: all entry points must set function!

    // TODO: remove, just for simple testing.
    void print_expr_function(const ir::Expr &expr);
    void print_stmt_function(const ir::Stmt &stmt);
protected:
    virtual void optimize_module();

    llvm::Value *codegen_expr(const ir::Expr &expr);
    void codegen_stmt(const ir::Stmt &stmt);
    llvm::Type *codegen_type(const ir::Type &type);

    llvm::Value *codegen_buffer_pointer(const std::string &buffer, const ir::Type &type, const ir::Expr &idx);
    llvm::Value *codegen_buffer_pointer(const std::string &buffer, const ir::Type &type, llvm::Value *idx);
    void add_tbaa_metadata(llvm::Instruction *inst, const std::string &buffer, const ir::Expr &index);

    void declare_struct_types(const std::vector<const ir::Struct_t *> structs);

    /** Get a unique name for the actual block of memory that an
     * allocate node uses. Used so that alias analysis understands
     * when multiple Allocate nodes shared the same memory. */
    virtual std::string get_allocation_name(const std::string &n) {
        return n;
    }

    // Types
    virtual void visit(const ir::Int_t *) override;
    virtual void visit(const ir::Float_t *) override;
    virtual void visit(const ir::Bool_t *) override;
    virtual void visit(const ir::Ptr_t *) override;
    virtual void visit(const ir::Vector_t *) override;
    virtual void visit(const ir::Struct_t *) override;
    // Expressions
    virtual void visit(const ir::IntImm *) override;
    virtual void visit(const ir::FloatImm *) override;
    virtual void visit(const ir::Var *) override;
    virtual void visit(const ir::BinOp *) override;
    virtual void visit(const ir::Broadcast *) override;
    virtual void visit(const ir::VectorReduce *) override;
    virtual void visit(const ir::Ramp *) override;
    virtual void visit(const ir::Build *) override;
    virtual void visit(const ir::Access *) override;
    // Stmts
    virtual void visit(const ir::Return *) override;
    virtual void visit(const ir::Store *) override;
    virtual void visit(const ir::LetStmt *) override;
    virtual void visit(const ir::IfElse *) override;
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
    std::map<std::string, llvm::StructType*> struct_types;

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
