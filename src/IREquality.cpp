#include "IREquality.h"

namespace bonsai {

bool equals(const Type &t0, const Type &t1) {
    if (t0.node_type() != t1.node_type()) {
        return false;
    }

    switch (t0.node_type()) {
        case IRTypeEnum::Int_t: {
            return t0.as<Int_t>()->bits == t1.as<Int_t>()->bits;
        }
        case IRTypeEnum::Float_t: {
            return t0.as<Float_t>()->bits == t1.as<Float_t>()->bits;
        }
        case IRTypeEnum::Bool_t: {
            return true;
        }
        case IRTypeEnum::Ptr_t: {
            return equals(t0.as<Ptr_t>()->etype, t1.as<Ptr_t>()->etype);
        }
        case IRTypeEnum::Vector_t: {
            const Vector_t *v0 = t0.as<Vector_t>();
            const Vector_t *v1 = t1.as<Vector_t>();
            return (v0->lanes == v1->lanes) && equals(v0->etype, v1->etype);
        }
        case IRTypeEnum::Struct_t: {
            const Struct_t *s0 = t0.as<Struct_t>();
            const Struct_t *s1 = t1.as<Struct_t>();
            if (s0->name != s1->name) {
                return false;
            }
            // TODO: can name ever match without fields matching?
            if (s0->fields.size() != s1->fields.size()) {
                return false;
            }
            for (const auto& [key, v0] : s0->fields) {
                const auto it1 = s1->fields.find(key);
                if (it1 == s1->fields.end()) {
                    return false;
                }

                const Type &v1 = it1->second;
                if (!equals(v0, v1)) {
                    return false;
                }
            }
            return true;
        }
    }
}

} // namespace bonsai
