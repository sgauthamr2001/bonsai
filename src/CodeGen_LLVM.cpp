#include "CodeGen_LLVM.h"

#include <llvm/IR/Constant.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

#include <llvm/MC/TargetRegistry.h>

#include <llvm/Passes/PassBuilder.h>
// #include <llvm/Passes/StandardInstrumentations.h>
// #include <llvm/Support/TargetSelect.h>
// #include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Instrumentation/AddressSanitizer.h>
#include <llvm/Transforms/Instrumentation/SanitizerCoverage.h>
#include <llvm/Transforms/Instrumentation/ThreadSanitizer.h>
// #include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>


#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/TargetParser/Host.h>

#include <llvm/Support/CodeGen.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>

#include "IR/Analysis.h"
#include "IR/Expr.h"
#include "IR/Printer.h"
#include "IR/Stmt.h"
#include "IR/Type.h"

#include "Lower/Intrinsics.h"

#include "Utils.h"

#include <sstream>


namespace bonsai {

using namespace ir;

CodeGen_LLVM::CodeGen_LLVM() {
    // open new context and module
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("bonsai_module", *context);
    // Create a new builder for the module.
    // TODO: there might be params to the IRBuilder...
    builder = std::make_unique<llvm::IRBuilder<>>(*context);

    // TODO: initialize all intended targets?
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // TODO: move to init_context and copy over Halide other stuff.

    // Branch weights for very likely branches
    llvm::MDBuilder md_builder(*context);
    // TODO: use?
    // very_likely_branch = md_builder.createBranchWeights(1 << 30, 0);
    llvm::MDNode *default_fp_math_md = md_builder.createFPMath(0.0);
    // TODO: when do we need strict float math?
    // strict_fp_math_md = md_builder.createFPMath(0.0);
    builder->setDefaultFPMathTag(default_fp_math_md);
    llvm::FastMathFlags fast_flags;
    fast_flags.setNoNaNs();
    fast_flags.setNoInfs();
    fast_flags.setNoSignedZeros();
    // Don't use approximate reciprocals for division. It's too inaccurate even for Halide.
    // fast_flags.setAllowReciprocal();
    // Theoretically, setAllowReassoc could be setUnsafeAlgebra for earlier versions, but that
    // turns on all the flags.
    fast_flags.setAllowReassoc();
    fast_flags.setAllowContract(true);
    fast_flags.setApproxFunc();
    builder->setFastMathFlags(fast_flags);

    // Define some types
    void_t = llvm::Type::getVoidTy(*context);
    i1_t = llvm::Type::getInt1Ty(*context);
    i8_t = llvm::Type::getInt8Ty(*context);
    i16_t = llvm::Type::getInt16Ty(*context);
    i32_t = llvm::Type::getInt32Ty(*context);
    i64_t = llvm::Type::getInt64Ty(*context);
    f16_t = llvm::Type::getHalfTy(*context);
    f32_t = llvm::Type::getFloatTy(*context);
    f64_t = llvm::Type::getDoubleTy(*context);
}

// void CodeGen_LLVM::print_expr_function(const Expr &expr) {
//     // Make function type
//     llvm::Type *ret_type = codegen_type(expr.type());

//     const auto free_vars = gather_free_vars(expr);
//     std::vector<llvm::Type *> arg_types(free_vars.size());
//     for (uint32_t i = 0; i < free_vars.size(); i++) {
//         arg_types[i] = codegen_type(free_vars[i].second);
//     }

//     llvm::FunctionType *ftype = llvm::FunctionType::get(ret_type, arg_types, /* isVarArg */ false);
//     function = llvm::Function::Create(ftype, llvm::GlobalValue::ExternalLinkage, "temp", module.get());

//     // TODO: should scope be cleared here?
//     uint32_t arg_idx = 0;
//     for (auto &arg : function->args()) {
//         arg.setName(free_vars[arg_idx].first);
//         // also add args to scope.
//         scope.push(free_vars[arg_idx].first, &arg);
//         arg_idx++;
//     }

//     // Add entry point.
//     llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(module->getContext(), "entry", function);
//     llvm::IRBuilderBase::InsertPoint here = builder->saveIP();
//     builder->SetInsertPoint(entry_bb);

//     llvm::Value *ret_val = codegen_expr(expr);
//     // Add return statement.
//     builder->CreateRet(ret_val);

//     // Validate the generated code, checking for consistency.
//     verifyFunction(*function);

//     function->dump();

//     this->optimize_module();

//     module->dump();
// }

// void CodeGen_LLVM::print_stmt_function(const Stmt &stmt) {
//     // TODO: this should be called ONCE on a whole Module.
//     auto struct_types = gather_struct_types(stmt);
//     // Must be called before getting ret_type, in case ret_type is struct
//     declare_struct_types(struct_types);

//     // Make function type
//     llvm::Type *ret_type = codegen_type(get_return_type(stmt));

//     const auto free_vars = gather_free_vars(stmt);
//     std::vector<llvm::Type *> arg_types(free_vars.size());
//     for (uint32_t i = 0; i < free_vars.size(); i++) {
//         arg_types[i] = codegen_type(free_vars[i].second);
//     }

//     llvm::FunctionType *ftype = llvm::FunctionType::get(ret_type, arg_types, /* isVarArg */ false);
//     function = llvm::Function::Create(ftype, llvm::GlobalValue::ExternalLinkage, "temp", module.get());

//     // TODO: should scope be cleared here?
//     uint32_t arg_idx = 0;
//     for (auto &arg : function->args()) {
//         arg.setName(free_vars[arg_idx].first);
//         // also add args to scope.
//         scope.push(free_vars[arg_idx].first, &arg);
//         arg_idx++;
//     }

//     // Add entry point.
//     llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(module->getContext(), "entry", function);
//     llvm::IRBuilderBase::InsertPoint here = builder->saveIP();
//     builder->SetInsertPoint(entry_bb);

//     codegen_stmt(stmt);

//     // Validate the generated code, checking for consistency.
//     verifyFunction(*function);

//     function->dump();

//     this->optimize_module();

//     module->dump();
// }

void CodeGen_LLVM::compile_function(const Function &func) {
    // Make function type
    llvm::Type *ret_type = codegen_type(func.ret_type);
    std::vector<llvm::Type *> arg_types(func.args.size());
    for (uint32_t i = 0; i < func.args.size(); i++) {
        arg_types[i] = codegen_type(func.args[i].type);
    }

    llvm::FunctionType *ftype = llvm::FunctionType::get(ret_type, arg_types, /* isVarArg */ false);
    function = llvm::Function::Create(ftype, llvm::GlobalValue::ExternalLinkage, func.name, module.get());

    frames.new_frame();

    uint32_t arg_idx = 0;
    for (auto &arg : function->args()) {
        arg.setName(func.args[arg_idx].name);
        // TODO: allow mutable args? probably not.
        frames.add_to_frame(func.args[arg_idx].name, {&arg, /* mutable */ false});
        arg_idx++;
    }

    // Add entry point.
    llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(module->getContext(), func.name + "_entry", function);
    llvm::IRBuilderBase::InsertPoint here = builder->saveIP();
    builder->SetInsertPoint(entry_bb);
    codegen_stmt(func.body);
    frames.pop_frame();

    // Validate the generated code, checking for consistency.
    verifyFunction(*function);

    function->dump();
}

void CodeGen_LLVM::compile_program(const Program &program) {
    std::vector<const Struct_t *> struct_types;
    for (const auto &[_, t] : program.types) {
        if (t.is<Struct_t>()) {
            struct_types.push_back(t.as<Struct_t>());
        }
    }
    declare_struct_types(struct_types);

    frames.new_frame();

    // TODO: add program.externs to the global frame.

    for (const auto &[_, func] : program.funcs) {
        this->compile_function(func);
    }

    // TODO: now compile main function from program.main_body with arguments defined by program.externs

    frames.pop_frame();

    this->optimize_module();
    module->dump();
}

std::unique_ptr<llvm::TargetMachine> make_target_machine(const llvm::Module &module) {
    std::string error_string;

    std::string targetTriple = llvm::sys::getDefaultTargetTriple();

    const llvm::Target *llvm_target = llvm::TargetRegistry::lookupTarget(targetTriple, error_string);
    if (!llvm_target) {
        std::cout << error_string << "\n";
        llvm::TargetRegistry::printRegisteredTargetsForVersion(llvm::outs());
    }
    auto triple = llvm::Triple(targetTriple);
    internal_assert(llvm_target) << "Could not create LLVM target for " << triple.str();

    llvm::TargetOptions options;

    // TODO: set options?
    options.AllowFPOpFusion =  llvm::FPOpFusion::Fast;
    options.UnsafeFPMath = true;
    options.NoInfsFPMath = true;
    options.NoNaNsFPMath = true;
    // get_target_options(module, options);

    bool use_pic = true;
    // get_md_bool(module.getModuleFlag("bonsai_use_pic"), use_pic);

    bool use_large_code_model = false;
    // get_md_bool(module.getModuleFlag("bonsai_use_large_code_model"), use_large_code_model);

    auto *tm = llvm_target->createTargetMachine(module.getTargetTriple(),
                                                /*CPU target=*/"", /*Features=*/"",
                                                options,
                                                use_pic ? llvm::Reloc::PIC_ : llvm::Reloc::Static,
                                                use_large_code_model ? llvm::CodeModel::Large : llvm::CodeModel::Small,
                                                llvm::CodeGenOptLevel::Aggressive);
    return std::unique_ptr<llvm::TargetMachine>(tm);

}

void CodeGen_LLVM::optimize_module() {
    // Get host target triple.
    std::string target_triple = llvm::sys::getDefaultTargetTriple();

    std::unique_ptr<llvm::TargetMachine> tm = make_target_machine(*module);

    const bool do_loop_opt = true; // get_target().has_feature(Target::EnableLLVMLoopOpt);

    llvm::PipelineTuningOptions pto;
    pto.LoopInterleaving = do_loop_opt;
    pto.LoopVectorization = do_loop_opt;
    pto.SLPVectorization = true;  // Note: SLP vectorization has no analogue in the scheduling model
    pto.LoopUnrolling = do_loop_opt;

    llvm::PassBuilder pb(tm.get(), pto);

    bool debug_pass_manager = false;
    // These analysis managers have to be declared in this order.
    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;
    llvm::FunctionPassManager fpm;

    // TODO: add other explicit passes?
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    fpm.addPass(llvm::InstCombinePass());
    // Reassociate expressions.
    fpm.addPass(llvm::ReassociatePass());
    // Eliminate Common SubExpressions.
    fpm.addPass(llvm::GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    fpm.addPass(llvm::SimplifyCFGPass());

    // Register all the basic analyses with the managers.
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);
    llvm::ModulePassManager mpm;

    using OptimizationLevel = llvm::OptimizationLevel;
    OptimizationLevel level = OptimizationLevel::O3;

    mpm.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(fpm)));


    // get_target().has_feature(Target::SanitizerCoverage)
    if (false) {
        pb.registerOptimizerLastEPCallback(
            [&](llvm::ModulePassManager &mpm, llvm::OptimizationLevel level) {
                llvm::SanitizerCoverageOptions sanitizercoverage_options;
                // Mirror what -fsanitize=fuzzer-no-link would enable.
                // See https://github.com/halide/Halide/issues/6528
                sanitizercoverage_options.CoverageType = llvm::SanitizerCoverageOptions::SCK_Edge;
                sanitizercoverage_options.IndirectCalls = true;
                sanitizercoverage_options.TraceCmp = true;
                sanitizercoverage_options.Inline8bitCounters = true;
                sanitizercoverage_options.PCTable = true;
                // Due to TLS differences, stack depth tracking is only enabled on Linux
                // if (get_target().os == Target::OS::Linux) {
                    // sanitizercoverage_options.StackDepth = true;
                // }
                mpm.addPass(llvm::SanitizerCoveragePass(sanitizercoverage_options));
            });
    }

    // get_target().has_feature(Target::ASAN)
    if (false) {
        // Nothing, ASanGlobalsMetadataAnalysis no longer exists

        pb.registerPipelineStartEPCallback([](llvm::ModulePassManager &mpm, OptimizationLevel) {
            llvm::AddressSanitizerOptions asan_options;  // default values are good...
            asan_options.UseAfterScope = true;     // ...except this one
            constexpr bool use_global_gc = false;
            constexpr bool use_odr_indicator = true;
            constexpr auto destructor_kind = llvm::AsanDtorKind::Global;
            mpm.addPass(llvm::AddressSanitizerPass(
                asan_options, use_global_gc, use_odr_indicator, destructor_kind));
        });
    }

    // Target::MSAN handling is sprinkled throughout the codebase,
    // there is no need to run MemorySanitizerPass here.

    // get_target().has_feature(Target::TSAN)
    if (false) {
        pb.registerOptimizerLastEPCallback(
            [](llvm::ModulePassManager &mpm, OptimizationLevel level) {
                mpm.addPass(
                    llvm::createModuleToFunctionPassAdaptor(llvm::ThreadSanitizerPass()));
            });
    }

    for (auto &function : *module) {
        if (false) { // get_target().has_feature(Target::ASAN)
            function.addFnAttr(llvm::Attribute::SanitizeAddress);
        }
        if (false) { // get_target().has_feature(Target::MSAN)
            function.addFnAttr(llvm::Attribute::SanitizeMemory);
        }
        if (false) { // get_target().has_feature(Target::TSAN)
            // Do not annotate any of Halide's low-level synchronization code as it has
            // tsan interface calls to mark its behavior and is much faster if
            // it is not analyzed instruction by instruction.
            // if (!(function.getName().startswith("_ZN6Halide7Runtime8Internal15Synchronization") ||
            //       // TODO: this is a benign data race that re-initializes the detected features;
            //       // we should really fix it properly inside the implementation, rather than disabling
            //       // it here as a band-aid.
            //       function.getName().startswith("halide_default_can_use_target_features") ||
            //       function.getName().startswith("halide_mutex_") ||
            //       function.getName().startswith("halide_cond_"))) {
            //     function.addFnAttr(llvm::Attribute::SanitizeThread);
            // }
        }
    }

    if (tm) {
        tm->registerPassBuilderCallbacks(pb, /* PopulateClassToPassNames */ true);
    }

    mpm = pb.buildPerModuleDefaultPipeline(level, debug_pass_manager);
    mpm.run(*module, mam);

    internal_assert(!llvm::verifyModule(*module, &llvm::errs())) << "Compilation resulted in an invalid module";
}

