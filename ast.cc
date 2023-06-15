#include "ast.hh"
#include "driver.hh"
#include <exception>

/* CONSTRUCTORS IMPLEMENTATIONS */

VariableExprAST::VariableExprAST(
      const std::string &name,
      const location& loc)
  : ExprAST(loc),
    name(name) {}

NumberExprAST::NumberExprAST(
      double value, 
      const location& loc)
  : ExprAST(loc),
  value(value) {}

BinaryExprAST::BinaryExprAST(
      BinaryOperator op,
      std::unique_ptr<ExprAST> lhs,
      std::unique_ptr<ExprAST> rhs,
      const location& loc)
  : ExprAST(loc),
    op(op),
    lhs(std::move(lhs)),
    rhs(std::move(rhs)) {}

UnaryExprAST::UnaryExprAST(
      UnaryOperator op,
      std::unique_ptr<ExprAST> operand,
      const location& loc)
  : ExprAST(loc),
    op(op),
    operand(std::move(operand)) {}

CallExprAST::CallExprAST(
      const std::string &callee,
      std::vector<std::unique_ptr<ExprAST>> args,
      const location& loc)
  : ExprAST(loc),
    callee(callee),
    args(std::move(args)) {}

SequenceAST::SequenceAST(
      std::unique_ptr<RootAST> current,
      std::unique_ptr<SequenceAST> next,
      const location& loc)
  : RootAST(loc),
    current(std::move(current)),
    next(std::move(next)) {}

FunctionPrototypeAST::FunctionPrototypeAST(
      const std::string &name,
      std::vector<std::string> argsNames,
      const location& loc)
  : RootAST(loc),
    name(name),
    argsNames(argsNames) {}

const std::string& FunctionPrototypeAST::getName() const { return name; }

FunctionAST::FunctionAST(
      std::unique_ptr<FunctionPrototypeAST> prototype,
      std::unique_ptr<ExprAST> body,
      const location& loc)
  : RootAST(loc),
    prototype(std::move(prototype)),
    body(std::move(body)) {}

IfExprAST::IfExprAST(
      std::unique_ptr<ExprAST> cond_expr,
      std::unique_ptr<ExprAST> then_expr,
      std::unique_ptr<ExprAST> else_expr,
      const location& loc)
  : ExprAST(loc),
    cond_expr(std::move(cond_expr)),
    then_expr(std::move(then_expr)),
    else_expr(std::move(else_expr)) {}

CompositeExprAST::CompositeExprAST(
      std::unique_ptr<ExprAST> current,
      std::unique_ptr<ExprAST> next,
      const location& loc)
  : ExprAST(loc),
    current(std::move(current)),
    next(std::move(next)) {}

ForExprAST::ForExprAST(
      std::unique_ptr<AssignmentExprAST> init_expr,
      std::unique_ptr<ExprAST> cond_expr,
      std::unique_ptr<AssignmentExprAST>  step_expr,
      std::unique_ptr<ExprAST> body_expr,
      const location& loc)
  : ExprAST(loc),
    init_expr(std::move(init_expr)),
    cond_expr(std::move(cond_expr)),
    step_expr(std::move(step_expr)),
    body_expr(std::move(body_expr)) {}

WhileExprAST::WhileExprAST(
      std::unique_ptr<ExprAST> cond_expr,
      std::unique_ptr<ExprAST> body_expr,
      const location& loc)
  : ExprAST(loc),
    cond_expr(std::move(cond_expr)),
    body_expr(std::move(body_expr)) {}

AssignmentExprAST::AssignmentExprAST(
      std::string id_name,
      std::unique_ptr<ExprAST> value_expr,
      const location& loc)
  : ExprAST(loc),
    id_name(id_name),
    value_expr(std::move(value_expr)) {}
const std::string& AssignmentExprAST::getDestinationName() const { return id_name; }

