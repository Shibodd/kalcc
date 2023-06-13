#include "ast.hh"
#include "driver.hh"
#include <exception>

/* CONSTRUCTORS IMPLEMENTATIONS */

VariableExprAST::VariableExprAST(const std::string &name)
  : name(name) {}

NumberExprAST::NumberExprAST(double value)
  : value(value) {}

BinaryExprAST::BinaryExprAST(
      BinaryOperator op,
      std::unique_ptr<ExprAST> lhs,
      std::unique_ptr<ExprAST> rhs)
  : op(op),
    lhs(std::move(lhs)),
    rhs(std::move(rhs)) {}

CallExprAST::CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args)
  : callee(callee),
    args(std::move(args)) {}

SequenceAST::SequenceAST(std::unique_ptr<RootAST> current, std::unique_ptr<SequenceAST> next)
  : current(std::move(current)),
    next(std::move(next)) {}

FunctionPrototypeAST::FunctionPrototypeAST(const std::string &name, std::vector<std::string> argsNames)
  : name(name),
    argsNames(argsNames) {}
const std::string& FunctionPrototypeAST::getName() const { return name; }

FunctionAST::FunctionAST(std::unique_ptr<FunctionPrototypeAST> prototype, std::unique_ptr<ExprAST> body)
  : prototype(std::move(prototype)),
    body(std::move(body)) {}

IfExprAST::IfExprAST(
      std::unique_ptr<ExprAST> cond_expr,
      std::unique_ptr<ExprAST> true_expr,
      std::unique_ptr<ExprAST> false_expr)
  : cond_expr(std::move(cond_expr)),
    true_expr(std::move(true_expr)),
    false_expr(std::move(false_expr)) {}


/* CODE GENERATION */

static inline void dbglog(const driver& drv, const std::string& construct, const std::string& str, int depth) {
  if (drv.trace_codegen)
    llvm::errs() << std::string(depth, '\'') << "[" << construct << "] " << str << "\n";
}


#include <llvm/IR/Verifier.h>

llvm::Value* VariableExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Variable", this->name, depth);

  llvm::Value* val = drv.namedValues[this->name];

  if (!val)
    throw "Unknown variable name: " + this->name;

  return val;
}

llvm::Value* NumberExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Number", std::to_string(this->value), depth);

  return llvm::ConstantFP::get(*drv.llvmContext, llvm::APFloat(this->value));
}

llvm::Value* BinaryExprAST::codegen(driver& drv, int depth) {
  static const std::map<BinaryOperator, std::string> BINOP_NAMES = {
    { BinaryOperator::Add, "Add" },
    { BinaryOperator::Sub, "Sub" },
    { BinaryOperator::Mul, "Mul" },
    { BinaryOperator::Div, "Div" }
  };

  dbglog(drv, "Binary expression", BINOP_NAMES.at(this->op), depth);

  llvm::Value* lhs = this->lhs->codegen(drv, depth + 1);
  llvm::Value* rhs = this->rhs->codegen(drv, depth + 1);

  assert(lhs && rhs);

  switch (this->op) {
    case BinaryOperator::Add:
      return drv.llvmIRBuilder->CreateFAdd(lhs, rhs, "addtmp");
    case BinaryOperator::Sub:
      return drv.llvmIRBuilder->CreateFSub(lhs, rhs, "subtmp");
    case BinaryOperator::Mul:
      return drv.llvmIRBuilder->CreateFMul(lhs, rhs, "multmp");
    case BinaryOperator::Div:
      return drv.llvmIRBuilder->CreateFDiv(lhs, rhs, "divtmp");
  }

  assert(false);
}

llvm::Value* CallExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Function call", this->callee, depth);

  llvm::Function* fun = drv.llvmModule->getFunction(this->callee);
  if (!fun)
    throw "Called unknown function " + this->callee;
  
  if (fun->arg_size() != this->args.size())
    throw "Function call argument count mismatch: expecting " + std::to_string(fun->arg_size()) + ", got " + std::to_string(this->args.size());
  
  std::vector<llvm::Value *> args;
  for (unsigned i = 0, e = this->args.size(); i != e; ++i) {
    args.push_back(this->args[i]->codegen(drv, depth + 1));
    if (!args.back())
      return nullptr;
  }

  return drv.llvmIRBuilder->CreateCall(fun, args, "calltmp");
}

