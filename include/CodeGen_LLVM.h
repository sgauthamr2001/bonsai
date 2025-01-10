#pragma once

/** \file
 *
 * Defines the base-class for all architecture-specific code
 * generators that use llvm.
 */

#include <memory>

#include "IR/Frame.h"
#include "IR/Function.h"
#include "IR/Program.h"
#include "IR/Visitor.h"
#include "LLVMIncl.h"
#include "Scope.h"


namespace bonsai {

struct CodeGen_LLVM : public ir::Visitor {
    CodeGen_LLVM();

    // TODO: all entry points must set function!

    // TODO: remove, just for simple testing.
    // void print_expr_function(const ir::Expr &expr);
    // void print_stmt_function(const ir::Stmt &stmt);
    void compile_function(const ir::Function &func);
    void compile_program(const ir::Program &prog);
protected:
    virtual void optimize_module();

    llvm::Value *codegen_expr(const ir::Expr &expr);
    std::vector<llvm::Value *> codegen_exprs(const std::vector<ir::Expr> exprs);
    void codegen_stmt(const ir::Stmt &stmt);
    llvm::Type *codegen_type(const ir::Type &type);
    llvm::Function *codegen_func_ptr(const ir::Expr &expr);
    llvm::Value *codegen_write_loc(const ir::WriteLoc &loc);

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
    virtual void visit(const ir::UInt_t *) override;
    virtual void visit(const ir::Float_t *) override;
    virtual void visit(const ir::Bool_t *) override;
    virtual void visit(const ir::Ptr_t *) override;
    virtual void visit(const ir::Vector_t *) override;
    virtual void visit(const ir::Struct_t *) override;
    virtual void visit(const ir::Tuple_t *) override;
    virtual void visit(const ir::Option_t *) override;
    virtual void visit(const ir::Set_t *) override;
    virtual void visit(const ir::Function_t *) override;
    // Expressions
    virtual void visit(const ir::IntImm *) override;
    virtual void visit(const ir::UIntImm *) override;
    virtual void visit(const ir::FloatImm *) override;
    virtual void visit(const ir::Var *) override;
    virtual void visit(const ir::BinOp *) override;
    virtual void visit(const ir::UnOp *) override;
    virtual void visit(const ir::Select *) override;
    virtual void visit(const ir::Cast *) override;
    virtual void visit(const ir::Broadcast *) override;
    virtual void visit(const ir::VectorReduce *) override;
    virtual void visit(const ir::VectorShuffle *) override;
    virtual void visit(const ir::Ramp *) override;
    virtual void visit(const ir::Extract *) override;
    virtual void visit(const ir::Build *) override;
    virtual void visit(const ir::Access *) override;
    virtual void visit(const ir::Intrinsic *) override;
    virtual void visit(const ir::Lambda *) override;
    virtual void visit(const ir::GeomOp *) override;
    virtual void visit(const ir::SetOp *) override;
    virtual void visit(const ir::Call *) override;
    // Stmts
    virtual void visit(const ir::Return *) override;
    virtual void visit(const ir::Store *) override;
    virtual void visit(const ir::LetStmt *) override;
    virtual void visit(const ir::IfElse *) override;
    // virtual void visit(const ir::Sequence *) override;
    virtual void visit(const ir::Assign *) override;
    virtual void visit(const ir::Accumulate *) override;


    // Local state for codegen() impls.
    llvm::Value *value = nullptr;
    llvm::Type *type = nullptr;
    llvm::Function *function = nullptr;

    // Global LLVM state
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    // Scope<llvm::Value *> scope;
    ir::FrameStack<std::pair<llvm::Value *, bool>> frames;
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