void CodeGen_LLVM::visit(const Int_t *node) {
    type = llvm::Type::getIntNTy(*context, node->bits);
}

void CodeGen_LLVM::visit(const UInt_t *node) {
    // LLVM does not distinguish between signed and unsigned integer types.
    type = llvm::Type::getIntNTy(*context, node->bits);
}

void CodeGen_LLVM::visit(const Bool_t *node) {
    type = llvm::Type::getInt1Ty(*context);
}

void CodeGen_LLVM::visit(const Float_t *node) {
    switch (node->bits) {
        // TODO: I need f8 on GPUs.
        // do we ever need it on CPUs?
        case 16:
            // assumes we're not deeling with bfloat
            type = llvm::Type::getHalfTy(*context);
            return;
        case 32:
            type = llvm::Type::getFloatTy(*context);
            return;
        case 64:
            type = llvm::Type::getDoubleTy(*context);
            return;
        default:
            internal_error << "There is no llvm type matching this floating-point bit width: " << Type(node);
    }
}

void CodeGen_LLVM::visit(const Ptr_t *node) {
    llvm::Type *etype = codegen_type(node->etype);
    // TODO: what does the address space parameter to this function do?
    type = etype->getPointerTo();
}

void CodeGen_LLVM::visit(const Vector_t *node) {
    llvm::Type *etype = codegen_type(node->etype);
    internal_assert(!etype->isVoidTy()) << "Cannot make a vector of type void: " << Type(node);
    // TODO: do we ever want to support scalable vectors? probably not.
    type = llvm::VectorType::get(etype, node->lanes, /* Scalable */ false);
}