llvm::Value* IfExprAST::codegen(driver& drv, int depth) {
  // Condition
  llvm::Value* cond_val = this->cond_expr->codegen(drv, depth + 1);
  assert(cond_val);
  drv.llvmIRBuilder->CreateFCmpONE(
    cond_val,
    llvm::ConstantFP::get(*drv.llvmContext, llvm::APFloat(0.0)),
    "ifcond"
  );

  
  // CFG
  llvm::Function *F = drv.llvmIRBuilder->GetInsertBlock()->getParent();
  llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(*drv.llvmContext, "then");
  llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(*drv.llvmContext, "else");
  llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(*drv.llvmContext, "ifcont");

  
  // Conditional branch
  drv.llvmIRBuilder->CreateCondBr(cond_val, thenBB, elseBB);

  
  // Then
  F->insert(F->end(), thenBB);
  drv.llvmIRBuilder->SetInsertPoint(thenBB);

  llvm::Value* then_val = this->then_expr->codegen(drv, depth + 1);
  assert(then_val);

  drv.llvmIRBuilder->CreateBr(mergeBB);
  thenBB = drv.llvmIRBuilder->GetInsertBlock(); // update BB with the last emitted one

  
  // Else
  F->insert(F->end(), elseBB);
  drv.llvmIRBuilder->SetInsertPoint(elseBB);

  llvm::Value* else_val = this->else_expr->codegen(drv, depth + 1);
  assert(else_val);

  drv.llvmIRBuilder->CreateBr(mergeBB);
  elseBB = drv.llvmIRBuilder->GetInsertBlock(); // update BB with the last emitted one


  // Merge
  F->insert(F->end(), mergeBB);
  drv.llvmIRBuilder->SetInsertPoint(mergeBB);
  llvm::PHINode *PN = Builder->CreatePHI(Type::getDoubleTy(*drv.llvmContext), 2, "iftmp");

  PN->addIncoming(then_val, thenBB);
  PN->addIncoming(else_val, elseBB);
  return PN;
}

llvm::Function* FunctionPrototypeAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Function prototype", this->getName(), depth);

  std::vector<llvm::Type *> types(this->argsNames.size(), llvm::Type::getDoubleTy(*drv.llvmContext));

  llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*drv.llvmContext), types, false);

  llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, this->name, drv.llvmModule.get());

  unsigned i = 0;
  for (auto &arg : F->args())
    arg.setName(this->argsNames[i++]);

  return F;
}
  
llvm::Value* FunctionAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Function", this->prototype->getName(), depth);

  llvm::Function *F = drv.llvmModule->getFunction(this->prototype->getName());
  if (!F)
    F = this->prototype->codegen(drv, depth + 1);

  assert(F);

  if (!F->empty())
    throw "Redefinition of function " + F->getName();

  llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(*drv.llvmContext, "entry", F);
  drv.llvmIRBuilder->SetInsertPoint(entryBB);

  drv.namedValues.clear(); // ????
  for (auto &arg : F->args())
    drv.namedValues[std::string(arg.getName())] = &arg;

  llvm::Value *returnValue = this->body->codegen(drv, depth + 1);
  assert(returnValue);
  drv.llvmIRBuilder->CreateRet(returnValue);

  llvm::verifyFunction(*F);

  return F;
}

llvm::Value* SequenceAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Sequence", "", depth);

  if (this->current) {
    if (ExprAST* expr = dynamic_cast<ExprAST*>(this->current.get())) {
      // We have a top level expression - replace it with an anonymous function

      // We need to get a new unique_ptr to ExprAST...
      this->current.release();
      std::unique_ptr<ExprAST> expr_ptr = std::unique_ptr<ExprAST>(expr);
      
      // Create the anon function and replace the current node with it
      const std::string anon_fun_name = "__anon_expr" + std::to_string(drv.get_unique_id());
      auto anon_fun_proto = std::make_unique<FunctionPrototypeAST>(anon_fun_name, std::vector<std::string>());
      this->current = std::make_unique<FunctionAST>(std::move(anon_fun_proto), std::move(expr_ptr));
    }

    this->current->codegen(drv, depth + 1);
  }

  if (this->next)
    this->next->codegen(drv, depth + 1);
  
  return nullptr;
}