VarExprAST::VarExprAST(
      std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> declarations,
      std::unique_ptr<ExprAST> body,
      const location& loc)
  : ExprAST(loc),
    declarations(std::move(declarations)),
    body(std::move(body)) {}


/* CODE GENERATION */
typedef yy::position position;
static inline std::string posToStrVerbose(const position& pos) {
  return "Ln " + std::to_string(pos.line) + " Col " + std::to_string(pos.column);
}

static inline std::string posToStrCompact(const position& pos) {
  return "{" + std::to_string(pos.line) + ", " + std::to_string(pos.column) + "}";
}

static inline void error(const location& loc, const std::string& message) {
  throw "Error at " + posToStrVerbose(loc.begin) + ": " + message;
}

static inline void dbglog(const driver& drv, const std::string& construct, const std::string& str, int depth, const location& loc) {
  if (drv.trace_codegen) {
    llvm::errs() << std::string(depth, '\'') << "[" << construct;
    if (!str.empty())
      llvm::errs() << " \"" << str << "\"";
    llvm::errs() << "]  ";

    llvm::errs() << "From " << posToStrCompact(loc.begin) << " to " << posToStrCompact(loc.end) << "\n";
  }
}

static llvm::AllocaInst* createAllocaInEntryBlock(const driver& drv, llvm::Function *F, const std::string &varName) {
  llvm::BasicBlock& entryBlock = F->getEntryBlock();
  llvm::IRBuilder<> builder(&entryBlock, entryBlock.begin());
  return builder.CreateAlloca(llvm::Type::getDoubleTy(*drv.llvmContext), nullptr, varName);
}

static llvm::AllocaInst* createVar(driver& drv, llvm::Function* F, const std::string& name, const location& loc, llvm::Value* initValue = nullptr) {
  if (drv.namedPointers[name])
    error(loc, "Redefinition of variable " + name);

  llvm::AllocaInst* ptr = createAllocaInEntryBlock(drv, F, name);

  drv.namedPointers[name] = ptr;

  if (initValue)
    drv.llvmIRBuilder->CreateStore(initValue, ptr);
    
  return ptr;
}

static llvm::AllocaInst* getVar(driver& drv, const location& loc, const std::string& name) {
  llvm::AllocaInst* ptr = drv.namedPointers[name];
  if (!ptr)
    error(loc, "Unknown variable name: " + name);
  return ptr;
}

static llvm::Value* doubleToBoolean(const driver& drv, llvm::Value* cond_val) {
  return drv.llvmIRBuilder->CreateFCmpONE(
    cond_val,
    llvm::ConstantFP::get(*drv.llvmContext, llvm::APFloat(0.0)),
    "cond"
  );
}

static llvm::Value* booleanToDouble(const driver& drv, llvm::Value* cond_val) {
  return drv.llvmIRBuilder->CreateUIToFP(
    cond_val,
    llvm::Type::getDoubleTy(*drv.llvmContext), "dbltmp"
  );
}


#include <llvm/IR/Verifier.h>

llvm::Value* VariableExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Variable", this->name, depth, this->getLocation());

  llvm::AllocaInst* ptr = getVar(drv, this->getLocation(), this->name);
  return drv.llvmIRBuilder->CreateLoad(ptr->getAllocatedType(), ptr, this->name);
}

