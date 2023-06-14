%option noyywrap nounput noinput batch debug

%{ /* -*- C++ -*- */

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring> // strerror
#include <string>
#include "driver.hh"
#include "parser.hh"

yy::parser::symbol_type parseNumber(const std::string &s, const yy::parser::location_type& loc);
yy::parser::symbol_type parseKeyword(const std::string &s, const yy::parser::location_type& loc);

// Code run each time a pattern is matched.
# define YY_USER_ACTION  loc.columns (yyleng);
%}

blank [ \t\r]
fpnum   [0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?
fixnum  (0|[1-9][0-9]*)\.?[0-9]*
num     {fpnum}|{fixnum}
id [a-zA-Z][a-zA-Z0-9]*


%%


%{
  // A handy shortcut to the location held by the driver.
  yy::location& loc = drv.location;
  // Code run each time yylex is called.
  loc.step ();
%}

{blank}+   loc.step();
\n+        loc.lines(yyleng); loc.step();

"-"        return yy::parser::make_MINUS(loc);
"+"        return yy::parser::make_PLUS(loc);
"*"        return yy::parser::make_STAR(loc);
"/"        return yy::parser::make_SLASH(loc);
"("        return yy::parser::make_LPAREN(loc);
")"        return yy::parser::make_RPAREN(loc);
";"        return yy::parser::make_SEMICOLON(loc);
":"        return yy::parser::make_COLON(loc);
","        return yy::parser::make_COMMA(loc);
">="       return yy::parser::make_GTE(loc);
">"        return yy::parser::make_GT(loc);
"<="       return yy::parser::make_LTE(loc);
"<"        return yy::parser::make_LT(loc);
"=="       return yy::parser::make_EQ(loc);
"!="       return yy::parser::make_NEQ(loc);


{num}      return parseNumber(yytext, loc);
{id}       return parseKeyword(yytext, loc);

<<EOF>>    return yy::parser::make_EOF(loc);
.          {
  throw yy::parser::syntax_error(loc, "Invalid character: " + std::string(yytext));
}


%%


yy::parser::symbol_type parseNumber(const std::string &s, const yy::parser::location_type& loc)
{
  try {
    return yy::parser::make_NUMBER(std::stod(s), loc);
  } 
  catch (std::invalid_argument) {
    throw yy::parser::syntax_error(loc, "Number is in an invalid format: " + s);
  }
  catch (std::out_of_range) {
    throw yy::parser::syntax_error(loc, "Number is out of range: " + s);
  }
}

yy::parser::symbol_type parseKeyword(const std::string &s, const yy::location& loc)  {
       if (s == "def")    return yy::parser::make_DEF(loc);
  else if (s == "extern") return yy::parser::make_EXTERN(loc);
  else if (s == "if")     return yy::parser::make_IF(loc);
  else if (s == "then")     return yy::parser::make_THEN(loc);
  else if (s == "else")   return yy::parser::make_ELSE(loc);
  else if (s == "end")    return yy::parser::make_END(loc);
  else
    return yy::parser::make_IDENTIFIER (yytext, loc);
}

void driver::scan_begin()
{
  yy_flex_debug = trace_scanning;
  if (file.empty() || file == "-")
    yyin = stdin;
  else if (!(yyin = fopen(file.c_str(), "r")))
  {
    std::cerr << "cannot open " << file << ": " << strerror(errno) << '\n';
    exit(EXIT_FAILURE);
  }
}


void driver::scan_end()
{
  fclose (yyin);
}
