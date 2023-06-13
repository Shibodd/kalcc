#include "driver.hh"
#include "parser.hh"

driver::driver ()
  : trace_parsing (false), 
    trace_scanning (false)
{ 
  llvmContext = std::make_unique<llvm::LLVMContext>();
  llvmModule = std::make_unique<llvm::Module>("Kaleidoscope", *llvmContext);
}

int driver::parse (const std::string &f)
{
  file = f;
  location.initialize (&file);
  
  scan_begin();

  yy::parser parse(*this);
  parse.set_debug_level (trace_parsing);

  int res = parse();
  scan_end();
  return res;
}