void CodeGen_LLVM::visit(const Struct_t *node) {
    // TODO: could just use module->getTypeByName
    type = struct_types[node->name];
}

void CodeGen_LLVM::visit(const Tuple_t *node) {
    // TODO: struct_types should include tuples, probably? but they're unnamed...
    // maybe use to_string() to map from node to built Struct_t
    internal_error << "TODO: implement Tuple_t code generation: " << Type(node);
}

void CodeGen_LLVM::visit(const Option_t *node) {
    internal_error << "TODO: implement Option_t code generation: " << Type(node);
}

void CodeGen_LLVM::visit(const Set_t *node) {
    internal_error << "TODO: implement Set_t code generation: " << Type(node);
}

void CodeGen_LLVM::visit(const Function_t *node) {
    internal_error << "TODO: implement Function_t code generation: " << Type(node);
}

void CodeGen_LLVM::visit(const IntImm *node) {
    value = llvm::ConstantInt::getSigned(codegen_type(node->type), node->value);
}

void CodeGen_LLVM::visit(const UIntImm *node) {
    value = llvm::ConstantInt::get(codegen_type(node->type), node->value, /* IsSigned */ false);
}

void CodeGen_LLVM::visit(const FloatImm *node) {
    // TODO: Halide does some weird stuff for f16.
    // Make sure this works on f16?
    value = llvm::ConstantFP::get(codegen_type(node->type), node->value);
}

void CodeGen_LLVM::visit(const Var *node) {
    auto [_value, _mutable] = frames.from_frames(node->name);
    if (_mutable) {
        llvm::Type *_type = codegen_type(node->type);
        value = builder->CreateLoad(_type, _value);
    } else {
        value = _value; // immutable so not pointer.
    }
}