llvm::Value* NumberExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Number", std::to_string(this->value), depth, this->getLocation());

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

  dbglog(drv, "Binary expression", BINOP_NAMES.at(this->op), depth, this->getLocation());

  llvm::Value* lhs = this->lhs->codegen(drv, depth + 1);
  llvm::Value* rhs = this->rhs->codegen(drv, depth + 1);

  assert(lhs && rhs);

  switch (this->op) {
    case BinaryOperator::Add:
      return drv.llvmIRBuilder->CreateFAdd(lhs, rhs, "add_tmp");
    case BinaryOperator::Sub:
      return drv.llvmIRBuilder->CreateFSub(lhs, rhs, "sub_tmp");
    case BinaryOperator::Mul:
      return drv.llvmIRBuilder->CreateFMul(lhs, rhs, "mul_tmp");
    case BinaryOperator::Div:
      return drv.llvmIRBuilder->CreateFDiv(lhs, rhs, "div_tmp");
    case BinaryOperator::Gt:
      return booleanToDouble(drv, drv.llvmIRBuilder->CreateFCmpOGT(lhs, rhs, "gt_tmp"));
    case BinaryOperator::Lt:
      return booleanToDouble(drv, drv.llvmIRBuilder->CreateFCmpOLT(lhs, rhs, "lt_tmp"));
    case BinaryOperator::Gte:
      return booleanToDouble(drv, drv.llvmIRBuilder->CreateFCmpOGE(lhs, rhs, "gte_tmp"));
    case BinaryOperator::Lte:
      return booleanToDouble(drv, drv.llvmIRBuilder->CreateFCmpOLE(lhs, rhs, "lte_tmp"));
    case BinaryOperator::Eq:
      return booleanToDouble(drv, drv.llvmIRBuilder->CreateFCmpOEQ(lhs, rhs, "eq_tmp"));
    case BinaryOperator::Neq:
      return booleanToDouble(drv, drv.llvmIRBuilder->CreateFCmpONE(lhs, rhs, "neq_tmp"));
  }

  assert(false);
}

llvm::Value* UnaryExprAST::codegen(driver& drv, int depth) {
  static const std::map<UnaryOperator, std::string> UNOP_NAMES = {
    { UnaryOperator::NumericNeg, "NumericNeg" }
  };

  dbglog(drv, "Unary expression", UNOP_NAMES.at(this->op), depth, this->getLocation());

  llvm::Value* op_value = this->operand->codegen(drv, depth + 1);

  assert(op_value);

  switch (this->op) {
    case UnaryOperator::NumericNeg:
      return drv.llvmIRBuilder->CreateFNeg(op_value, "num_neg_tmp");
  }

  assert(false);
}

llvm::Value* CallExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Function call", this->callee, depth, this->getLocation());

  llvm::Function* fun = drv.llvmModule->getFunction(this->callee);
  if (!fun)
    error(this->getLocation(), "Called unknown function " + this->callee);
  
  if (fun->arg_size() != this->args.size())
    error(this->getLocation(), "Function call argument count mismatch: expecting " + std::to_string(fun->arg_size()) + ", got " + std::to_string(this->args.size()));
  
  std::vector<llvm::Value *> args;
  for (unsigned i = 0, e = this->args.size(); i != e; ++i) {
    args.push_back(this->args[i]->codegen(drv, depth + 1));
    if (!args.back())
      return nullptr;
  }

  return drv.llvmIRBuilder->CreateCall(fun, args, "call_tmp");
}

llvm::Value* IfExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "If expression", "", depth, this->getLocation());

  // Condition
  llvm::Value* cond_val = this->cond_expr->codegen(drv, depth + 1);
  assert(cond_val);
  cond_val = doubleToBoolean(drv, cond_val);

  // CFG
  llvm::Function *F = drv.llvmIRBuilder->GetInsertBlock()->getParent();
  llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(*drv.llvmContext, "then");
  llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(*drv.llvmContext, "else");
  llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(*drv.llvmContext, "ifexit");
  auto& bblist = F->getBasicBlockList();

  // Conditional branch
  drv.llvmIRBuilder->CreateCondBr(cond_val, thenBB, elseBB);
  
  // Then
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
  llvm::PHINode *PN = drv.llvmIRBuilder->CreatePHI(llvm::Type::getDoubleTy(*drv.llvmContext), 2, "if_tmp");

  PN->addIncoming(then_val, thenBB);
  PN->addIncoming(else_val, elseBB);
  return PN;
}


llvm::Value* CompositeExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Composite Expression", "", depth, this->getLocation());

  assert(this->current);
  llvm::Value* cur_val = this->current->codegen(drv, depth + 1);

  if (this->next) {
    return this->next->codegen(drv, depth + 1);
  } else {
    return cur_val;
  }
}

