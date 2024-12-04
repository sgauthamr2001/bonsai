#include "Bonsai.h"

using namespace bonsai;

void test_example() {
    Type f32 = Float_t::make(32);
    Type f32x2 = Vector_t::make(f32, 2);
    Type f32x2x2 = Vector_t::make(f32x2, 2);
    std::cout << f32x2x2 << std::endl;
    Type p_f32x2x2 = Ptr_t::make(f32x2x2);

    Type mut = merge_float2x2_to_float4(p_f32x2x2);
    std::cout << mut << std::endl;
}

int main(void) {
    test_example();
}