void CodeGen_LLVM::visit(const BinOp *node) {
    // TODO: upgrade type for arithmetic?
    llvm::Value *a = codegen_expr(node->a);
    llvm::Value *b = codegen_expr(node->b);

    // TODO: predications?
    if (node->a.type().is_float()) {
        switch (node->op) {
            case BinOp::Add: {
                value = builder->CreateFAdd(a, b);
                return;
            }
            case BinOp::Mul: {
                value = builder->CreateFMul(a, b);
                return;
            }
            case BinOp::Div: {
                value = builder->CreateFDiv(a, b);
                return;
            }
            case BinOp::Sub: {
                value = builder->CreateFSub(a, b);
                return;
            }
            case BinOp::Le: {
                value = builder->CreateFCmpOLE(a, b);
                return;
            }
            case BinOp::Lt: {
                value = builder->CreateFCmpOLT(a, b);
                return;
            }
            case BinOp::Eq: {
                value = builder->CreateFCmpOEQ(a, b);
                return;
            }
            default:  {
                internal_error << "Unimplemented BinOp lowering for float: " << Expr(node);
            }
        }
    } else if (node->type.is_int()) {
        // TODO: do we ever want NSW?
        switch (node->op) {
            case BinOp::Add: {
                value = builder->CreateAdd(a, b);
                return;
            }
            case BinOp::Mul: {
                value = builder->CreateMul(a, b);
                return;
            }
            case BinOp::Div: {
                // TODO: is this the correct behavior we want?
                value = builder->CreateSDiv(a, b);
                return;
            }
            case BinOp::Sub: {
                value = builder->CreateSub(a, b);
                return;
            }
            case BinOp::Mod: {
                // signed remainder
                value = builder->CreateSRem(a, b);
                return;
            }
            case BinOp::Le: {
                // unsigned comparison
                value = builder->CreateICmpSLE(a, b);
                return;
            }
            case BinOp::Lt: {
                // signed comparison
                value = builder->CreateICmpSLT(a, b);
                return;
            }
            case BinOp::Eq: {
                value = builder->CreateICmpEQ(a, b);
                return;
            }
            default:  {
                internal_error << "Unimplemented BinOp lowering for signed integer: " << Expr(node);
            }
        }
    } else if (node->type.is_uint()) {
        switch (node->op) {
            case BinOp::Add: {
                value = builder->CreateAdd(a, b);
                return;
            }
            case BinOp::Mul: {
                value = builder->CreateMul(a, b);
                return;
            }
            case BinOp::Div: {
                // Use unsigned division for unsigned integers
                value = builder->CreateUDiv(a, b);
                return;
            }
            case BinOp::Sub: {
                value = builder->CreateSub(a, b);
                return;
            }
            case BinOp::Mod: {
                // unsigned remainder
                value = builder->CreateURem(a, b);
                return;
            }
            case BinOp::Le: {
                // Unsigned less-than-or-equal comparison
                value = builder->CreateICmpULE(a, b);
                return;
            }
            case BinOp::Lt: {
                // Unsigned less-than comparison
                value = builder->CreateICmpULT(a, b);
                return;
            }
            case BinOp::Eq: {
                value = builder->CreateICmpEQ(a, b);
                return;
            }
            default: {
                internal_error << "Unimplemented BinOp lowering for unsigned integer: " << Expr(node);
            }
        }
    } else if (node->type.is_bool()) {
        switch (node->op) {
            case BinOp::And: {
                value = builder->CreateAnd(a, b);
                return;
            }
            case BinOp::Or: {
                value = builder->CreateOr(a, b);
                return;
            }
            case BinOp::Xor: {
                value = builder->CreateXor(a, b);
                return;
            }
            default: {
                internal_error << "Unimplemented BinOp lowering for boolean: " << Expr(node);
            }
        }
    }

    internal_error << "Cannot codegen BinOp: " << Expr(node);
}

void CodeGen_LLVM::visit(const UnOp *node) {
    // TODO: upgrade type for arithmetic?
    llvm::Value *a = codegen_expr(node->a);

    switch (node->op) {
        case UnOp::Neg: {
            if (node->type.is_float()) {
                value = builder->CreateFNeg(a);
            } else {
                internal_assert(node->type.is_int() || node->type.is_uint());
                llvm::Type *itype = a->getType();
                llvm::Constant *_0 = llvm::ConstantInt::get(itype, 0);
                value = builder->CreateSub(_0, a);
            }
            return;
        }
        case UnOp::Not: {
            internal_assert(node->type.is_bool());
            value = builder->CreateNot(a);
            return;
        }
    }

    internal_error << "Cannot codegen UnOp: " << Expr(node);
}

void CodeGen_LLVM::visit(const Select *node) {
    llvm::Value *cond = codegen_expr(node->cond);
    llvm::Value *tvalue = codegen_expr(node->tvalue);
    llvm::Value *fvalue = codegen_expr(node->fvalue);
    if (tvalue->getType()->isVectorTy()) {
        // TODO: handle broadcasting!
        internal_assert(cond->getType()->isVectorTy()) << "Select lowering failure: " << ir::Expr(node);
        internal_assert(fvalue->getType()->isVectorTy()) << "Select lowering failure: " << ir::Expr(node);
    }
    // TODO: try Vector Predication Intrinsics!
    // https://llvm.org/docs/LangRef.html#vector-predication-intrinsics
    // https://llvm.org/docs/LangRef.html#llvm-vp-select-intrinsics
    value = builder->CreateSelect(cond, tvalue, fvalue);
}

void CodeGen_LLVM::visit(const Broadcast *node) {
    llvm::Value *v = codegen_expr(node->value);
    value = builder->CreateVectorSplat(node->lanes, v);
}

