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
#include <fstream>
#include "../src/pyparser.h"

#undef yylex
#define yylex scanner.yylex

#define getloc(b, e) Location{filename, b.begin.line, b.begin.column, e.end.line, e.end.column}
%}

%token ELLIPSIS
%token EQ NE GEQ LEQ RS LS AWT NEW DM SR INC DEC
%token NEWLINE INDENT OUTDENT
%token<const char*> LITERAL ID PLCHLD2
%token<char> PLCHLD1
%token<int> PARAM
%type<std::vector<ExprAST*>*> block stmt_string
%type<ExprListAST*> expr_string optional_expr_string expr_list optional_expr_list
%type<ExprAST*> id_or_plchld expr

%start file
%right '=' '?' ':'
%left OR
%left AND
%left '&'
%left EQ NE
%left GEQ LEQ RS LS '>' '<'
%left '+' '-'
%left '*' '/' '%'
%right AWT NEW REF PREINC
%left '.' CALL SUBSCRIPT TPLT DM POSTINC
%left SR
%left ENC

%%

file
	: stmt_string { *rootBlock = new BlockExprAST(getloc(@1, @1), $1); }
;

block
	: INDENT stmt_string OUTDENT { $$ = $2; }
;

stmt_string
	: %empty { $$ = new std::vector<ExprAST*>(); }
	| stmt_string optional_expr_string NEWLINE { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); $$->push_back(new StopExprAST(getloc(@3, @3))); }
	| stmt_string expr_string ':' NEWLINE block { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); $$->push_back(new BlockExprAST(getloc(@5, @5), $5)); }
;

expr_string
	: expr_list { $$ = new ExprListAST('\0'); $$->exprs.push_back($1); }
	| expr_string expr_list { ($$ = $1)->exprs.push_back($2); }
	| expr_string ELLIPSIS { ($$ = $1)->exprs.back() = new EllipsisExprAST(getloc(@2, @2), $1->exprs.back()); }
;

optional_expr_string
	: %empty { $$ = new ExprListAST('\0'); }
	| expr_string { $$ = $1; }
;

expr_list
	: expr { $$ = new ExprListAST(','); $$->exprs.push_back($1); }
	| expr_list ',' expr { ($$ = $1)->exprs.push_back($3); }
	| expr_list ',' ELLIPSIS { ($$ = $1)->exprs.back() = new EllipsisExprAST(getloc(@3, @3), $1->exprs.back()); }
;

optional_expr_list
	: %empty { $$ = new ExprListAST(','); }
	| expr_list { $$ = $1; }
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
	| '(' optional_expr_list ')' %prec ENC { $$ = new EncOpExprAST(getloc(@1, @3), (int)'(', "(", ")", $2); }
	| '[' optional_expr_list ']' %prec ENC { $$ = new EncOpExprAST(getloc(@1, @3), (int)'[', "[", "]", $2); }

	// Parameterized expressions
	| expr '(' optional_expr_list ')' %prec CALL { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'(', "(", ")", $1, $3); }
	| expr '[' optional_expr_list ']' %prec SUBSCRIPT { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'[', "[", "]", $1, $3); }

	// Binary operators
	| expr '=' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'=', "=", $1, $3); }
	| expr '.' id_or_plchld { $$ = new BinOpExprAST(getloc(@1, @3), (int)'.', ".", $1, $3); }
	| expr '.' ELLIPSIS { $$ = new VarBinOpExprAST(getloc(@1, @3), (int)'.', ".", $1); }
	| expr DM id_or_plchld { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::DM, "->", $1, $3); }
	| expr SR id_or_plchld { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::SR, "::", $1, $3); }
	| expr '+' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'+', "+", $1, $3); }
	| expr '-' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'-', "-", $1, $3); }
	| expr '*' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'*', "*", $1, $3); }
	| expr '/' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'/', "/", $1, $3); }
	| expr '&' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'&', "&", $1, $3); }
	| expr EQ expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::EQ, "==", $1, $3); }
	| expr NE expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::NE, "!=", $1, $3); }
	| expr GEQ expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::GEQ, ">=", $1, $3); }
	| expr LEQ expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::LEQ, "<=", $1, $3); }
	| expr '>' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'>', ">", $1, $3); }
	| expr '<' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'<', "<", $1, $3); }
	| expr RS expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::RS, ">>", $1, $3); }
	| expr LS expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::LS, "<<", $1, $3); }
	| expr AND expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::AND, "&&", $1, $3); }
	| expr OR expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::OR, "||", $1, $3); }

	// Unary operators
	| '*' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'*', "*", $2); }
	| expr '*' { $$ = new PostfixExprAST(getloc(@1, @2), (int)'*', "*", $1); }
	| '!' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'!', "!", $2); }
	| '&' expr %prec REF { $$ = new PrefixExprAST(getloc(@1, @2), (int)'&', "&", $2); }
	| AWT expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::AWT, "await", $2); }
	| NEW expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::NEW, "new", $2); }
	| INC id_or_plchld %prec PREINC { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::INC, "++", $2); }
	| DEC id_or_plchld %prec PREINC { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::DEC, "--", $2); }
	| id_or_plchld INC %prec POSTINC { $$ = new PostfixExprAST(getloc(@1, @2), (int)token::INC, "++", $1); }
	| id_or_plchld DEC %prec POSTINC { $$ = new PostfixExprAST(getloc(@1, @2), (int)token::DEC, "--", $1); }
;

%%

void yy::PyParser::error( const location_type &l, const std::string &err_message )
{
	std::cerr << "Error: " << err_message << " at " << l << "\n"; //TODO: throw syntax error
}

BlockExprAST* parsePythonFile(const char* filename)
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
	PyLexer lexer(&in, &std::cout);
	yy::PyParser parser(lexer, filename, &rootBlock);
	parser.parse();

	// Close source file
	in.close();

	return rootBlock;
}