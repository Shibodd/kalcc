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
 COLON ":"
 COMMA ","
 MINUS "-"
 PLUS "+"
 STAR "*"
 SLASH "/"
 LPAREN "("
 RPAREN ")"
 LT "<"
 LTE "<="
 GT ">"
 GTE ">="
 EQ "=="
 NEQ "!="
 ASSIGN "="
 FOR "for"
 WHILE "while"
 IN "in"
 EXTERN "extern"
 DEF "def"
 IF "if"
 THEN "then"
 ELSE "else"
 END "end"
 VAR "var"
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
%nterm <std::unique_ptr<ExprAST>> for_step

%nterm <std::pair<std::string, std::unique_ptr<ExprAST>>> varlist_var
%nterm <std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>>> varlist

%nterm <std::vector<std::unique_ptr<ExprAST>>> opt_expr_list
%nterm <std::vector<std::unique_ptr<ExprAST>>> expr_list

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

%right ":";
%nonassoc "=";
%nonassoc "<" "<=" ">" ">=" "==" "!=";
%left "+" "-";
%left "*" "/";
%left UMINUS;

expr:
  "number" { $$ = std::make_unique<NumberExprAST>($1); }
  | expr "+" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Add, std::move($1), std::move($3)); }
  | expr "-" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Sub, std::move($1), std::move($3)); }
  | expr "*" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Mul, std::move($1), std::move($3)); }
  | expr "/" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Div, std::move($1), std::move($3)); }
  | expr "<" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Lt, std::move($1), std::move($3)); }
  | expr "<=" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Lte, std::move($1), std::move($3)); }
  | expr ">" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Gt, std::move($1), std::move($3)); }
  | expr ">=" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Gte, std::move($1), std::move($3)); }
  | expr "==" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Eq, std::move($1), std::move($3)); }
  | expr "!=" expr { $$ = std::make_unique<BinaryExprAST>(BinaryOperator::Neq, std::move($1), std::move($3)); }
  | "id" "=" expr { $$ = std::make_unique<AssignmentExprAST>($1, std::move($3)); }
  | expr ":" expr { $$ = std::make_unique<CompositeExprAST>(std::move($1), std::move($3)); }
  | "-" expr %prec UMINUS { $$ = std::make_unique<UnaryExprAST>(UnaryOperator::NumericNeg, std::move($2)); }
  | identifier_expr { $$ = std::move($1); }
  | "(" expr ")" { $$ = std::move($2); }
  | "if" expr "then" expr "else" expr "end" { $$ = std::make_unique<IfExprAST>(std::move($2), std::move($4), std::move($6)); }
  | "for" "id" "=" expr "," expr for_step "in" expr "end"
      {
        $$ = std::make_unique<ForExprAST>(
          std::make_unique<AssignmentExprAST>($2, std::move($4)),
          std::move($6), 
          std::make_unique<AssignmentExprAST>(
            $2,
            std::make_unique<BinaryExprAST>(
              BinaryOperator::Add,
              std::make_unique<VariableExprAST>($2),
              std::move($7)
            )
          ),
          std::move($9)
        );
      }
  | "while" expr "in" expr "end" { $$ = std::make_unique<WhileExprAST>(std::move($2), std::move($4)); }
  | "var" varlist "in" expr "end" { $$ = std::make_unique<VarExprAST>(std::move($2), std::move($4)); }

varlist:
  varlist_var { std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> v; v.push_back(std::move($1)); $$ = std::move(v); }
  | varlist_var "," varlist { $3.insert($3.begin(), std::move($1)); $$ = std::move($3); }

varlist_var:
  "id" { $$ = std::make_pair($1, std::make_unique<NumberExprAST>(0)); }
  | "id" "=" expr { $$ = std::make_pair($1, std::move($3)); }

for_step:
  %empty { $$ = std::make_unique<NumberExprAST>(1.0); }
  | "," expr { $$ = std::move($2); }

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