void CodeGen_LLVM::visit(const VectorReduce *node) {
    internal_assert(node->type.is_scalar()) << "Cannot codegen 2+ dimensional VectorReduce: " << Expr(node);
    // TODO: upgrade type for arithmetic?

    llvm::Value *v = codegen_expr(node->value);

    llvm::VectorType *vecType = llvm::cast<llvm::VectorType>(v->getType());
    llvm::Type *elementType = vecType->getElementType();

    // TODO: better instruction selection.

    // TODO: try fold vector reduce
    llvm::Value *init = nullptr;

    llvm::Intrinsic::IndependentIntrinsics intrin;

    switch (node->op) {
    case VectorReduce::Add:
        if (node->type.is_float()) {
            intrin = llvm::Intrinsic::vector_reduce_fadd;
            // TODO: is this right? why do we have to do this.
            init = llvm::ConstantFP::get(elementType, 0.0f);
        } else {
            intrin = llvm::Intrinsic::vector_reduce_add;
        }
        break;
    case VectorReduce::Mul:
        if (node->type.is_float()) {
            intrin = llvm::Intrinsic::vector_reduce_fmul;
            // TODO: is this right? why do we have to do this.
            init = llvm::ConstantFP::get(elementType, 1.0f);
        } else {
            intrin = llvm::Intrinsic::vector_reduce_mul;
        }
        break;
    case VectorReduce::Min:
        // TODO: handle unsigned eventually!
        // TODO: what is the difference between fmin and fminimum?
        intrin = node->type.is_float() ? llvm::Intrinsic::vector_reduce_fmin : llvm::Intrinsic::vector_reduce_smin;
        break;
    case VectorReduce::Max:
        // TODO: handle unsigned eventually!
        // TODO: what is the difference between fmax and fmaximum?
        intrin = node->type.is_float() ? llvm::Intrinsic::vector_reduce_fmax : llvm::Intrinsic::vector_reduce_smax;
        break;
    case VectorReduce::Idxmax:
        // TODO: on x86 lower to phminposuw
        value = codegen_expr(lower::argmax(node->value));
        return;
    default: {
        internal_error << "Unsupported VectorReduce operation" << Expr(node);
    }
    }

    // TODO: perform splitting? investigate LLVM's splitting.

    if (init) {
        // std::cerr << "Calling with init = " << init << "\n";
        // std::cout << "calling intrinsic for: " << ir::Expr(node) << std::endl;
        value = builder->CreateIntrinsic(elementType, intrin, {init, v});
    } else {
        // std::cout << "calling intrinsic for: " << ir::Expr(node) << std::endl;
        value = builder->CreateIntrinsic(elementType, intrin, {v});
    }

    internal_assert(value) << "VectorReduce intrin failure: " << Expr(node);
}

void CodeGen_LLVM::visit(const VectorShuffle *node) {
    llvm::Value *_value = codegen_expr(node->value);
    llvm::Type *vectorType = _value->getType();
    llvm::Type *out_type = codegen_type(node->type);
    const uint32_t inputSize = node->value.type().lanes();
    const uint32_t outputSize = node->type.lanes();

    llvm::Type *etype = codegen_type(node->type.element_of());

    // TODO: optimize the case for a constant shuffle!

    llvm::Value *result = llvm::UndefValue::get(llvm::VectorType::get(etype, outputSize, /* isScalable */ false));

    // Generate an extract and insert per index.
    for (size_t i = 0; i < node->idxs.size(); i++) {
        const Expr &idx = node->idxs[i];
        // We need 32 bit indices.
        internal_assert(idx.type().is_int() || idx.type().is_uint());
        llvm::Value *load_index = codegen_expr(idx);

        // TODO: we should maybe clamp to [0, inputSize) to avoid UB...

        // TODO: truncs aren't really safe...
        if (idx.type().is_int()) {
            load_index = builder->CreateSExtOrTrunc(load_index, i32_t);
        } else {
            load_index = builder->CreateZExtOrTrunc(load_index, i32_t);
        }

        // llvm::errs() << *_value << " and " << *load_index << "\n";
        llvm::Value *element = builder->CreateExtractElement(_value, load_index);

        llvm::Constant *store_idx = llvm::ConstantInt::get(i32_t, i);
        result = builder->CreateInsertElement(result, element, store_idx);
    }

    value = result;
}

void CodeGen_LLVM::visit(const Ramp *node) {
    internal_error << "TODO: implement Ramp code generation: " << Expr(node);
}

void CodeGen_LLVM::visit(const Extract *node) {
    llvm::Value *vec = codegen_expr(node->vec);
    llvm::Value *idx = codegen_expr(node->idx);
    value = builder->CreateExtractElement(vec, idx);
}

void CodeGen_LLVM::visit(const Intrinsic *node) {
    llvm::Intrinsic::IndependentIntrinsics intrin;
    switch (node->op) {
        case Intrinsic::abs: {
            intrin = node->args[0].type().is_float() ? llvm::Intrinsic::fabs : llvm::Intrinsic::abs;
            break;
        }
        case Intrinsic::cos: {
            intrin = llvm::Intrinsic::cos;
            break;
        }
        case Intrinsic::cross: {
            Expr expr = lower::cross_product(node->args[0], node->args[1]);
            value = codegen_expr(expr);
            return;
        }
        case Intrinsic::max: {
            if (node->args[0].type().is_int()) {
                intrin = llvm::Intrinsic::smax;
            } else if (node->args[0].type().is_uint()) {
                intrin = llvm::Intrinsic::umax;
            } else {
                internal_assert(node->args[0].type().is_float()) << "Cannot lower max of type: " << node->args[0].type();
                // Follows the IEEE-754 semantics for maxNum except for the handling of signaling NaNs. This matches the behavior of libm’s fmax.
                // https://llvm.org/docs/LangRef.html#llvm-maxnum-intrinsic
                // intrin = llvm::Intrinsic::maxnum;
                internal_error << "TODO: figure out fmax codegen: " << Expr(node);
            }
            break;
        }
        case Intrinsic::min: {
            if (node->args[0].type().is_int()) {
                intrin = llvm::Intrinsic::smin;
            } else if (node->args[0].type().is_uint()) {
                intrin = llvm::Intrinsic::umin;
            } else {
                internal_assert(node->args[0].type().is_float()) << "Cannot lower min of type: " << node->args[0].type();
                // Follows the IEEE-754 semantics for minNum, except for handling of signaling NaNs. This match’s the behavior of libm’s fmin.
                // https://llvm.org/docs/LangRef.html#llvm-minnum-intrinsic
                // intrin = llvm::Intrinsic::minnum;
                internal_error << "TODO: figure out fmin codegen: " << Expr(node);
            }
            break;
        }
        case Intrinsic::sin: {
            intrin = llvm::Intrinsic::sin;
            break;
        }
        case Intrinsic::sqrt: {
            intrin = llvm::Intrinsic::sqrt;
            break;
        }
    }
    std::vector<llvm::Value *> args(node->args.size());
    for (size_t i = 0; i < args.size(); i++) {
        args[i] = codegen_expr(node->args[i]);
    }

    llvm::Type *ret_type = codegen_type(node->type);

    // std::cout << "calling intrinsic for: " << ir::Expr(node) << std::endl;
    value = builder->CreateIntrinsic(ret_type, intrin, args);

    internal_assert(value) << "Intrinsic codegen failure: " << Expr(node);
}

