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

#define getloc(b, e) Location{filename, (unsigned)b.begin.line, (unsigned)b.begin.column, (unsigned)e.end.line, (unsigned)e.end.column}
%}

%token ELLIPSIS
%token EQ NE GEQ LEQ RS LS AND OR CADD CSUB CMUL CMML CDIV CMOD CAND COR CXOR CLS CRS CPOW CIDV AWT NEW DM
%token NEWLINE INDENT OUTDENT
%token<const char*> LITERAL ID PLCHLD2
%token<char> PLCHLD1
%token<int> PARAM
%type<BlockExprAST*> block
%type<std::vector<ExprAST*>*> stmt_string
%type<std::pair<ExprListAST*, bool>> stmt
%type<ExprListAST*> optional_stmt expr_string optional_expr_string expr_list expr_idx
%type<ExprAST*> id_or_plchld expr expr_or_single_expr_list

%start file
%right '=' '?' ':' CADD CSUB CMUL CMML CDIV CMOD CAND COR CXOR CLS CRS CPOW CIDV
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
	: INDENT stmt_string OUTDENT { $$ = new BlockExprAST(Location{filename, (unsigned)@1.begin.line, (unsigned)@1.begin.column, $2->back()->loc.end_line, $2->back()->loc.end_col}, $2); }
;

stmt_string
	: stmt NEWLINE { $$ = &$1.first->exprs; $$->push_back(new StopExprAST(Location{filename, (unsigned)@1.end.line, (unsigned)@1.end.column, (unsigned)@1.end.line, (unsigned)@1.end.column})); }
	| stmt ':' NEWLINE block { $$ = &$1.first->exprs; $$->push_back($4); }
	| stmt_string NEWLINE { $$ = $1; } // Blank line
	| stmt_string stmt NEWLINE { ($$ = $1)->insert($1->end(), $2.first->cbegin(), $2.first->cend()); $$->push_back(new StopExprAST(Location{filename, (unsigned)@2.end.line, (unsigned)@2.end.column, (unsigned)@2.end.line, (unsigned)@2.end.column})); }
	| stmt_string stmt ':' NEWLINE block { ($$ = $1)->insert($1->end(), $2.first->cbegin(), $2.first->cend()); $$->push_back($5); }
;

stmt
	: expr_or_single_expr_list	{
								ExprListAST* stmt = new ExprListAST('\0');
								stmt->exprs.push_back($1);
								$$ = std::make_pair(stmt, $1->exprtype == ExprAST::ExprType::LIST);
							}
	| stmt expr_or_single_expr_list	{
								ExprListAST* stmt = $1.first;
								if (stmt->exprs.back()->exprtype != ExprAST::ExprType::LIST || $1.second == false) // If stmt.back() is single expression or stmt ended without a trailing ',', ...
									stmt->exprs.push_back($2); // Append expr to stmt
								else if ($2->exprtype != ExprAST::ExprType::LIST) // If stmt.back() is a list, but expr is a single expression ...
									((ExprListAST*)stmt->exprs.back())->push_back($2); // Append expr to stmt.back()
								else // If both stmt.back() and expr are lists ...
								{
									 // Merge expr into stmt.back()
									((ExprListAST*)stmt->exprs.back())->push_back(((ExprListAST*)$2)->exprs.front());
									delete $2;
								}
								$$ = std::make_pair(stmt, $2->exprtype == ExprAST::ExprType::LIST);
							}
	| stmt ELLIPSIS	{
								ExprListAST* stmt = $1.first;
								if (stmt->exprs.back()->exprtype == ExprAST::ExprType::ELLIPSIS)
									{} // Swallow subsequent ellipses
								else if (stmt->exprs.back()->exprtype != ExprAST::ExprType::LIST || $1.second == false)
									stmt->exprs.back() = new EllipsisExprAST(getloc(@2, @2), stmt->exprs.back());
								else
									((ExprListAST*)stmt->exprs.back())->exprs.back() = new EllipsisExprAST(getloc(@2, @2), ((ExprListAST*)stmt->exprs.back())->exprs.back());
								$$ = std::make_pair(stmt, false);
							}
	| stmt ':' expr_or_single_expr_list	{
								ExprListAST* stmt = $1.first;
								const Location& loc = Location{filename, stmt->exprs.back()->loc.begin_line, stmt->exprs.back()->loc.begin_col, (unsigned)@3.end.line, (unsigned)@3.end.column};
								stmt->exprs.back() = new BinOpExprAST(loc, (int)':', ":", stmt->exprs.back(), $3);
								$$ = std::make_pair(stmt, $3->exprtype == ExprAST::ExprType::LIST);
							}
;

optional_stmt
	: %empty { $$ = new ExprListAST('\0'); }
	| stmt { $$ = $1.first; }
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

expr_idx
	: optional_expr_string { $$ = new ExprListAST(':'); $$->exprs.push_back($1); }
	| ':' optional_expr_string { $$ = new ExprListAST(':'); $$->exprs.push_back(new ExprListAST('\0')); $$->exprs.push_back($2); }
	| expr_idx ':' optional_expr_string { ($$ = $1)->exprs.push_back($3); }
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
	| '(' optional_stmt ')' %prec ENC { $$ = new EncOpExprAST(getloc(@1, @3), (int)'(', "(", ")", $2); }
	| '[' expr_idx ']' %prec ENC { $$ = new EncOpExprAST(getloc(@1, @3), (int)'[', "[", "]", $2); }
	| '{' optional_stmt '}' %prec ENC { $$ = new EncOpExprAST(getloc(@1, @3), (int)'{', "{", "}", $2); }

	// Parameterized expressions
	| expr '(' optional_stmt ')' %prec CALL { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'(', "(", ")", $1, $3); }
	| expr '[' expr_idx ']' %prec SUBSCRIPT { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'[', "[", "]", $1, $3); }

	// Binary operators
	| expr '=' expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)'=', "=", $1, $3); }
	| expr CADD expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CADD, "+=", $1, $3); }
	| expr CSUB expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CSUB, "-=", $1, $3); }
	| expr CMUL expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CMUL, "*=", $1, $3); }
	| expr CMML expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CMML, "@=", $1, $3); }
	| expr CDIV expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CDIV, "/=", $1, $3); }
	| expr CMOD expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CMOD, "%=", $1, $3); }
	| expr CAND expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CAND, "&=", $1, $3); }
	| expr COR expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::COR, "|=", $1, $3); }
	| expr CXOR expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CXOR, "^=", $1, $3); }
	| expr CLS expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CLS, "<<=", $1, $3); }
	| expr CRS expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CRS, ">>=", $1, $3); }
	| expr CPOW expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CPOW, "**=", $1, $3); }
	| expr CIDV expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::CIDV, "//=", $1, $3); }
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
	| '+' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'+', "+", $2); } //TODO: Precedence
	| '-' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'-', "-", $2); } //TODO: Precedence
	| '*' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'*', "*", $2); }
	| expr '*' { $$ = new PostfixExprAST(getloc(@1, @2), (int)'*', "*", $1); }
	| '!' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'!', "!", $2); }
	| '&' expr %prec REF { $$ = new PrefixExprAST(getloc(@1, @2), (int)'&', "&", $2); }
	| AWT expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::AWT, "await", $2); }
	| NEW expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::NEW, "new", $2); }
;

expr_or_single_expr_list
	: expr { $$ = $1; }
	| expr ',' { $$ = new ExprListAST(',', std::vector<ExprAST*>(1, $1)); }
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