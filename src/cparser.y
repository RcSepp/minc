%require "3.3"
%debug
%defines
%language "C++"
%locations
%define api.parser.class {CParser}
%define api.value.type variant
%parse-param {CLexer& scanner}
%parse-param {const char* filename}
%parse-param {BlockExprAST** rootBlock}

%code requires{
	class CLexer;
}

%{
#include <vector>
#include <stdlib.h>
#include <fstream>
#include "../src/cparser.h"

#undef yylex
#define yylex scanner.yylex

#define getloc(b, e) Location{filename, b.begin.line, b.begin.column, e.end.line, e.end.column}
%}

%token ELLIPSIS
%token EQ NE LEQ GEQ NEW DM SR
%token<const char*> LITERAL ID PLCHLD2
%token<char> PLCHLD1
%token<int> PARAM
%type<BlockExprAST*> block
%type<std::vector<ExprAST*>*> stmt_string
%type<ExprListAST*> expr_string expr_list expr_lists optional_expr_lists
%type<ExprAST*> id_or_plchld expr

%start file
%right '=' '?' ':'
%left OR
%left AND
%left '&'
%left EQ NE
%left '<' LEQ '>' GEQ
%left '+' '-'
%left '*' '/' '%'
%left NEW
%left '.' CALL SUBSCRIPT TPLT DM
%left SR
%left ENC

%%

file
	: block { *rootBlock = $1; }
;

block
	: stmt_string { $$ = new BlockExprAST(getloc(@1, @1), $1); }
;

stmt_string
	: %empty { $$ = new std::vector<ExprAST*>(); }
	| stmt_string expr_string ';' { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); $$->push_back(new StopExprAST(getloc(@3, @3))); }
	| stmt_string expr_string '{' block '}' { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); $$->push_back($4); }
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

optional_expr_lists
	: %empty { $$ = new ExprListAST(';'); }
	| expr_lists { $$ = $1; }
;

id_or_plchld
	: ID { $$ = new IdExprAST(getloc(@1, @1), $1); }
	| PLCHLD1 { $$ = new PlchldExprAST(getloc(@1, @1), $1); }
	| PLCHLD2 { $$ = new PlchldExprAST(getloc(@1, @1), $1); }
;

expr
	: LITERAL { $$ = new LiteralExprAST(getloc(@1, @1), $1); }
	| PARAM { $$ = new ParamExprAST(getloc(@1, @1), $1); }
	| id_or_plchld { $$ = $1; }

	// Enclosed expressions
	| '(' optional_expr_lists ')' %prec ENC { $$ = new EncOpExprAST(getloc(@1, @3), (int)'(', "(", ")", $2); }
	| '[' optional_expr_lists ']' %prec ENC { $$ = new EncOpExprAST(getloc(@1, @3), (int)'[', "[", "]", $2); }
	| '<' optional_expr_lists '>' %prec ENC { $$ = new EncOpExprAST(getloc(@1, @3), (int)'<', "<", ">", $2); }
	| '{' block '}' { $$ = $2; }

	// Parameterized expressions
	| expr '(' optional_expr_lists ')' %prec CALL { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'(', "(", ")", $1, $3); }
	| expr '[' optional_expr_lists ']' %prec SUBSCRIPT { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'[', "[", "]", $1, $3); }
	| id_or_plchld '<' optional_expr_lists '>' %prec TPLT { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'<', "<", ">", $1, $3); }

	// Tertiary operators
	| expr '?' expr ':' expr { $$ = new TerOpExprAST(getloc(@1, @5), (int)'?', (int)':', "?", ":", $1, $3, $5); }

	// Binary operators
	| expr '=' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'=', "=", $1, $3); }
	| expr '.' id_or_plchld { $$ = new BinOpExprAST(getloc(@1, @3), (int)'.', ".", $1, $3); }
	| expr DM id_or_plchld { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::DM, "->", $1, $3); }
	| expr SR id_or_plchld { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::SR, "::", $1, $3); }
	| expr '+' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'+', "+", $1, $3); }
	| expr '-' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'-', "-", $1, $3); }
	| expr '*' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'*', "*", $1, $3); }
	| expr '/' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'/', "/", $1, $3); }
	| expr '&' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'&', "&", $1, $3); }
	| expr EQ expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::EQ, "==", $1, $3); }
	| expr NE expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::NE, "!=", $1, $3); }
	| expr LEQ expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::LEQ, "<=", $1, $3); }
	| expr GEQ expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::GEQ, ">=", $1, $3); }
	| expr AND expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::AND, "&&", $1, $3); }
	| expr OR expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::OR, "||", $1, $3); }

	// Unary operators
	| '*' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'*', "*", $2); }
	| '!' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'!', "!", $2); }
	| expr ':' { $$ = new PostfixExprAST(getloc(@1, @2), (int)':', ":", $1); }
	| NEW expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::NEW, "new", $2); }
;

%%

void yy::CParser::error( const location_type &l, const std::string &err_message )
{
	std::cerr << "Error: " << err_message << " at " << l << "\n"; //TODO: throw syntax error
}

BlockExprAST* parseCFile(const char* filename)
{
	// Open source file
	std::ifstream in(filename);
	if (!in.good())
	{
		std::cerr << "\033[31merror:\033[0m " << std::string(filename) << ": No such file or directory\n";
		return nullptr;
	}

	// Parse file into rootBlock
	BlockExprAST* rootBlock;
	CLexer lexer(&in, &std::cout);
	yy::CParser parser(lexer, filename, &rootBlock);
	parser.parse();

	// Close source file
	in.close();

	return rootBlock;
}