void CodeGen_LLVM::visit(const Lambda *node) {
    internal_error << "TODO: implement Lambda code generation: " << Expr(node);
}

void CodeGen_LLVM::visit(const GeomOp *node) {
    internal_error << "TODO: implement GeomOp code generation: " << Expr(node);
}

void CodeGen_LLVM::visit(const SetOp *node) {
    internal_error << "TODO: implement SetOp code generation: " << Expr(node);
}

void CodeGen_LLVM::visit(const Call *node) {
    llvm::Function *func = codegen_func_ptr(node->func);

    // TODO: figure out how to make sure we have the right
    // number of arguments here for better error handling.

    const size_t n_args = node->args.size();
    std::vector<llvm::Value *> args(n_args);

    for (size_t i = 0; i < n_args; i++) {
        args[i] = codegen_expr(node->args[i]);
    }

    value = builder->CreateCall(func, args);
}

void CodeGen_LLVM::visit(const Build *node) {
    // This will be a StructType or a VectorType
    llvm::Type *build_type = codegen_type(node->type);

    std::vector<llvm::Value *> values = codegen_exprs(node->values);

    const bool build_empty = values.empty();

    if (build_empty) {
        value = llvm::Constant::getNullValue(build_type);
        return;
    }

    value = llvm::PoisonValue::get(build_type);

    if (build_type->isVectorTy()) {
        for (size_t i = 0; i < values.size(); i++) {
            value = builder->CreateInsertElement(value, values[i], i);
        }
        return;
    } else if (build_type->isStructTy()) {
        for (size_t i = 0; i < values.size(); i++) {
            value = builder->CreateInsertValue(value, values[i], i);
        }
        return;
    } else {
        internal_error << "Unexpected llvm Type in Build lowering: " << Expr(node);
    }
}

void CodeGen_LLVM::visit(const Access *node) {
    llvm::Value *_struct = codegen_expr(node->value);
    if (_struct->getType()->isStructTy()) {
        const size_t idx = find_struct_index(node->field, node->value.type().as<Struct_t>()->fields);
        value = builder->CreateExtractValue(_struct, idx);
        return;
    }
    internal_error << "Lowering of an Access's value did not result in a struct type: " << Expr(node);
    
}

void CodeGen_LLVM::visit(const Return *node) {
    llvm::Value *ret_val = codegen_expr(node->value);
    // Add return statement.
    builder->CreateRet(ret_val);
}

void CodeGen_LLVM::visit(const Store *node) {
    // TODO: eventually handle atomics...
    // TODO: eventually handle predication...
    llvm::Value *expr = codegen_expr(node->value);
    Type value_type = node->value.type();

    if (value_type.is_scalar()) {
        // Scalar
        llvm::Value *ptr = codegen_buffer_pointer(node->name, value_type, node->index);
        // TODO: handle booleans better.
        llvm::StoreInst *store = builder->CreateAlignedStore(expr, ptr, llvm::Align(value_type.bytes()));
        // TODO: fix this, add type-based metadata to all stores to allow reordering!
        // annotate_store(store, op->index);
        add_tbaa_metadata(store, node->name, node->index);
    } else {
        // TODO: handle alignment info better.
        int alignment = value_type.element_of().bytes();
        const Ramp *ramp = node->index.as<Ramp>();
        bool is_dense = ramp && is_const_one(ramp->stride);

        if (is_dense) {
            // Generate native vector store(s).
            // TODO: do manual splitting into native vector stores.
            llvm::Value *base_ptr = codegen_buffer_pointer(node->name, value_type.element_of(), ramp->base);
            llvm::Value *vec_ptr = builder->CreateBitCast(base_ptr, llvm::PointerType::getUnqual(expr->getType()));
            // TODO: is this always aligned? figure out alignment stuff.
            llvm::StoreInst *store = builder->CreateAlignedStore(expr, vec_ptr, llvm::Align(alignment));
            add_tbaa_metadata(store, node->name, node->index);
        } else if (ramp) {
            // Generate strided stores.
            Type ptr_type = value_type.element_of();
            llvm::Value *ptr = codegen_buffer_pointer(node->name, ptr_type, ramp->base);
            const IntImm *const_stride = ramp->stride.as<IntImm>();
            llvm::Value *stride = codegen_expr(ramp->stride);
            llvm::Type *load_type = codegen_type(ptr_type);
            // Scatter without generating the indices as a vector
            for (int i = 0; i < ramp->lanes; i++) {
                llvm::Constant *lane = llvm::ConstantInt::get(i32_t, i);
                llvm::Value *v = builder->CreateExtractElement(expr, lane);
                if (const_stride) {
                    // Use a constant offset from the base pointer
                    llvm::Value *p =
                        builder->CreateConstInBoundsGEP1_32(
                            load_type, ptr,
                            const_stride->value * i);
                    llvm::StoreInst *store = builder->CreateStore(v, p);
                    // annotate_store(store, op->index);
                    add_tbaa_metadata(store, node->name, node->index);
                } else {
                    // Increment the pointer by the stride for each element
                    llvm::StoreInst *store = builder->CreateStore(v, ptr);
                    // annotate_store(store, op->index);
                    add_tbaa_metadata(store, node->name, node->index);
                    // ptr = CreateInBoundsGEP(builder.get(), load_type, ptr, stride);
                    ptr = builder->CreateInBoundsGEP(load_type, ptr, {stride});
                }
            }
        } else {
            // Generate scatter.
            // TODO: do better on some archs?
            llvm::Value *index = codegen_expr(node->index);
            for (int i = 0; i < value_type.lanes(); i++) {
                llvm::Value *lane = llvm::ConstantInt::get(i32_t, i);
                llvm::Value *idx = builder->CreateExtractElement(index, lane);
                llvm::Value *v = builder->CreateExtractElement(expr, lane);
                llvm::Value *ptr = codegen_buffer_pointer(node->name, value_type.element_of(), idx);
                llvm::StoreInst *store = builder->CreateStore(v, ptr);
                // annotate_store(store, node->index);
                add_tbaa_metadata(store, node->name, node->index);
            }
        }
    }
}