llvm::Value* ForExprAST::codegen(driver& drv, int depth)  {
  dbglog(drv, "For Expression", "", depth, this->getLocation());

  // CFG
  llvm::Function* F = drv.llvmIRBuilder->GetInsertBlock()->getParent();
  llvm::BasicBlock* header = llvm::BasicBlock::Create(*drv.llvmContext, "header", F);
  llvm::BasicBlock* body = llvm::BasicBlock::Create(*drv.llvmContext, "body", F);
  llvm::BasicBlock* exitBlock = llvm::BasicBlock::Create(*drv.llvmContext, "exitBlock", F);

  llvm::AllocaInst* exitValuePtr = createAllocaInEntryBlock(drv,  F, "exitValuePtr");

  createVar(drv, F, this->init_expr->getDestinationName(), this->getLocation());

  /* PREHEADER */

  // Initialize exit value to 0.
  drv.llvmIRBuilder->CreateStore(
    llvm::ConstantFP::get(*drv.llvmContext, llvm::APFloat(llvm::APFloat(0.0))),
    exitValuePtr
  );

  // Initialize induction variable
  this->init_expr->codegen(drv, depth + 1);

  drv.llvmIRBuilder->CreateBr(header);
  


  /* HEADER */
  drv.llvmIRBuilder->SetInsertPoint(header);

  llvm::Value* cond_val = this->cond_expr->codegen(drv, depth + 1);
  assert(cond_val);

  drv.llvmIRBuilder->CreateCondBr(
    doubleToBoolean(drv, cond_val), 
    body, 
    exitBlock
  );
  

  /* BODY */
  drv.llvmIRBuilder->SetInsertPoint(body);

  llvm::Value* body_val = this->body_expr->codegen(drv, depth + 1);
  assert(body_val);

  // Store the exit value.
  drv.llvmIRBuilder->CreateStore(body_val, exitValuePtr);

  // Increment the induction variable.
  this->step_expr->codegen(drv, depth + 1);

  drv.llvmIRBuilder->CreateBr(header);


  /* EXIT BLOCK */
  drv.llvmIRBuilder->SetInsertPoint(exitBlock);
  return drv.llvmIRBuilder->CreateLoad(exitValuePtr->getAllocatedType(), exitValuePtr);
}

llvm::Value* WhileExprAST::codegen(driver& drv, int depth)  {
  dbglog(drv, "While Expression", "", depth, this->getLocation());


  // CFG
  llvm::Function* F = drv.llvmIRBuilder->GetInsertBlock()->getParent();
  llvm::BasicBlock* header = llvm::BasicBlock::Create(*drv.llvmContext, "header", F);
  llvm::BasicBlock* body = llvm::BasicBlock::Create(*drv.llvmContext, "body", F);
  llvm::BasicBlock* exitBlock = llvm::BasicBlock::Create(*drv.llvmContext, "exitBlock", F);


  // Create a default exit value of 0.
  llvm::AllocaInst* exitValuePtr = createAllocaInEntryBlock(drv,  F, "exitValuePtr");


  // Preheader
  drv.llvmIRBuilder->CreateStore(
    llvm::ConstantFP::get(*drv.llvmContext, llvm::APFloat(llvm::APFloat(0.0))),
    exitValuePtr
  );
  drv.llvmIRBuilder->CreateBr(header);
  

  // Header
  drv.llvmIRBuilder->SetInsertPoint(header);

  llvm::Value* cond_val = this->cond_expr->codegen(drv, depth + 1);
  assert(cond_val);
  cond_val = doubleToBoolean(drv, cond_val);

  drv.llvmIRBuilder->CreateCondBr(cond_val, body, exitBlock);
  

  // Body
  drv.llvmIRBuilder->SetInsertPoint(body);

  llvm::Value* body_val = this->body_expr->codegen(drv, depth + 1);
  assert(body_val);

  drv.llvmIRBuilder->CreateStore(body_val, exitValuePtr);
  drv.llvmIRBuilder->CreateBr(header);


  // Exit block
  drv.llvmIRBuilder->SetInsertPoint(exitBlock);
  return drv.llvmIRBuilder->CreateLoad(exitValuePtr->getAllocatedType(), exitValuePtr);
}

