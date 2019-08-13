%require "3.3"
%debug
%defines
%language "C++"
%locations
%define api.parser.class {PyParser}
%define api.value.type variant
%parse-param {PyLexer& scanner}
%parse-param {const char* filename}
%parse-param {BlockExprAST** rootBlock}

%code requires{
	class PyLexer;
}

%{
#include <vector>
#include <stdlib.h>
#include "../src/pyparser.h"

#undef yylex
#define yylex scanner.yylex

#define getloc(b, e) Location{filename, b.begin.line, b.begin.column, e.end.line, e.end.column}
%}

%token NEWLINE INDENT OUTDENT
%token PARAMS ELLIPSIS
%token EQ NE LEQ GEQ
%token<const char*> LITERAL ID PLCHLD2
%token<char> PLCHLD1
%token<int> PARAM
%type<BlockExprAST*> block
%type<std::vector<ExprAST*>*> stmt_string
%type<ExprListAST*> expr_string expr_list expr_lists
%type<ExprAST*> id_or_plchld expr

%start file
%right '='
%left EQ NE
%left '<' LEQ '>' GEQ
%left '+' '-'
%left '*' '/' '%'
%left '.' CALL SUBSCRIPT TPLT

%%

file
	: block { *rootBlock = $1; }
;

block
	: stmt_string { $$ = new BlockExprAST(getloc(@1, @1), $1); }
	| stmt_string expr_string { $1->insert($1->end(), $2->cbegin(), $2->cend()); $1->push_back(new StopExprAST(getloc(@2, @2))); $$ = new BlockExprAST(getloc(@1, @1), $1); }
;

stmt_string
	: expr_string NEWLINE { $$ = new std::vector<ExprAST*>($1->cbegin(), $1->cend()); $$->push_back(new StopExprAST(getloc(@2, @2))); }
	| expr_string ':' NEWLINE INDENT block OUTDENT { $$ = new std::vector<ExprAST*>($1->cbegin(), $1->cend()); $$->push_back($5); }
	| stmt_string expr_string NEWLINE { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); $$->push_back(new StopExprAST(getloc(@3, @3))); }
	| stmt_string expr_string ':' NEWLINE INDENT block OUTDENT { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); $$->push_back($6); }
;

expr_string
	: expr { $$ = new ExprListAST('\0'); $$->exprs.push_back($1); }
	| expr_string expr { ($$ = $1)->exprs.push_back($2); }
	| expr_string ELLIPSIS { ($$ = $1)->exprs.back() = new EllipsisExprAST(getloc(@2, @2), $1->exprs.back()); }
;

expr_list
	: expr_string { $$ = new ExprListAST(','); $$->exprs.push_back($1); }
	| expr_list ',' expr_string { ($$ = $1)->exprs.push_back($3); }
	| expr_list ',' ELLIPSIS { ($$ = $1)->exprs.back() = new EllipsisExprAST(getloc(@3, @3), $1->exprs.back()); }
;

expr_lists
	: expr_list { $$ = new ExprListAST(';'); $$->exprs.push_back($1); }
	| expr_lists ';' expr_list { ($$ = $1)->exprs.push_back($3); }
	| expr_lists ';' ELLIPSIS { ($$ = $1)->exprs.back() = new EllipsisExprAST(getloc(@3, @3), $1->exprs.back()); }
;

id_or_plchld
	: ID { $$ = new IdExprAST(getloc(@1, @1), $1); }
	| PLCHLD1 { $$ = new PlchldExprAST(getloc(@1, @1), $1); }
	| PLCHLD2 { $$ = new PlchldExprAST(getloc(@1, @1), $1); }
;

expr
	: LITERAL { $$ = new LiteralExprAST(getloc(@1, @1), $1); }
	| PARAM { $$ = new ParamExprAST(getloc(@1, @1), $1); }
	| PARAMS '[' expr ']' { $$ = new ParamExprAST(getloc(@1, @4), $3); }
	| id_or_plchld { $$ = $1; }
	| id_or_plchld '(' ')' %prec CALL { $$ = new CallExprAST(getloc(@1, @3), $1, new ExprListAST(';')); }
	| id_or_plchld '(' expr_lists ')' %prec CALL { $$ = new CallExprAST(getloc(@1, @4), $1, $3); }
	| '{' block '}' { $$ = $2; }
	| expr '[' ']' %prec SUBSCRIPT { $$ = new SubscrExprAST(getloc(@1, @3), $1, new ExprListAST(';')); }
	| expr '[' expr_lists ']' %prec SUBSCRIPT { $$ = new SubscrExprAST(getloc(@1, @4), $1, $3); }
	| id_or_plchld '<' '>' %prec TPLT { $$ = new TpltExprAST(getloc(@1, @3), $1, new ExprListAST(';')); }
	| id_or_plchld '<' expr_lists '>' %prec TPLT { $$ = new TpltExprAST(getloc(@1, @4), $1, $3); }
	| expr '=' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'=', "=", $1, $3); }
	| expr '.' id_or_plchld { $$ = new BinOpExprAST(getloc(@1, @3), (int)'.', ".", $1, $3); }

	| expr '+' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'+', "+", $1, $3); }
	| expr '-' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'-', "-", $1, $3); }
	| expr '*' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'*', "*", $1, $3); }
	| expr '/' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'/', "/", $1, $3); }
	| expr EQ expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::EQ, "==", $1, $3); }
	| expr NE expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::NE, "!=", $1, $3); }
	| expr LEQ expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::LEQ, "<=", $1, $3); }
	| expr GEQ expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::GEQ, ">=", $1, $3); }
;

%%

void yy::PyParser::error( const location_type &l, const std::string &err_message )
{
	std::cerr << "Error: " << err_message << " at " << l << "\n"; //TODO: throw syntax error
}