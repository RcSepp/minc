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
%token EQ NE GEQ LEQ RS LS AWT NEW DM
%token NEWLINE INDENT OUTDENT
%token<const char*> LITERAL ID PLCHLD2
%token<char> PLCHLD1
%token<int> PARAM
%type<BlockExprAST*> block
%type<std::vector<ExprAST*>*> stmt_string
%type<std::pair<ExprListAST*, bool>> expr_string
%type<ExprListAST*> optional_expr_string expr_list optional_expr_list expr_idx
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
%right AWT NEW REF
%left '.' CALL SUBSCRIPT TPLT DM
%left ENC

%%

file
	: stmt_string { *rootBlock = new BlockExprAST(getloc(@1, @1), $1); }
	| %empty { *rootBlock = new BlockExprAST({0}, new std::vector<ExprAST*>()); } // Empty file
;

block
	: INDENT stmt_string OUTDENT { $$ = new BlockExprAST(getloc(@1, @3), $2); }
;

stmt_string
	: expr_string NEWLINE { $$ = &$1.first->exprs; $$->push_back(new StopExprAST(getloc(@2, @2))); }
	| expr_string ':' NEWLINE block { $$ = &$1.first->exprs; $$->push_back($4); }
	| stmt_string NEWLINE { $$ = $1; } // Blank line
	| stmt_string expr_string NEWLINE { ($$ = $1)->insert($1->end(), $2.first->cbegin(), $2.first->cend()); $$->push_back(new StopExprAST(getloc(@3, @3))); }
	| stmt_string expr_string ':' NEWLINE block { ($$ = $1)->insert($1->end(), $2.first->cbegin(), $2.first->cend()); $$->push_back($5); }
;

expr_string
	: expr					{
								ExprListAST* expr_string = new ExprListAST('\0');
								expr_string->exprs.push_back($1);
								$$ = std::make_pair(expr_string, $1->exprtype == ExprAST::ExprType::LIST);
							}
	| expr_string expr		{
								ExprListAST* expr_string = $1.first;
								if (expr_string->exprs.back()->exprtype != ExprAST::ExprType::LIST || $1.second == false) // If expr_string.back() is single expression or expr_string ended without a trailing ',', ...
									expr_string->exprs.push_back($2); // Append expr to expr_string
								else if ($2->exprtype != ExprAST::ExprType::LIST) // If expr_string.back() is a list, but expr is a single expression ...
									((ExprListAST*)expr_string->exprs.back())->push_back($2); // Append expr to expr_string.back()
								else // If both expr_string.back() and expr are lists ...
								{
									 // Merge expr into expr_string.back()
									((ExprListAST*)expr_string->exprs.back())->push_back(((ExprListAST*)$2)->exprs.front());
									delete $2;
								}
								$$ = std::make_pair(expr_string, $2->exprtype == ExprAST::ExprType::LIST);
							}
	| expr_string ELLIPSIS	{
								ExprListAST* expr_string = $1.first;
								if (expr_string->exprs.back()->exprtype == ExprAST::ExprType::ELLIPSIS)
									{} // Swallow subsequent ellipses
								else if (expr_string->exprs.back()->exprtype != ExprAST::ExprType::LIST || $1.second == false)
									expr_string->exprs.back() = new EllipsisExprAST(getloc(@2, @2), expr_string->exprs.back());
								else
									((ExprListAST*)expr_string->exprs.back())->exprs.back() = new EllipsisExprAST(getloc(@2, @2), ((ExprListAST*)expr_string->exprs.back())->exprs.back());
								$$ = std::make_pair(expr_string, false);
							}
	| expr_string ':' expr	{
								ExprListAST* expr_string = $1.first;
								const Location& loc = Location{filename, expr_string->exprs.back()->loc.begin_line, expr_string->exprs.back()->loc.begin_col, @3.end.line, @3.end.column};
								expr_string->exprs.back() = new BinOpExprAST(loc, (int)':', ":", expr_string->exprs.back(), $3);
								$$ = std::make_pair(expr_string, $3->exprtype == ExprAST::ExprType::LIST);
							}
;

optional_expr_string
	: %empty { $$ = new ExprListAST('\0'); }
	| expr_string { $$ = $1.first; }
