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

UnaryExprAST::UnaryExprAST(UnaryOperator op, std::unique_ptr<ExprAST> operand)
  : op(op),
    operand(std::move(operand)) {}

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
      std::unique_ptr<ExprAST> then_expr,
      std::unique_ptr<ExprAST> else_expr)
  : cond_expr(std::move(cond_expr)),
    then_expr(std::move(then_expr)),
    else_expr(std::move(else_expr)) {}

CompositeExprAST::CompositeExprAST(
    std::unique_ptr<ExprAST> current,
    std::unique_ptr<ExprAST> next)
  : current(std::move(current)),
    next(std::move(next)) {}

ForExprAST::ForExprAST(
    std::string id_name,
    std::unique_ptr<ExprAST> init_expr,
    std::unique_ptr<ExprAST> cond_expr,
    std::unique_ptr<ExprAST> step_expr,
    std::unique_ptr<ExprAST> body_expr)
  : id_name(id_name),
    init_expr(std::move(init_expr)),
    cond_expr(std::move(cond_expr)),
    step_expr(std::move(step_expr)),
    body_expr(std::move(body_expr)) {}

WhileExprAST::WhileExprAST(
    std::unique_ptr<ExprAST> cond_expr,
    std::unique_ptr<ExprAST> body_expr)
  : cond_expr(std::move(cond_expr)),
    body_expr(std::move(body_expr)) {}

AssignmentExprAST::AssignmentExprAST(
    std::string id_name,
    std::unique_ptr<ExprAST> value_expr)
  : id_name(id_name),
    value_expr(std::move(value_expr)) {}

VarExprAST::VarExprAST(
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> declarations,
    std::unique_ptr<ExprAST> body)
  : declarations(std::move(declarations)),
    body(std::move(body)) {}



/* CODE GENERATION */

static inline void dbglog(const driver& drv, const std::string& construct, const std::string& str, int depth) {
  if (drv.trace_codegen)
    llvm::errs() << std::string(depth, '\'') << "[" << construct << "] " << str << "\n";
}

static llvm::AllocaInst* createAllocaInEntryBlock(const driver& drv, llvm::Function *F, const std::string &varName) {
  llvm::BasicBlock& entryBlock = F->getEntryBlock();
  llvm::IRBuilder<> builder(&entryBlock, entryBlock.begin());
  return builder.CreateAlloca(llvm::Type::getDoubleTy(*drv.llvmContext), nullptr, varName);
}


#include <llvm/IR/Verifier.h>

llvm::Value* VariableExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Variable", this->name, depth);

  llvm::AllocaInst* ptr = drv.namedPointers[this->name];
  if (!ptr)
    throw "Unknown variable name: " + this->name;

  return drv.llvmIRBuilder->CreateLoad(ptr->getAllocatedType(), ptr, this->name);
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
    { BinaryOperator::Div, "Div" },
    { BinaryOperator::Gt, "Gt" },
    { BinaryOperator::Gte, "Gte" },
    { BinaryOperator::Lt, "Lt" },
    { BinaryOperator::Lte, "Lte" },
    { BinaryOperator::Eq, "Eq" },
    { BinaryOperator::Neq, "Neq" },
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

    case BinaryOperator::Gt:
      return drv.llvmIRBuilder->CreateUIToFP(
        drv.llvmIRBuilder->CreateFCmpOGT(lhs, rhs, "booltmp"),
        llvm::Type::getDoubleTy(*drv.llvmContext), "dbltmp"
      );

    case BinaryOperator::Lt:
      return drv.llvmIRBuilder->CreateUIToFP(
        drv.llvmIRBuilder->CreateFCmpOLT(lhs, rhs, "booltmp"),
        llvm::Type::getDoubleTy(*drv.llvmContext), "dbltmp"
      );

    case BinaryOperator::Gte:
      return drv.llvmIRBuilder->CreateUIToFP(
        drv.llvmIRBuilder->CreateFCmpOGE(lhs, rhs, "booltmp"),
        llvm::Type::getDoubleTy(*drv.llvmContext), "dbltmp"
      );

    case BinaryOperator::Lte:
      return drv.llvmIRBuilder->CreateUIToFP(
        drv.llvmIRBuilder->CreateFCmpOLE(lhs, rhs, "booltmp"),
        llvm::Type::getDoubleTy(*drv.llvmContext), "dbltmp"
      );
    
    case BinaryOperator::Eq:
      return drv.llvmIRBuilder->CreateUIToFP(
        drv.llvmIRBuilder->CreateFCmpOEQ(lhs, rhs, "booltmp"),
        llvm::Type::getDoubleTy(*drv.llvmContext), "dbltmp"
      );
    
    case BinaryOperator::Neq:
      return drv.llvmIRBuilder->CreateUIToFP(
        drv.llvmIRBuilder->CreateFCmpONE(lhs, rhs, "booltmp"),
        llvm::Type::getDoubleTy(*drv.llvmContext), "dbltmp"
      );
  }

  assert(false);
}

