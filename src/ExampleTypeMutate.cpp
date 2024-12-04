#include "ExampleTypeMutate.h"

#include "IRMutator.h"

namespace bonsai {

namespace {

struct MergeFloat2x2ToFloat4 : public IRMutator {
    Type visit(const Vector_t *node) override {
        if (node->lanes == 2) {
            // TODO: how to merge these if statements?
            if (const Vector_t *inner = node->etype.as<Vector_t>()) {
                if (inner->etype.as<Float_t>() && inner->lanes == 2) {
                    return Vector_t::make(inner->etype, 4);
                }
            }
        }
        return IRMutator::visit(node);
    }
};

}

Type merge_float2x2_to_float4(const Type &type) {
    return MergeFloat2x2ToFloat4().mutate(type);
}

} // namespace bonsai
