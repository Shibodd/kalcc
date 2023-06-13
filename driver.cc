#include "driver.hh"
#include "parser.hh"

driver::driver ()
  : trace_parsing(false), 
    trace_scanning(false),
    trace_codegen(false)
{ 
  llvmContext = std::make_unique<llvm::LLVMContext>();
  llvmModule = std::make_unique<llvm::Module>("Kaleidoscope", *llvmContext);
  llvmIRBuilder = std::make_unique<llvm::IRBuilder<>>(*llvmContext);
}

int driver::parse (const std::string &f)
{
  file = f;
  location.initialize (&file);
  
  scan_begin();

  yy::parser parser(*this);
  parser.set_debug_level (trace_parsing);

  int res = parser.parse();
  scan_end();
  return res;
}