llvm::Value* AssignmentExprAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Assignment", this->id_name, depth, this->getLocation());
  assert(this->value_expr);

  llvm::Value* value = this->value_expr->codegen(drv, depth);
  assert(value);

  drv.llvmIRBuilder->CreateStore(value, getVar(drv, this->getLocation(), this->id_name));
  return value;
}

llvm::Value* VarExprAST::codegen(driver& drv, int depth) {
  if (this->declarations.size() > 0) {

    std::string varnames = this->declarations[0].first;
    for (auto &decl : this->declarations)
      varnames.append(", " + decl.first);
    dbglog(drv, "VarExpr", varnames, depth, this->getLocation());

    llvm::Function* F = drv.llvmIRBuilder->GetInsertBlock()->getParent();

    for (auto &decl : this->declarations) {
      llvm::Value* initValue = decl.second->codegen(drv, depth + 1);
      assert(initValue);

      createVar(drv, F, decl.first, this->getLocation(), initValue);
    }
  } else {
    dbglog(drv, "VarExpr", "", depth, this->getLocation());
  }

  return this->body->codegen(drv, depth + 1);
}



llvm::Function* FunctionPrototypeAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Function prototype", this->getName(), depth, this->getLocation());

  std::vector<llvm::Type *> types(this->argsNames.size(), llvm::Type::getDoubleTy(*drv.llvmContext));

  llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*drv.llvmContext), types, false);

  llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, this->name, drv.llvmModule.get());

  unsigned i = 0;
  for (auto &arg : F->args())
    arg.setName(this->argsNames[i++]);

  return F;
}
  
llvm::Value* FunctionAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Function", this->prototype->getName(), depth, this->getLocation());

  llvm::Function* F = drv.llvmModule->getFunction(this->prototype->getName());
  if (!F)
    F = this->prototype->codegen(drv, depth + 1);

  assert(F);

  if (!F->empty())
    error(this->getLocation(), "Redefinition of function " + std::string(F->getName()));

  llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(*drv.llvmContext, "entry", F);
  drv.llvmIRBuilder->SetInsertPoint(entryBB);

  drv.namedPointers.clear();
  for (auto &arg : F->args())
    createVar(drv, F, std::string(arg.getName()), this->getLocation(), &arg);

  llvm::Value *returnValue = this->body->codegen(drv, depth + 1);
  assert(returnValue);
  drv.llvmIRBuilder->CreateRet(returnValue);

  llvm::verifyFunction(*F);

  return F;
}

llvm::Value* SequenceAST::codegen(driver& drv, int depth) {
  dbglog(drv, "Sequence", "", depth, this->getLocation());

  if (this->current) {
    if (ExprAST* expr = dynamic_cast<ExprAST*>(this->current.get())) {
      // We have a top level expression - replace it with an anonymous function

      // We need to get a new unique_ptr to ExprAST...
      this->current.release();
      std::unique_ptr<ExprAST> expr_ptr = std::unique_ptr<ExprAST>(expr);
      
      // Create the anon function and replace the current node with it
      const std::string anon_fun_name = "__anon_expr" + std::to_string(drv.get_unique_id());
      auto anon_fun_proto = std::make_unique<FunctionPrototypeAST>(anon_fun_name, std::vector<std::string>(), expr_ptr->getLocation());
      this->current = std::make_unique<FunctionAST>(std::move(anon_fun_proto), std::move(expr_ptr), expr_ptr->getLocation());
    }

    this->current->codegen(drv, depth + 1);
  }

  if (this->next)
    this->next->codegen(drv, depth + 1);
  
  return nullptr;
}