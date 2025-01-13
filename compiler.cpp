#include <iostream>
#include <fstream>
#include "Bonsai.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }

    bool jit = std::string(argv[1]) == "-jit";

    if (jit && argc < 3) {
        std::cerr << "Usage: " << argv[0] << "-jit <input_file>" << std::endl;
        return 1;
    }

    const std::string filename = jit ? argv[2] : argv[1];

    // Parse the input file
    bonsai::ir::Program program = bonsai::parser::parse(filename);

    // Perform type inference.
    program = bonsai::lower::infer_types(program);

    // Perform canoncalization.
    program = bonsai::lower::canonicalize(program);

    program.dump(std::cout);
    // bonsai::internal_error << "TODO: implement lowering after type inference";



    // TODO:
    // Lower spatial queries
    // Perform first round of scheduling.
    // Lower data structures.
    // Perform second round of scheduling.
    // Perform final code generation

    bonsai::CodeGen_LLVM codegen;

    if (jit) {
        bonsai::codegen::jit(program, &codegen);
    } else {
        auto _module = codegen.compile_program(program);
        // TODO: compile to object / header files.
    }

    return 1;
}