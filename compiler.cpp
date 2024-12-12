#include <iostream>
#include <fstream>
#include "Bonsai.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }

    const std::string filename = argv[1];

    // Parse the input file
    bonsai::ir::Program program;
    program = bonsai::parser::parse(filename);
    // try {
    //     program = bonsai::parser::parse(filename);
    // } catch (const std::exception& e) {
    //     std::cerr << "Parsing failed: " << e.what() << std::endl;
    //     return 1;
    // }

    // TODO: could print Program.
    std::cerr << "TODO: implement full Program lowering!" << std::endl;
    return 1;

    // TODO: perform lowering passes
    // TODO: perform code generation
    // TODO: AOT or JIT
}