%skeleton "lalr1.cc"
%require "3.7.5"
%defines

%define api.value.type variant
%define parse.assert

%code requires {
  #include "ast.hh"
}

%param { driver& drv }

%locations

%define parse.trace
%define parse.error detailed
%define parse.lac full

%code {
  #include "driver.hh"
}

%define api.token.raw
%define api.token.constructor
%define api.token.prefix {TOK_}
%token
 EOF  0  "end of file"
 SEMICOLON ";"
 COMMA ","
 MINUS "-"
 PLUS "+"
 STAR "*"
 SLASH "/"
 LPAREN "("
 RPAREN ")"
 EXTERN "extern"
 DEF "def"
 IF "if"
 THEN "then"
 ELSE "else"
 END "end"
;

%token <std::string> IDENTIFIER "id"
%token <double> NUMBER "number"
%nterm <std::unique_ptr<SequenceAST>> program
%nterm <std::unique_ptr<RootAST>> top
%nterm <std::unique_ptr<FunctionAST>> fun_def
%nterm <std::unique_ptr<FunctionPrototypeAST>> fun_proto
%nterm <std::vector<std::string>> fun_proto_params
%nterm <std::unique_ptr<FunctionPrototypeAST>> fun_ext
%nterm <std::unique_ptr<ExprAST>> expr
%nterm <std::unique_ptr<ExprAST>> identifier_expr
%nterm <std::vector<std::unique_ptr<ExprAST>>> opt_expr_list
%nterm <std::vector<std::unique_ptr<ExprAST>>> expr_list
%nterm <std::unique_ptr<IfExprAST>> ifexpr

%%

%start axiom;
axiom: 
  program { drv.root = std::move($1); }

program:
  %empty { $$ = nullptr; }
  | top ";" program { $$ = std::make_unique<SequenceAST>(std::move($1), std::move($3)); }

top:
  %empty { $$ = nullptr; }
  | fun_def { $$ = std::move($1); }
  | fun_ext { $$ = std::move($1); }
  | expr { $$ = std::move($1); }

fun_def:
  "def" fun_proto expr { $$ = std::make_unique<FunctionAST>(std::move($2), std::move($3)); }

fun_proto:
  "id" "(" fun_proto_params ")" { $$ = std::make_unique<FunctionPrototypeAST>(std::move($1), std::move($3)); }

fun_proto_params:
  %empty { $$ = std::vector<std::string>(); }
  | "id" fun_proto_params { $2.insert($2.begin(), std::move($1)); $$ = std::move($2); }

fun_ext:
  "extern" fun_proto { $$ = std::move($2); }

%left "+" "-";
%left "*" "/";

expr:
  "number" { $$ = std::make_unique<NumberExprAST>($1); }
  | expr "+" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Add, std::move($1), std::move($3)); }
  | expr "-" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Sub, std::move($1), std::move($3)); }
  | expr "*" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Mul, std::move($1), std::move($3)); }
  | expr "/" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Div, std::move($1), std::move($3)); }
  | identifier_expr { $$ = std::move($1); }
  | "(" expr ")" { $$ = std::move($2); }
  | ifexpr { $$ = std::move($1); }

ifexpr:
  "if" expr "then" expr "else" expr "end" { $$ = std::make_unique<IfExprAST>(std::move($2), std::move($4), std::move($6)); }

identifier_expr:
  "id" { $$ = std::make_unique<VariableExprAST>(std::move($1)); }
  | "id" "(" opt_expr_list ")" { $$ = std::make_unique<CallExprAST>(std::move($1), std::move($3)); }

opt_expr_list:
  %empty { $$ = std::vector<std::unique_ptr<ExprAST>>(); }
  | expr_list { $$ = std::move($1); }

expr_list:
  expr  { auto v = std::vector<std::unique_ptr<ExprAST>>(); v.push_back(std::move($1)); $$ = std::move(v); }
  | expr "," expr_list { $3.insert($3.begin(), std::move($1)); $$ = std::move($3);  }


%%


void yy::parser::error (const location_type& l, const std::string& m)
{
  std::cerr << l << ": " << m << '\n';
}