#include "driver.hh"

int main(int argc, char* argv[]) {
  if (argc <= 1) {
    llvm::errs() << "Usage: " << argv[0] << " source\n";
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
    std::string error = "";

    try {
      drv.root->codegen(drv, 0);
    } catch (std::string& s) {
      error = s;
    }

    if (drv.trace_codegen || drv.trace_parsing || drv.trace_scanning)
      llvm::errs() << "\n";

    if (error != "")
      llvm::errs() << "Error: " << error << "\n";
    else
      drv.llvmModule->print(llvm::outs(), nullptr);
  }
  else
    llvm::errs() << "Error!\n";
}