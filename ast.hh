#ifndef AST_HH
#define AST_HH

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <llvm/IR/Value.h>

class driver;

class RootAST {
  public:
    RootAST() {};
    virtual llvm::Value* codegen(driver &drv, int depth) { return nullptr; };
    virtual ~RootAST() = default;
};


/* EXPRESSIONS */


class ExprAST : public RootAST {
};


class VariableExprAST : public ExprAST {
  std::string name;

public:
  VariableExprAST(const std::string &name) : name(name) {}
  llvm::Value* codegen(driver& drv, int depth) override {
    std::cerr << std::string(depth, ' ') << "Variable " << name << std::endl;
    return nullptr;
  }
};


class NumberExprAST : public ExprAST {
  double value;

public:
  NumberExprAST(double value) : value(value) {}

  llvm::Value* codegen(driver& drv, int depth) override {
    std::cerr << std::string(depth, ' ') << "Number " << value << std::endl;
    return nullptr;
  }
};

enum class BinaryOperator {
  Add,
  Sub,
  Mul,
  Div
};

class BinaryExprAST : public ExprAST {
  BinaryOperator op;
  std::unique_ptr<ExprAST> lhs, rhs;

public:
  BinaryExprAST(
      BinaryOperator op,
      std::unique_ptr<ExprAST> lhs,
      std::unique_ptr<ExprAST> rhs)
    : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

  llvm::Value* codegen(driver& drv, int depth) override {
    std::cerr << std::string(depth, ' ') << "Binexp " << (int)op << std::endl;
    if (lhs)
      lhs->codegen(drv, depth + 1);
    if (rhs)
      rhs->codegen(drv, depth + 1);
    return nullptr;
  }
};


class CallExprAST : public ExprAST {
  std::string callee;
  std::vector<std::unique_ptr<ExprAST>> args;

public:
  CallExprAST(const std::string &callee,
              std::vector<std::unique_ptr<ExprAST>> args)
      : callee(callee), args(std::move(args)) {}
    
  llvm::Value* codegen(driver& drv, int depth) override {
    std::cerr << std::string(depth, ' ') << "Callexpr " << callee << std::endl;
    for (int i = 0; i < args.size(); ++i)
      if (args[i])
        args[i]->codegen(drv, depth + 1);
    return nullptr;
  } 
};


/* STATEMENTS */


class SequenceAST : public RootAST {
  std::unique_ptr<RootAST> current;
  std::unique_ptr<RootAST> next;
public:
  SequenceAST(std::unique_ptr<RootAST> current, std::unique_ptr<RootAST> next) 
    : current(std::move(current)), next(std::move(next)) {}
  
  llvm::Value* codegen(driver& drv, int depth) override {
    std::cerr << std::string(depth, ' ') << "Seq " << std::endl;
    
    if (current)
      current->codegen(drv, depth + 1);

    if (next)
      next->codegen(drv, depth + 1);

    return nullptr;
  }
};


class FunctionPrototypeAST : public RootAST {
  std::string name;
  std::vector<std::string> argsNames;

public:
  FunctionPrototypeAST(const std::string &name, std::vector<std::string> argsNames)
      : name(name), argsNames(std::move(argsNames)) {}

  const std::string &getName() const { return name; }

  llvm::Value* codegen(driver& drv, int depth) override {
    std::cerr << std::string(depth, ' ') << "Fun proto " << name << std::endl;
    return nullptr;
  }
};


class FunctionAST : public RootAST {
  std::unique_ptr<FunctionPrototypeAST> prototype;
  std::unique_ptr<ExprAST> body;

public:
  FunctionAST(std::unique_ptr<FunctionPrototypeAST> prototype,
              std::unique_ptr<ExprAST> body)
      : prototype(std::move(prototype)), body(std::move(body)) {}

  llvm::Value* codegen(driver& drv, int depth) override  {
    std::cerr << std::string(depth, ' ') << "Fun " << std::endl;
    if (prototype)
      prototype->codegen(drv, depth + 1);
    if (body)
      body->codegen(drv, depth + 1);
    return nullptr;
  }
};

#endif // !AST_HH