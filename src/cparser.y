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

#define getloc(b, e) Location{filename, (unsigned)b.begin.line, (unsigned)b.begin.column, (unsigned)e.end.line, (unsigned)e.end.column}
%}

%token ELLIPSIS // ...
%token EQ NE GEQ LEQ GR LE DM SR INC DEC RS LS AND OR IDIV CADD CSUB CMUL CMML CDIV CMOD CAND COR CXOR CLS CRS CPOW CIDV // Operators
%token AWT NEW // Keywords
%token<const char*> LITERAL ID PLCHLD2
%token<char> PLCHLD1
%token<int> PARAM
%type<std::vector<ExprAST*>*> block stmt_string
%type<ListExprAST*> expr_string optional_expr_string expr_list expr_lists optional_expr_lists
%type<ExprAST*> id_or_plchld expr

%start file
%right '=' '?' ':'
%left OR
%left AND
%left '&'
%left EQ NE
%left GEQ LEQ GR LE
%left '+' '-'
%left '*' '/' '%'
%right AWT NEW REF PREINC
%left '.' CALL SUBSCRIPT TPLT DM POSTINC
%left SR
%left ENC

%%

file
	: block { *rootBlock = new BlockExprAST(getloc(@1, @1), $1); }
;

block
	: stmt_string { $$ = $1; }
;

stmt_string
	: %empty { $$ = new std::vector<ExprAST*>(); }
	| stmt_string optional_expr_string ';' { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); $$->push_back(new StopExprAST(getloc(@3, @3))); }
	| stmt_string expr_string '{' block '}' { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); $$->push_back(new BlockExprAST(getloc(@3, @5), $4)); }
;

expr_string
	: expr { $$ = new ListExprAST('\0'); $$->exprs.push_back($1); }
	| expr_string expr { ($$ = $1)->exprs.push_back($2); }
	| expr_string ELLIPSIS { ($$ = $1)->exprs.back() = new EllipsisExprAST(getloc(@2, @2), $1->exprs.back()); }
;

optional_expr_string
	: %empty { $$ = new ListExprAST('\0'); }
	| expr_string { $$ = $1; }
;

expr_list
	: expr_string { $$ = new ListExprAST(','); $$->exprs.push_back($1); }
	| expr_list ',' expr_string { ($$ = $1)->exprs.push_back($3); }
	| expr_list ',' ELLIPSIS { ($$ = $1)->exprs.back() = new EllipsisExprAST(getloc(@3, @3), $1->exprs.back()); }
;

expr_lists
	: expr_list { $$ = new ListExprAST(';'); $$->exprs.push_back($1); }
	| expr_lists ';' expr_list { ($$ = $1)->exprs.push_back($3); }
	| expr_lists ';' ELLIPSIS { ($$ = $1)->exprs.back() = new EllipsisExprAST(getloc(@3, @3), $1->exprs.back()); }
;

optional_expr_lists
	: %empty { $$ = new ListExprAST(';'); }
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
	| '{' block '}' { $$ = new BlockExprAST(getloc(@1, @3), $2); }

	// Parameterized expressions
	| expr '(' optional_expr_lists ')' %prec CALL { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'(', "(", ")", $1, $3); }
	| expr '[' optional_expr_lists ']' %prec SUBSCRIPT { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'[', "[", "]", $1, $3); }
	| id_or_plchld '<' optional_expr_lists '>' %prec TPLT { $$ = new ArgOpExprAST(getloc(@1, @4), (int)'<', "<", ">", $1, $3); }

	// Tertiary operators
	| expr '?' expr ':' expr { $$ = new TerOpExprAST(getloc(@1, @5), (int)'?', (int)':', "?", ":", $1, $3, $5); }

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
	| expr GR expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::GR, ">>", $1, $3); }
	| expr LE expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::LE, "<<", $1, $3); }
	| expr AND expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::AND, "&&", $1, $3); }
	| expr OR expr { $$ = new BinOpExprAST(getloc(@1, @3), (int)token::OR, "||", $1, $3); }

	// Unary operators
	| '*' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'*', "*", $2); }
	| expr '*' { $$ = new PostfixExprAST(getloc(@1, @2), (int)'*', "*", $1); }
	| '!' expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)'!', "!", $2); }
	| '&' expr %prec REF { $$ = new PrefixExprAST(getloc(@1, @2), (int)'&', "&", $2); }
	| expr ':' { $$ = new PostfixExprAST(getloc(@1, @2), (int)':', ":", $1); }
	| AWT expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::AWT, "await", $2); }
	| NEW expr { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::NEW, "new", $2); }
	| INC id_or_plchld %prec PREINC { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::INC, "++", $2); }
	| DEC id_or_plchld %prec PREINC { $$ = new PrefixExprAST(getloc(@1, @2), (int)token::DEC, "--", $2); }
	| id_or_plchld INC %prec POSTINC { $$ = new PostfixExprAST(getloc(@1, @2), (int)token::INC, "++", $1); }
	| id_or_plchld DEC %prec POSTINC { $$ = new PostfixExprAST(getloc(@1, @2), (int)token::DEC, "--", $1); }
;

%%

void yy::CParser::error( const location_type &l, const std::string &err_message )
{
	std::cerr << "Error: " << err_message << " at " << l << "\n"; //TODO: throw syntax error
}

extern "C"
{
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

	const std::vector<ExprAST*> parseCTplt(const char* tpltStr)
	{
		// Append STOP expr to make tpltStr a valid statement
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';

		// Parse tpltStr into tpltBlock
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
		{
			std::cerr << "\033[31merror:\033[0merror parsing template " << std::string(tpltStr) << '\n';
			return {};
		} //TODO: Throw CompileError instead:
			//throw CompileError("error parsing template " + std::string(tpltStr));

		// Remove appended STOP expr if last expr is $B
		assert(tpltBlock->exprs->back()->exprtype == ExprAST::ExprType::STOP);
		if (tpltBlock->exprs->size() >= 2)
		{
			const PlchldExprAST* lastExpr = (const PlchldExprAST*)tpltBlock->exprs->at(tpltBlock->exprs->size() - 2);
			if (lastExpr->exprtype == ExprAST::ExprType::PLCHLD && lastExpr->p1 == 'B')
				tpltBlock->exprs->pop_back();
		}

		return *tpltBlock->exprs;
	}
}

BlockExprAST* BlockExprAST::parseCFile(const char* filename)
{
	return ::parseCFile(filename);
}

const std::vector<ExprAST*> BlockExprAST::parseCTplt(const char* tpltStr)
{
	return ::parseCTplt(tpltStr);
}