llvm::Value* UnaryExprAST::codegen(driver& drv, int depth) {
  static const std::map<UnaryOperator, std::string> UNOP_NAMES = {
    { UnaryOperator::NumericNeg, "NumericNeg" }
  };

  dbglog(drv, "Unary expression", UNOP_NAMES.at(this->op), depth);

  llvm::Value* op_value = this->operand->codegen(drv, depth + 1);

  assert(op_value);

  switch (this->op) {
    case UnaryOperator::NumericNeg:
      return drv.llvmIRBuilder->CreateFNeg(op_value, "numnegtmp");
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
  dbglog(drv, "If expression", "", depth);

  // Condition
  llvm::Value* cond_val = this->cond_expr->codegen(drv, depth + 1);
  assert(cond_val);
  
  llvm::Value* bool_cond_val = drv.llvmIRBuilder->CreateFCmpONE(
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
  drv.llvmIRBuilder->CreateCondBr(bool_cond_val, thenBB, elseBB);
  
  // Then
  auto& bblist = F->getBasicBlockList();

  bblist.insert(bblist.end(), thenBB);
  drv.llvmIRBuilder->SetInsertPoint(thenBB);

  llvm::Value* then_val = this->then_expr->codegen(drv, depth + 1);
  assert(then_val);

  drv.llvmIRBuilder->CreateBr(mergeBB);
  thenBB = drv.llvmIRBuilder->GetInsertBlock(); // update BB with the last emitted one

  
  // Else
  bblist.insert(bblist.end(), elseBB);
  drv.llvmIRBuilder->SetInsertPoint(elseBB);

  llvm::Value* else_val = this->else_expr->codegen(drv, depth + 1);
  assert(else_val);

  drv.llvmIRBuilder->CreateBr(mergeBB);
  elseBB = drv.llvmIRBuilder->GetInsertBlock(); // update BB with the last emitted one


  // Merge
  bblist.insert(bblist.end(), mergeBB);
  drv.llvmIRBuilder->SetInsertPoint(mergeBB);
  llvm::PHINode *PN = drv.llvmIRBuilder->CreatePHI(llvm::Type::getDoubleTy(*drv.llvmContext), 2, "iftmp");

  PN->addIncoming(then_val, thenBB);
  PN->addIncoming(else_val, elseBB);
  return PN;
}


llvm::Value* CompositeExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Composite Expression", "", depth);

  assert(this->current);
  llvm::Value* cur_val = this->current->codegen(drv, depth + 1);

  if (this->next) {
    return this->next->codegen(drv, depth + 1);
  } else {
    return cur_val;
  }
}

llvm::Value* ForExprAST::codegen(driver& drv, int depth)  {
  dbglog(drv, "For Expression", "", depth);
  return nullptr;
}

llvm::Value* WhileExprAST::codegen(driver& drv, int depth)  {
  dbglog(drv, "While Expression", "", depth);
  return nullptr;
}

llvm::Value* AssignmentExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Assignment", this->id_name, depth);
  assert(this->value_expr);

  llvm::Value* value = this->value_expr->codegen(drv, depth);
  assert(value);

  llvm::Value *ptr = drv.namedPointers[this->id_name];
  if (!ptr)
    throw "Unknown variable name: " + this->id_name;

  drv.llvmIRBuilder->CreateStore(value, ptr);
  return value;
}

llvm::Value* VarExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "VarExpr", "", depth);

  llvm::Function* F = drv.llvmIRBuilder->GetInsertBlock()->getParent();

  for (auto &decl : this->declarations) {
    llvm::Value* initValue = decl.second->codegen(drv, depth + 1);
    assert(initValue);

    if (drv.namedPointers[decl.first])
      throw "Redefinition of variable " + decl.first;

    llvm::AllocaInst* ptr = createAllocaInEntryBlock(drv, F, decl.first);
    drv.llvmIRBuilder->CreateStore(initValue, ptr);
    drv.namedPointers[decl.first] = ptr;
  }

  return this->body->codegen(drv, depth + 1);
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

  llvm::Function* F = drv.llvmModule->getFunction(this->prototype->getName());
  if (!F)
    F = this->prototype->codegen(drv, depth + 1);

  assert(F);

  if (!F->empty())
    throw "Redefinition of function " + F->getName();

  llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(*drv.llvmContext, "entry", F);
  drv.llvmIRBuilder->SetInsertPoint(entryBB);

  drv.namedPointers.clear();
  for (auto &arg : F->args()) {
    llvm::AllocaInst* ptr = createAllocaInEntryBlock(drv, F, std::string(arg.getName()));
    drv.llvmIRBuilder->CreateStore(&arg, ptr);
    drv.namedPointers[std::string(arg.getName())] = ptr;
  }

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