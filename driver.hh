#ifndef DRIVER_HH
#define DRIVER_HH

#include "parser.hh"
#include <map>

#include <memory>
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

# define YY_DECL yy::parser::symbol_type yylex(driver& drv)

YY_DECL;

class driver
{
  unsigned long long unique_id;

public:
  driver();

  std::unique_ptr<llvm::LLVMContext> llvmContext;
  std::unique_ptr<llvm::Module> llvmModule;
  std::unique_ptr<llvm::IRBuilder<>> llvmIRBuilder;
  std::map<std::string, llvm::AllocaInst*> namedPointers;

  std::unique_ptr<RootAST> root;

  // Run the parser on file F.  Return 0 on success.
  int parse (const std::string& f);

  unsigned long long get_unique_id();

  // The name of the file being parsed.
  std::string file;
  
  // Whether to generate parser debug traces.
  bool trace_parsing;

  bool trace_codegen;
  
  // Handling the scanner.
  void scan_begin ();
  void scan_end ();
  
  // Whether to generate scanner debug traces.
  bool trace_scanning;
  
  // The token's location used by the scanner.
  yy::location location;
};

#endif // !DRIVER_HH