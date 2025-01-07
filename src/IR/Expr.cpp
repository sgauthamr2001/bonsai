#include "IR/Expr.h"

#include <iostream>
#include <numeric>
#include <stdexcept>
#include <utility>

#include "IR/Float16.h"
#include "IR/IREquality.h"
#include "IR/IRPrinter.h"

#include "IR/TypeEnforcement.h"

#include "Error.h"
#include "Utils.h"

namespace bonsai {
namespace ir {

Expr::Expr(int8_t x)
    : IRHandle(IntImm::make(Int_t::make(8), x)) {
}

Expr::Expr(int16_t x)
    : IRHandle(IntImm::make(Int_t::make(16), x)) {
}

Expr::Expr(int32_t x)
    : IRHandle(IntImm::make(Int_t::make(32), x)) {
}

Expr::Expr(int64_t x)
    : IRHandle(IntImm::make(Int_t::make(64), x)) {
}

Expr IntImm::make(Type t, int64_t value) {
    const bool infer_types = type_enforcement_enabled() || t.defined();

    if (infer_types) {
        internal_assert(t.defined() && t.is_int() && t.is_scalar())
            << "IntImm::make passed bad type for value: " << value << " type: " << t;

        // Normalize the value by dropping the high bits.
        // Since left-shift of negative value is UB in C++, cast to uint64 first;
        // it's unlikely any compilers we care about will misbehave, but UBSan will complain.
        value = (int64_t)(((uint64_t)value) << (64 - t.bits()));
        // Then sign-extending to get them back
        value >>= (64 - t.bits());
    }

    IntImm *node = new IntImm;
    node->type = t;
    node->value = value;
    return node;
}

Expr UIntImm::make(Type t, uint64_t value) {
    const bool infer_types = type_enforcement_enabled() || t.defined();

    if (infer_types) {
        internal_assert(t.defined() && t.is_uint() && t.is_scalar())
            << "UIntImm::make passed bad type for value: " << value << " type: " << t << "\n"
            << t.defined() << " && " << t.is_uint() << " && " << t.is_scalar(); 
        internal_assert(t.bits() >= 1 && t.bits() <= 64)
            << "UIntImm must have between 1 and 64 bits\n";

        // Normalize the value by dropping the high bits
        value <<= (64 - t.bits());
        value >>= (64 - t.bits());
    }

    UIntImm *node = new UIntImm;
    node->type = t;
    node->value = value;
    return node;
}


Expr FloatImm::make(Type t, double value) {
    // internal_assert(t.is_float() && t.is_scalar())
    //     << "FloatImm must be a scalar Float\n";
    
    const bool infer_types = type_enforcement_enabled() || t.defined();

    if (infer_types) {
        internal_assert(t.defined()) << "FloatImm::make passed undefined type for value: " << value;
        switch (t.bits()) {
        case 16:
            value = cast_to_float16(value);
            break;
        case 32:
            value = (float)value;
            break;
        case 64:
            value = value;
            break;
        default:
            internal_error << "FloatImm must be f16, f32, or f64, instead received: " << t;
        }
    }

    FloatImm *node = new FloatImm;
    node->type = t;
    node->value = value;
    return node;
}

Expr Var::make(Type type, const std::string &name) {
    internal_assert(!name.empty()) << "Var::make called with empty name and type: " << type;
    internal_assert(!type_enforcement_enabled() || type.defined()) << "Var::make called with undefined type for name: " << name;
    Var *node = new Var;
    node->type = type;
    node->name = name;
    return node;
}

bool BinOp::is_numeric_op(const BinOp::OpType &op) {
    switch (op) {
        // Technically, And, Or, and Xor keep the type of the operands.
        // maybe need to rename this function.
        case BinOp::And:
        case BinOp::Or:
        case BinOp::Xor:
        case BinOp::Add:
        case BinOp::Mod:
        case BinOp::Mul:
        case BinOp::Div:
        case BinOp::Sub: return true;
        case BinOp::Neq:
        case BinOp::Eq:
        case BinOp::Le:
        case BinOp::Lt: return false;
    }
}

bool BinOp::is_boolean_op(const BinOp::OpType &op) {
    switch (op) {
        case BinOp::And:
        case BinOp::Or:
        case BinOp::Xor: // see above note
        case BinOp::Add:
        case BinOp::Mod:
        case BinOp::Mul:
        case BinOp::Div:
        case BinOp::Sub: return false;
        case BinOp::Neq:
        case BinOp::Eq:
        case BinOp::Le:
        case BinOp::Lt: return true;
    }
}

namespace {

void try_match_types(Expr &a, Expr &b) {
    if (a.type().defined() && b.type().defined()) {
        if (equals(a.type(), b.type())) return;
        internal_assert(is_const(a) || is_const(b)) << "Implicit casting of types: " << a << " is not the same type as " << b << ": " << a.type() << " versus " << b.type();
        // TODO: is this right?
        // Cast to the larger bitwidth
        if (a.type().bits() > b.type().bits()) {
            b = constant_cast(a.type(), b);
        } else if (b.type().bits() > a.type().bits()) {
            a = constant_cast(b.type(), a);
        } else {
            internal_error << "Same bitwidth, not sure how to cast: " << a << " and " << b
                           << " are types " << a.type() << " and " << b.type();
        }
    } else if (a.type().defined() && !b.type().defined() && is_const(b)) {
        b = constant_cast(a.type(), b);
    } else if (b.type().defined() && !a.type().defined() && is_const(a)) {
        a = constant_cast(b.type(), a);
    }
    // otherwise can't (currently) do better.
}

}  // namespace

Expr BinOp::make(BinOp::OpType op, Expr a, Expr b) {
    // TODO: operator overloading and broadcasting!
    internal_assert(a.defined() && b.defined()) << "BinOp of undefined: " << a << to_string(op) << b;

    BinOp *node = new BinOp;

    try_match_types(a, b);

    const bool infer_types = type_enforcement_enabled() || (a.type().defined() && b.type().defined());
    if (infer_types) {
        internal_assert(equals(a.type(), b.type())) << "BinOp of mismatched types: " << a << to_string(op) << b;

        if (BinOp::is_numeric_op(op)) {
            node->type = a.type();
        } else if (BinOp::is_boolean_op(op)) {
            node->type = a.type().to_bool();
        } else {
            internal_error << "Cannot infer output type: "  << a << to_string(op) << b;
        }
    }

    node->op = op;
    node->a = std::move(a);
    node->b = std::move(b);
    return node;
}

Expr UnOp::make(UnOp::OpType op, Expr a) {
    internal_assert(a.defined()) << "UnOp of undefined: " << to_string(op) << a;

    UnOp *node = new UnOp;

    const bool infer_types = type_enforcement_enabled() || a.type().defined();
    if (infer_types) {
        if (op == UnOp::Not) {
            // not on only integers and boolean? what does not of float mean
            internal_assert(a.type().is_int() || a.type().is_bool() || a.type().is_uint()) << "Cannot not non-([u]int | bool): " << to_string(op) << a;
            node->type = a.type();
        } else {
            // Must be signed int or float?
            internal_assert(a.type().is_float() || a.type().is_int()) << "Cannot negate non-(int | float): " << to_string(op) << a;
            node->type = a.type();
        }
    }

    node->op = op;
    node->a = std::move(a);
    return node;
}

Expr Broadcast::make(uint32_t lanes, Expr value) {
    internal_assert(value.defined()) << "Broadcast of undefined.";

    Broadcast *node = new Broadcast;

    const bool infer_types = type_enforcement_enabled() || value.type().defined();
    if (infer_types) {
        // TODO: support?
        internal_assert(value.type().is_scalar()) << "Broadcast of non-scalar: " << value;
        node->type = Vector_t::make(value.type(), lanes);
    }

    node->lanes = lanes;
    node->value = std::move(value);
    return node;
}

Expr VectorReduce::make(VectorReduce::OpType op, Expr value) {
    internal_assert(value.defined()) << "VectorReduce of undefined.";

    VectorReduce *node = new VectorReduce;

    const bool infer_types = type_enforcement_enabled() || value.type().defined();
    if (infer_types) {
        internal_assert(value.type().is_vector()) << "VectorReduce of non-vector: " << value;
        if (op == VectorReduce::Idxmin || op == VectorReduce::Idxmax) {
            internal_assert(value.type().element_of().is_scalar()) << "TODO: type for argmin/argmax of nested vector?: " << value;
            node->type = UInt_t::make(32);
            // TODO: what if it's not uint32?
        } else {
            node->type = value.type().element_of();
        }
    }

    node->op = op;
    node->value = std::move(value);
    return node;
}

Expr VectorShuffle::make(Expr value, std::vector<Expr> idxs) {
    internal_assert(value.defined()) << "VectorShuffle of undefined.";
    internal_assert(std::all_of(idxs.cbegin(), idxs.cend(), [](const auto &e) { return e.defined(); }))
        << "VectorShuffle with undefined idxs";

    VectorShuffle *node = new VectorShuffle;

    const bool infer_types = type_enforcement_enabled() || value.type().defined();
    if (infer_types) {
        internal_assert(value.type().is_vector()) << "VectorShuffle of non-vector: " << value;
        internal_assert(std::all_of(idxs.cbegin(), idxs.cend(), [](const auto &e) { return e.type().defined() && (e.type().is_int() || e.type().is_uint()); }))
            << "Vector Shuffle with undefined index types, of value: " << value << " on " << std::accumulate(std::next(idxs.begin()), idxs.end(), to_string(idxs[0]) + " : " + to_string(idxs[0].type()), [](const std::string &a, const Expr &b) { return a + ", " + to_string(b) + " : " + to_string(b.type()); });
        node->type = Vector_t::make(value.type().element_of(), idxs.size());
    }

    node->value = std::move(value);
    node->idxs = std::move(idxs);
    return node;
}

Expr Ramp::make(Expr base, Expr stride, int lanes) {
    internal_assert(base.defined() && stride.defined() && lanes > 1) << "Bad Ramp parameters: " << base << " " << stride << " " << lanes;

    Ramp *node = new Ramp;

    const bool infer_types = type_enforcement_enabled() || (stride.type().defined() && base.type().defined());
    if (infer_types) {
        internal_assert(equals(stride.type(), base.type())) << "Ramp of mismatched types: " << base << " " << stride << " " << lanes;
        node->type = Vector_t::make(base.type(), lanes);
    }

    node->base = std::move(base);
    node->stride = std::move(stride);
    node->lanes = lanes;
    return node;
}

Expr Build::make(Type type, std::vector<Expr> values) {
    Build *node = new Build;

    const bool infer_types = type_enforcement_enabled() || (type.defined() && std::all_of(values.cbegin(), values.cend(), [](const auto &v) { return v.type().defined(); }));

    if (infer_types) {
        internal_assert(type.defined()) << "Build received empty type: " << type;
        // TODO: if values.empty() or values.size() < expected_n, then assert type has defaults!
        for (const auto& expr : values) {
            internal_assert(expr.defined()) << "Build with undefined field of type: " << type;
        }

        if (type.is<Vector_t>()) {
            internal_assert(values.empty() || type.as<Vector_t>()->lanes == values.size())
                << "Build<Vector_t> with incorrect number of arguments, expected: " << type << " but received " << values.size() << " elements.";
            Type etype = type.as<Vector_t>()->etype;
            for (const auto& expr : values) {
                internal_assert(equals(expr.type(), etype)) << "Build<Vector_t> requires uniform element type, expected: " << etype << " but received " << expr;
            }
        } else if (type.is<Struct_t>()) {
            if (!values.empty()) {
                internal_assert(type.as<Struct_t>()->fields.size() == values.size())
                    << "Build<Struct_t> with incorrect number of arguments, expected: " << type << " but received " << values.size() << " elements.";

                const auto &fields = type.as<Struct_t>()->fields;
                for (size_t i = 0; i < values.size(); i++) {
                    internal_assert(equals(fields[i].second, values[i].type()))
                        << "Build<Vector_t> requires matching field types, expected: " << fields[i].second << " but received " << values[i] << " for field " << fields[i].first;
                }
            }
        } else {
            internal_error << "Build::make with non-(vector, struct) type: " << type;
        }
    }

    node->type = std::move(type);
    node->values = std::move(values);
    return node;
}

Expr Access::make(std::string field, Expr value) {
    internal_assert(!field.empty() && value.defined()) << "Bad Access parameters: " << field << " on " << value;

    Access *node = new Access;

    const bool infer_types = type_enforcement_enabled() || value.type().defined();
    if (infer_types) {
        node->type = get_field_type(value.type(), field);
    }

    node->field = std::move(field);
    node->value = std::move(value);
    return node;
}

Expr Intrinsic::make(OpType op, std::vector<Expr> args) {
    internal_assert(!args.empty() && std::all_of(args.cbegin(), args.cend(), [](const auto &arg) { return arg.defined(); }))
        << "Intrinsic received undefined argument";

    Intrinsic *node = new Intrinsic;

    const bool infer_types = type_enforcement_enabled() || std::all_of(args.cbegin(), args.cend(), [](const auto &arg) { return arg.type().defined(); });
    // TODO:implement type enforcement for all intrinsics.
    if (infer_types) {
        switch (op) {
            case Intrinsic::abs: {
                internal_assert(args.size() == 1);
                if (args[0].type().is_int()) {
                    node->type = args[0].type().to_uint();
                } else {
                    node->type = args[0].type();   
                }
                break;
            }
            default: {
                node->type = args[0].type();
                break;
            }
        }
    }
    node->op = op;
    node->args = std::move(args);
    return node;
}

Expr Lambda::make(std::vector<Lambda::Argument> args, Expr value) {
    internal_assert(value.defined()) << "Lambda::make received undefined value";
    for (const auto &arg : args) {
        internal_assert(!arg.name.empty()) << "Lambda::make received empty arg name";
        internal_assert(!type_enforcement_enabled() || arg.type.defined()) << "Lambda::make received undefined arg type: " << arg.name;
    }

    Lambda *node = new Lambda;

    // Only do type inference if it's enabled or both types are defined.
    const bool infer_types = type_enforcement_enabled() || (value.type().defined() && std::all_of(args.cbegin(), args.cend(), [](const auto &arg) { return arg.type.defined(); }));
    if (infer_types) {
        // TODO: assert that the vars are used?
        // or we can just optimize those out later, sometimes ppl write dumb code.
        std::vector<Type> arg_types(args.size());
        for (size_t i = 0; i < args.size(); i++) {
            arg_types[i] = args[i].type;
        }
        node->type = Function_t::make(value.type(), std::move(arg_types));
    }

    node->args = std::move(args);
    node->value = std::move(value);
    return node;
}

Expr GeomOp::make(OpType op, Expr a, Expr b) {
    internal_assert(a.defined() && b.defined()) << "GeomOp::make received undefined value: " << to_string(op) << " " << a << " " << b;
    GeomOp *node = new GeomOp;

    const bool infer_types = type_enforcement_enabled() || (a.type().defined() && b.type().defined());
    if (infer_types) {
        // TODO: assert that these are volumes with defined geometric constructs?
        const Struct_t *sa = a.type().as<Struct_t>();
        const Struct_t *sb = b.type().as<Struct_t>();
        internal_assert(sa && sb) << "GeomOp::make expected geometric structs: " << to_string(op) << " " << a << " " << b;

        Type ret_type;
        if (op == GeomOp::distance) {
            // TODO: distance could have any value, it's user-defined...
            // TODO: do we need Real_t?
            // For now, just assume f32
            node->type = Float_t::make(32);
        } else {
            node->type = Bool_t::make();
        }
    }

    node->op = op;
    node->a = std::move(a);
    node->b = std::move(b);
    return node;
}

Expr SetOp::make(OpType op, Expr a, Expr b) {
    internal_assert(a.defined() && b.defined()) << "SetOp::make received undefined value: " << to_string(op) << " " << a << " " << b;
    SetOp *node = new SetOp;

    // Only do type inference if it's enabled or both types are defined.
    const bool infer_types = type_enforcement_enabled() || (a.type().defined() && b.type().defined());

    if (infer_types) {
        if (op == SetOp::filter) {
            internal_assert(a.type().is<Function_t>() && a.type().as<Function_t>()->ret_type.is_bool())
                << "Expected lhs of filter to be a boolean function, instead received: " << a << " : " << a.type();
            internal_assert(b.type().is<Set_t>()) << "Expected rhs of filter to be a set, instead received: " << b << " : " << b.type();
            const Function_t *f = a.type().as<Function_t>();
            internal_assert(f->arg_types.size() == 1 && equals(f->arg_types[0], b.type().element_of()))
                << "Expected filter function to accept element of type: " << b.type().element_of() << " instead got " << a << " : " << a.type();
            node->type = b.type();
        } else if (op == SetOp::argmin) {
            internal_assert(a.type().is<Function_t>() && a.type().as<Function_t>()->ret_type.is_numeric())
                << "Expected lhs of argmin to be a numeric function, instead received: " << a << " : " << a.type();
            internal_assert(b.type().is<Set_t>()) << "Expected rhs of argmin to be a set, instead received: " << b << " : " << b.type();
            const Function_t *f = a.type().as<Function_t>();
            internal_assert(f->arg_types.size() == 1 && equals(f->arg_types[0], b.type().element_of()))
                << "Expected argmin function to accept element of type: " << b.type().element_of() << " instead got " << a << " : " << a.type();
            node->type = b.type().element_of();
        } else if (op == SetOp::map) {
            internal_assert(a.type().is<Function_t>()) << "Expected lhs of map to be afunction, instead received: " << a << " : " << a.type();
            internal_assert(b.type().is<Set_t>()) << "Expected rhs of map to be a set, instead received: " << b << " : " << b.type();
            const Function_t *f = a.type().as<Function_t>();
            internal_assert(f->arg_types.size() == 1 && equals(f->arg_types[0], b.type().element_of()))
                << "Expected map function to accept element of type: " << b.type().element_of() << " instead got " << a << " : " << a.type();
            node->type = Set_t::make(f->ret_type);
        } else if (op == SetOp::product) {
            internal_assert(a.type().is<Set_t>() && b.type().is<Set_t>())
                << "Expected args of product to be sets, instead received: " << a << " : " << a.type() << " and " << b << " : " << b.type();
            Type atype = a.type().element_of();
            Type btype = b.type().element_of();
            node->type = Tuple_t::make({std::move(atype), std::move(btype)});
        }
    }

    node->op = op;
    node->a = std::move(a);
    node->b = std::move(b);
    return node;
}

Expr Call::make(Expr func, std::vector<Expr> args) {
    internal_assert(func.defined()) << "Call::make received undefined func";
    internal_assert(std::all_of(args.cbegin(), args.cend(), [](const Expr &e) { return e.defined(); })) << "Call::make received undefined arg to func: " << func;

    Call *node = new Call;

    const bool infer_types = type_enforcement_enabled() || func.type().defined();

    if (infer_types) {
        internal_assert(func.type().defined()) << "Call::make needs func to have a defined type: " << func;
        internal_assert(func.type().is<Function_t>()) << "Call::make received non-callable func: " << func;
        const Function_t *f = func.type().as<Function_t>();
        internal_assert(f->arg_types.size() == args.size())
            << "Call::make received incorrect number of arguments to: " << func
            << ", expected: " << f->arg_types.size() << " but received: " << args.size();
        for (size_t i = 0; i < args.size(); i++) {
            internal_assert(f->arg_types[i].defined());
            if (!args[i].type().defined()) {
                internal_assert(is_const(args[i])) << "Undefined type in function call for non-constant expression: " << args[i];
                args[i] = constant_cast(f->arg_types[i], args[i]);
            } else {
                internal_assert(equals(args[i].type(), f->arg_types[i])) << "Call::make received bad argument: " << args[i] << " when expecting type: " << f->arg_types[i] << " at index " << i << " of call to func: " << func;
            }
        }
        node->type = f->ret_type;
    }

    node->func = std::move(func);
    node->args = std::move(args);
    return node;
}

}  // namespace ir
}  // namespace bonsai
