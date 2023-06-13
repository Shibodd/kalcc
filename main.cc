#include <iostream>
#include "driver.hh"

int main(int argc, char* argv[]) {
  if (argc <= 1) {
    std::cerr << "Usage: " << argv[0] << " source" << std::endl;
    return 1;
  }

  driver drv;

  for (int i = 2; i < argc; ++i) {
    std::string arg = std::string(argv[i]);
    if (arg == "-tc")
      drv.trace_codegen = true;
    else if (arg == "-tp")
      drv.trace_parsing = true;
    else if (arg == "-ts")
      drv.trace_scanning = true;
  }

  int ans = drv.parse(std::string(argv[1]));
  if (ans == 0) {
    drv.root->codegen(drv, 0);
  }
  else
    std::cout << "Error!" << std::endl;

  drv.llvmModule->print(llvm::outs(), nullptr);
}