void CodeGen_LLVM::visit(const LetStmt *node) {
    llvm::Value *_value = codegen_expr(node->value);
    frames.add_to_frame(node->loc.base, {_value, /* mutable */ false});
}

void CodeGen_LLVM::visit(const IfElse *node) {
    // internal_error << "TODO: implement codegen for IfElse: " << Stmt(node);

    // Gather the conditions and values in an if-else chain
    struct Block {
        Expr expr;
        Stmt stmt;
        bool returns;
        Block(Expr _expr, Stmt _stmt, bool _returns)
            : expr(std::move(_expr)), stmt(std::move(_stmt)), returns(_returns) {}
    };
    std::vector<Block> blocks;
    Stmt final_else;
    const IfElse *next_if = node;
    bool needs_after_bb = false;
    do {
        bool returns = always_returns(next_if->then_body);
        blocks.emplace_back(next_if->cond,
                            next_if->then_body,
                            returns);
        needs_after_bb = needs_after_bb || !returns;
        final_else = next_if->else_body;
        next_if = final_else.defined() ? final_else.as<IfElse>() : nullptr;
    } while (next_if);

    needs_after_bb = needs_after_bb && (!final_else.defined() || !always_returns(final_else));

    // TODO: we will support a switch statement, make sure to use Halide's codegen for it!

    llvm::BasicBlock *after_bb = needs_after_bb ? llvm::BasicBlock::Create(*context, "after_bb", function) : nullptr;

    for (const auto &p : blocks) {
        llvm::BasicBlock *then_bb = llvm::BasicBlock::Create(*context, "then_bb", function);
        llvm::BasicBlock *next_bb = llvm::BasicBlock::Create(*context, "next_bb", function);
        builder->CreateCondBr(codegen_expr(p.expr), then_bb, next_bb);
        builder->SetInsertPoint(then_bb);
        codegen_stmt(p.stmt);
        if (!p.returns) {
            builder->CreateBr(after_bb);
        }
        builder->SetInsertPoint(next_bb); 
    }

    if (final_else.defined()) {
        codegen_stmt(final_else);
    }

    if (needs_after_bb) {
        builder->CreateBr(after_bb);
        builder->SetInsertPoint(after_bb);
    }
}

// default behavior is fine.
// void CodeGen_LLVM::visit(const Sequence *node) {
//     internal_error << "TODO: implement codegen for Sequence!";
// }

void CodeGen_LLVM::visit(const Assign *node) {
    // internal_error << "TODO: implement codegen for assign: " << Stmt(node);

    llvm::Value *loc = codegen_write_loc(node->loc);
    internal_assert(loc) << "Failed to codegen LLVM ptr for: " << node->loc << " in assignment: " << ir::Stmt(node);

    llvm::Value *_value = codegen_expr(node->value);

    // TODO: handle node->mutating?

    llvm::Type *value_type = _value->getType();

    // internal_assert(value->getType()->getPointerElementType()->isSameType(value_type)) << "Type mismatch between WriteLoc and Expr in Assign" << Stmt(node);

    // llvm::errs() << "trying to store: " << *_value << " in " << *loc << "\n";
    builder->CreateStore(_value, loc, /* isVolatile */ false); // TODO: when is isVolatile true?
}

void CodeGen_LLVM::visit(const Accumulate *node) {
    llvm::Value *loc = codegen_write_loc(node->loc);

    llvm::Value *_value = codegen_expr(node->value);

    llvm::Value *current = builder->CreateLoad(_value->getType(), loc);
    llvm::Value *acc = nullptr;

    switch (node->op) {
        case Accumulate::Add: {
            if (node->value.type().is_float()) {
                acc = builder->CreateFAdd(current, _value);
            } else {
                acc = builder->CreateAdd(current, _value);
            }
            break;
        }
        case Accumulate::Mul: {
            if (node->value.type().is_float()) {
                acc = builder->CreateFMul(current, _value);
            } else {
                acc = builder->CreateMul(current, _value);
            }
            break;
        }
        default: {
            internal_error << "TODO: implement codegen for accumulate: " << Stmt(node);
        }
    }

    builder->CreateStore(acc, loc);
}

void CodeGen_LLVM::add_tbaa_metadata(llvm::Instruction *inst, const std::string &buffer, const Expr &index) {

    // Get the unique name for the block of memory this allocate node
    // is using.
    const std::string alloc_name = get_allocation_name(buffer);

    // If the index is constant, we generate some TBAA info that helps
    // LLVM understand our loads/stores aren't aliased.
    bool constant_index = false;
    int64_t base = 0;
    int64_t width = 1;

    if (index.defined()) {
        if (const Ramp *ramp = index.as<Ramp>()) {
            const int64_t *pstride = as_const_int(ramp->stride);
            const int64_t *pbase = as_const_int(ramp->base);
            if (pstride && pbase) {
                // We want to find the smallest aligned width and offset
                // that contains this ramp.
                int64_t stride = *pstride;
                base = *pbase;
                // base = 0
                internal_assert(base >= 0) << "base of ramp is negative";
                width = next_power_of_two(ramp->lanes * stride);

                while (base % width) {
                    base -= base % width;
                    width *= 2;
                }
                constant_index = true;
            }
        } else {
            const int64_t *pbase = as_const_int(index);
            if (pbase) {
                base = *pbase;
                constant_index = true;
            }
        }
    } else {
        // Index is implied 0
        constant_index = true;
        base = 0;
    }

    llvm::MDBuilder builder(*context);

    // Add type-based-alias-analysis metadata to the pointer, so that
    // loads and stores to different buffers can get reordered.
    llvm::MDNode *tbaa = builder.createTBAARoot("Bonsai buffer");

    tbaa = builder.createTBAAScalarTypeNode(alloc_name, tbaa);

    // We also add metadata for constant indices to allow loads and
    // stores to the same buffer to get reordered.
    // if (constant_index) {
    // TODO: is this necessary if scalar
    //     for (int w = 1024; w >= width; w /= 2) {
    //         int64_t b = (base / w) * w;

    //         std::stringstream level;
    //         level << buffer << ".width" << w << ".base" << b;
    //         tbaa = builder.createTBAAScalarTypeNode(level.str(), tbaa);
    //     }
    // }

    tbaa = builder.createTBAAStructTagNode(tbaa, tbaa, 0);

    inst->setMetadata("tbaa", tbaa);
}

