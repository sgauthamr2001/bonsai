# bonsai
DSL for Recursive Geometric Queries

```bash
export LLVM_ROOT=/Users/ajroot/projects/llvm-install-18
export LLVM_CONFIG=$LLVM_ROOT/bin/llvm-config
cmake 
cmake -S . -B build
cmake --build build
# ORm for Debug
cmake -S . -B build-dbg -DCMAKE_BUILD_TYPE=Debug
cmake --build build-dbg --config Debug
```

### Acknowledgements

A significant portion of the code in this repository is modeled after, or directly taken from, the [Halide](https://github.com/halide/Halide) compiler. That is because they both have done incredible work, and because it is the compiler that I (AJR) am most familiar with navigating and understanding. As a result, this repository benefits heavily from over a decade of hard work from the Halide developers.