;

expr_list
	: expr { $$ = new ExprListAST(','); $$->exprs.push_back($1); }
	| expr_list expr { ($$ = $1)->exprs.push_back($2); }
	| expr_list ELLIPSIS { ($$ = $1)->exprs.back() = new EllipsisExprAST(getloc(@2, @2), $1->exprs.back()); }
;

optional_expr_list
	: %empty { $$ = new ExprListAST(','); }
	| expr_list { $$ = $1; }
;

expr_idx
	: optional_expr_list { $$ = new ExprListAST(':'); $$->exprs.push_back($1); }
	| ':' optional_expr_list { $$ = new ExprListAST(':'); $$->exprs.push_back(new ExprListAST('\0')); $$->exprs.push_back($2); }
	| expr_idx ':' optional_expr_list { ($$ = $1)->exprs.push_back($3); }
	| expr_idx ':' ELLIPSIS { ($$ = $1)->exprs.back() = new EllipsisExprAST(getloc(@3, @3), $1->exprs.back()); }
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
	| '(' optional_expr_string ')' %prec ENC { $$ = new EncOpExprAST(getloc(@1, @3), (int)'(', "(", ")", $2); }
	| '[' expr_idx ']' %prec ENC { $$ = new EncOpExprAST(getloc(@1, @3), (int)'[', "[", "]", $2); }
	| '{' optional_expr_string '}' %prec ENC { $$ = new EncOpExprAST(getloc(@1, @3), (int)'{', "{", "}", $2); }

	// Parameterized expressions
	| expr '(' optional_expr_string ')' %prec CALL { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'(', "(", ")", $1, $3); }
	| expr '[' expr_idx ']' %prec SUBSCRIPT { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'[', "[", "]", $1, $3); }

	// Binary operators
	| expr '=' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'=', "=", $1, $3); }
	| expr '.' id_or_plchld { $$ = new BinOpExprAST(getloc(@1, @3), (int)'.', ".", $1, $3); }
	| expr '.' ELLIPSIS { $$ = new VarBinOpExprAST(getloc(@1, @3), (int)'.', ".", $1); }
	| expr DM id_or_plchld { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::DM, "->", $1, $3); }
	| expr '+' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'+', "+", $1, $3); }
	| expr '-' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'-', "-", $1, $3); }
	| expr '*' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'*', "*", $1, $3); }
	| expr '/' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'/', "/", $1, $3); }
	| expr '%' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'%', "%", $1, $3); }
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
	| expr ',' { $$ = new ExprListAST(',', std::vector<ExprAST*>(1, $1)); }
	| '+' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'+', "+", $2); } //TODO: Precedence
	| expr '+' { $$ = new PostfixExprAST(getloc(@1, @2), (int)'+', "+", $1); } //TODO: Precedence
	| '-' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'-', "-", $2); } //TODO: Precedence
	| expr '-' { $$ = new PostfixExprAST(getloc(@1, @2), (int)'-', "-", $1); } //TODO: Precedence
	| '*' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'*', "*", $2); }
	| expr '*' { $$ = new PostfixExprAST(getloc(@1, @2), (int)'*', "*", $1); }
	| '!' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'!', "!", $2); }
	| '&' expr %prec REF { $$ = new PrefixExprAST(getloc(@1, @2), (int)'&', "&", $2); }
	| AWT expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::AWT, "await", $2); }
	| NEW expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::NEW, "new", $2); }
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

const std::vector<ExprAST*> parsePythonTplt(const char* tpltStr)
{
	// Parse tpltStr into tpltBlock
	std::stringstream ss(tpltStr);
	PyLexer lexer(ss, std::cout);
	BlockExprAST* tpltBlock;
	yy::PyParser parser(lexer, nullptr, &tpltBlock);
	if (parser.parse())
	{
		std::cerr << "\033[31merror:\033[0merror parsing template " << std::string(tpltStr) << '\n';
		return {};
	} //TODO: Throw CompileError instead:
		//throw CompileError("error parsing template " + std::string(tpltStr));

	// Remove trailing STOP expr
	if (tpltBlock->exprs->size())
		tpltBlock->exprs->pop_back();

	return *tpltBlock->exprs;
}