void CodeGen_LLVM::declare_struct_types(const std::vector<const Struct_t *> structs) {
    internal_assert(struct_types.empty()) << "declare_struct_types called with non-empty struct_types!";

    // TODO: does this handle recursive types properly?
    // First insert empty StructTypes into struct_types, to handle
    // weird ordering on types.
    // TODO: maybe make sure there's never an infinitely-recursive type?
    for (const auto& _struct : structs) {
        struct_types[_struct->name] = llvm::StructType::create(*context, _struct->name);
    }
    // Now build bodies, possibly referencing other struct types.
    for (const auto& _struct : structs) {
        std::vector<llvm::Type *> types(_struct->fields.size());
        size_t i = 0;
        for (const auto& [key, value] : _struct->fields) {
            types[i++] = codegen_type(value);
        }
        struct_types[_struct->name]->setBody(types);
    }

}


llvm::Value *CodeGen_LLVM::codegen_buffer_pointer(const std::string &buffer, const Type &type, llvm::Value *idx) {
    llvm::DataLayout d(module.get());
    auto [base_addr, _] = frames.from_frames(buffer);
    // llvm::Value *base_addr = frames.from_frames(buffer);

    // TODO: upgrade type for storage?
    llvm::Type *load_type = codegen_type(type);
    unsigned address_space = base_addr->getType()->getPointerAddressSpace();
    llvm::Type *pointer_load_type = load_type->getPointerTo(address_space);
    
    // TODO: This can likely be removed once opaque pointers are default
    // in all supported LLVM versions.
    base_addr = builder->CreatePointerCast(base_addr, pointer_load_type);

    // TODO: support Halide's nice optimizations here.
    if (idx == nullptr) {
        return base_addr;
    }

    llvm::Constant *constant_index = llvm::dyn_cast<llvm::Constant>(idx);
    if (constant_index && constant_index->isZeroValue()) {
        return base_addr;
    }

    // Promote index to 64-bit on targets that use 64-bit pointers.
    if (d.getPointerSize() == 8) {
        llvm::Type *index_type = idx->getType();
        llvm::Type *desired_index_type = llvm::Type::getInt64Ty(*context); // TODO: cache this like Halide does.
        if (llvm::isa<llvm::VectorType>(index_type)) {
            desired_index_type = llvm::VectorType::get(desired_index_type,
                                                 llvm::dyn_cast<llvm::VectorType>(index_type)->getElementCount());
        }
        // TODO: is isSigned always true for us?
        idx = builder->CreateIntCast(idx, desired_index_type, /* isSigned */ true);
    }

    return builder->CreateInBoundsGEP(load_type, base_addr, idx);
}

llvm::Value *CodeGen_LLVM::codegen_buffer_pointer(const std::string &buffer, const Type &type, const Expr &idx) {
    llvm::Value *offset = idx.defined() ? codegen_expr(idx) : nullptr;
    return codegen_buffer_pointer(buffer, type, offset);
}

llvm::Value *CodeGen_LLVM::codegen_expr(const Expr &e) {
    internal_assert(e.defined());
    value = nullptr;
    e.accept(this);
    internal_assert(value) << "Failed to codegen expression: " << e;
    return value;
}

std::vector<llvm::Value *> CodeGen_LLVM::codegen_exprs(const std::vector<ir::Expr> exprs) {
    std::vector<llvm::Value *> values(exprs.size());
    for (size_t i = 0; i < exprs.size(); i++) {
        values[i] = codegen_expr(exprs[i]);
    }
    return values;
}

void CodeGen_LLVM::codegen_stmt(const Stmt &s) {
    internal_assert(s.defined());
    s.accept(this);
}

llvm::Type *CodeGen_LLVM::codegen_type(const Type &t) {
    internal_assert(t.defined());
    type = nullptr;
    t.accept(this);
    internal_assert(type) << "Failed to codegen type: " << t;
    return type;
}

llvm::Function *CodeGen_LLVM::codegen_func_ptr(const Expr &expr) {
    if (expr.is<Var>()) {
        return module->getFunction(expr.as<Var>()->name);
    }
    internal_error << "TODO: cannot codegen function pointer from: " << expr;
    return nullptr;
}

llvm::Value *CodeGen_LLVM::codegen_write_loc(const ir::WriteLoc &loc) {
    llvm::Value *base = nullptr;
    if (frames.name_in_scope(loc.base)) {
        auto [_base, _mutable] = frames.from_frames(loc.base);
        internal_assert(_mutable) << "Attempting to codegen write to immutable data: " << loc.base;
        base = _base;
    } else {
        // Create alloc of base type
        llvm::Type *base_type = codegen_type(loc.base_type);

        base = builder->CreateAlloca(base_type, /* arraysize ? */ nullptr, loc.base);
        frames.add_to_frame(loc.base, {base, /* mutable */ true});
    }
    llvm::Value *ptr = base;
    llvm::Type *ptype = codegen_type(loc.base_type);
    std::string name = loc.base;

    for (const auto &value : loc.accesses) {
        if (std::holds_alternative<std::string>(value)) {
            internal_error << "TODO: implement field write access: " << std::get<std::string>(value) << " in loc: " << loc;
        } else {
            Expr idx = std::get<Expr>(value);
            llvm::Value *_idx = codegen_expr(idx);
            name += "_idx";
            ptr = builder->CreateGEP(ptype, ptr, {_idx}, name);
            // TODO: update ptype?
        }
    }
    // llvm::errs() << *base << "\n";
    // llvm::errs() << *ptr << "\n";
    // internal_assert(loc.accesses.empty()) << "TODO: implement codegen writeloc for accesses: " << loc;
    return ptr;
}


} //  namespace bonsai
