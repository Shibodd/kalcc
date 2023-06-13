#ifndef AST_HH
#define AST_HH

#include <string>
#include <memory>
#include <vector>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>

class driver;

class RootAST {
  public:
    RootAST() {};
    virtual llvm::Value* codegen(driver &drv, int depth) { return nullptr; };
    virtual ~RootAST() = default;
};


/* EXPRESSIONS */


class ExprAST : public RootAST { };


class VariableExprAST : public ExprAST {
  std::string name;

public:
  VariableExprAST(const std::string &name);
  llvm::Value* codegen(driver& drv, int depth) override;
};


class NumberExprAST : public ExprAST {
  double value;

public:
  NumberExprAST(double value);

  llvm::Value* codegen(driver& drv, int depth) override;
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
      std::unique_ptr<ExprAST> rhs);

  llvm::Value* codegen(driver& drv, int depth) override;
};


class CallExprAST : public ExprAST {
  std::string callee;
  std::vector<std::unique_ptr<ExprAST>> args;

public:
  CallExprAST(const std::string &callee,
              std::vector<std::unique_ptr<ExprAST>> args);
    
  llvm::Value* codegen(driver& drv, int depth) override;
};


/* STATEMENTS */


class SequenceAST : public RootAST {
  std::unique_ptr<RootAST> current;
  std::unique_ptr<RootAST> next;
public:
  SequenceAST(std::unique_ptr<RootAST> current, std::unique_ptr<RootAST> next);
  
  llvm::Value* codegen(driver& drv, int depth) override;
};


class FunctionPrototypeAST : public RootAST {
  std::string name;
  std::vector<std::string> argsNames;

public:
  FunctionPrototypeAST(const std::string &name, std::vector<std::string> argsNames);

  llvm::Function* codegen(driver& drv, int depth) override;
  const std::string &getName() const;
};


class FunctionAST : public RootAST {
  std::unique_ptr<FunctionPrototypeAST> prototype;
  std::unique_ptr<ExprAST> body;

public:
  FunctionAST(std::unique_ptr<FunctionPrototypeAST> prototype,
              std::unique_ptr<ExprAST> body);

  llvm::Value* codegen(driver& drv, int depth) override;
};

#endif // !AST_HH