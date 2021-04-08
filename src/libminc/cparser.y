%require "3.3"
%debug
%defines
%language "C++"
%locations
%define api.parser.class {CParser}
%define api.value.type variant
%parse-param {CLexer& scanner}
%parse-param {const char* filename}
%parse-param {MincBlockExpr** rootBlock}

%code requires{
	class CLexer;
}

%{
#include <vector>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include "../../src/libminc/cparser.h"

#undef yylex
#define yylex scanner.yylex

#define getloc(b, e) MincLocation{filename, (unsigned)b.begin.line, (unsigned)b.begin.column, (unsigned)e.end.line, (unsigned)e.end.column}
%}

%token ELLIPSIS // ...
%token EQ NE GEQ LEQ GR LE DM SR INC DEC RS LS AND OR IDIV CADD CSUB CMUL CMML CDIV CMOD CAND COR CXOR CBA CBO CBN CBX CLS CRS CPOW CIDV // Operators
%token AWT NEW PAND POR IF ELSE FOR WHL NOT IN NIN IS // Keywords
%token<const char*> LITERAL ID PLCHLD2
%token<char> PLCHLD1
%token<int> PARAM
%type<std::vector<MincExpr*>*> block stmt_string
%type<MincListExpr*> expr_string expr_list single_expr_list expr_lists optional_expr_lists stmt stmt_expr_list
%type<MincExpr*> id_or_plchld expr

%start file
%right CADD CSUB CMUL CMML CDIV CMOD CAND COR CXOR CBA CBO CBN CBX CLS CRS CPOW CIDV
%right IF ELSE FOR WHL
%left IN IS
%right '=' '?' ':'
%left OR
%left AND
%left '&'
%left EQ NE
%left GEQ LEQ GR LE
%left '+' '-'
%left '*' '/' '%'
%right PREFIX
%left '.' CALL SUBSCRIPT TPLT DM POSTFIX
%left SR
%left ENC

%%

file
	: block { *rootBlock = new MincBlockExpr(getloc(@1, @1), $1); }
;

block
	: stmt_string { $$ = $1; }
;

stmt_string
	: %empty { $$ = new std::vector<MincExpr*>(); }
	| stmt_string stmt { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); }
;

stmt
	: ';' { $$ = new MincListExpr('\0', { new MincStopExpr(getloc(@1, @1)) }); }
	| '{' block '}' { $$ = new MincListExpr('\0', { new MincBlockExpr(getloc(@1, @3), $2) }); }
	| stmt_expr_list ';' { ($$ = $1)->push_back(new MincStopExpr(getloc(@2, @2))); }
	| stmt_expr_list '{' block '}' { ($$ = $1)->push_back(new MincBlockExpr(getloc(@2, @4), $3)); }
;

stmt_expr_list
	: expr { $$ = new MincListExpr('\0', { $1 }); }

	| stmt_expr_list expr { ($$ = $1)->exprs.push_back($2); }
	| stmt_expr_list ELLIPSIS { ($$ = $1)->exprs.back() = new MincEllipsisExpr(getloc(@2, @2), $1->exprs.back()); }

	| stmt_expr_list ',' expr		{
									MincListExpr* back = (MincListExpr*)($$ = $1)->exprs.back();
									if (back->exprtype == MincExpr::ExprType::LIST && back->separator == ',')
										back->push_back($3);
									else
										$1->exprs.back() = new MincListExpr(',', { back, $3 });
								}
	| stmt_expr_list ',' ELLIPSIS	{
									MincListExpr* back = (MincListExpr*)($$ = $1)->exprs.back();
									if (back->exprtype == MincExpr::ExprType::LIST && back->separator == ',')
										back->exprs.back() = new MincEllipsisExpr(getloc(@3, @3), back->exprs.back());
									else
										$1->exprs.back() = new MincListExpr(',', { new MincEllipsisExpr(getloc(@3, @3), back) });
								}
;

expr_string
	: expr { $$ = new MincListExpr('\0'); $$->exprs.push_back($1); }
	| expr_string expr { ($$ = $1)->exprs.push_back($2); }
	| expr_string ELLIPSIS { ($$ = $1)->exprs.back() = new MincEllipsisExpr(getloc(@2, @2), $1->exprs.back()); }
;

expr_list
	: expr_string { $$ = new MincListExpr(','); $$->exprs.push_back($1); }
	| expr_list ',' expr_string { ($$ = $1)->exprs.push_back($3); }
	| expr_list ',' ELLIPSIS { ($$ = $1)->exprs.back() = new MincEllipsisExpr(getloc(@3, @3), $1->exprs.back()); }
;

single_expr_list
	: expr { $$ = new MincListExpr(','); $$->exprs.push_back($1); }
	| single_expr_list ',' expr { ($$ = $1)->exprs.push_back($3); }
	| single_expr_list ',' ELLIPSIS { ($$ = $1)->exprs.back() = new MincEllipsisExpr(getloc(@3, @3), $1->exprs.back()); }
;

expr_lists
	: expr_list { $$ = new MincListExpr(';'); $$->exprs.push_back($1); }
	| expr_list ',' { $$ = new MincListExpr(';'); $$->exprs.push_back($1); }
	| expr_lists ';' expr_list { ($$ = $1)->exprs.push_back($3); }
	| expr_lists ';' ELLIPSIS { ($$ = $1)->exprs.back() = new MincEllipsisExpr(getloc(@3, @3), $1->exprs.back()); }
;

optional_expr_lists
	: %empty { $$ = new MincListExpr(';'); }
	| expr_lists { $$ = $1; }
;

id_or_plchld
	: ID { $$ = new MincIdExpr(getloc(@1, @1), $1); }
	| PLCHLD1 { $$ = new MincPlchldExpr(getloc(@1, @1), $1); }
	| PLCHLD2 { $$ = new MincPlchldExpr(getloc(@1, @1), $1); }
	| ELSE { $$ = new MincIdExpr(getloc(@1, @1), "else"); }
;

expr
	: LITERAL { $$ = new MincLiteralExpr(getloc(@1, @1), $1); }
	| PARAM { $$ = new MincParamExpr(getloc(@1, @1), $1); }
	| id_or_plchld { $$ = $1; }

	// Enclosed expressions
	| '(' optional_expr_lists ')' %prec ENC { $$ = new MincEncOpExpr(getloc(@1, @3), (int)'(', "(", ")", $2); }
	| '[' optional_expr_lists ']' %prec ENC { $$ = new MincEncOpExpr(getloc(@1, @3), (int)'[', "[", "]", $2); }
	| '<' optional_expr_lists '>' %prec ENC { $$ = new MincEncOpExpr(getloc(@1, @3), (int)'<', "<", ">", $2); }
	| '{' block '}' { $$ = new MincBlockExpr(getloc(@1, @3), $2); }

	// Parameterized expressions
	| expr '(' optional_expr_lists ')' %prec CALL { $$ = new MincArgOpExpr(getloc(@1, @4), (int)'(', "(", ")", $1, $3); }
	| expr '[' optional_expr_lists ']' %prec SUBSCRIPT { $$ = new MincArgOpExpr(getloc(@1, @4), (int)'[', "[", "]", $1, $3); }
	| id_or_plchld '<' optional_expr_lists '>' %prec TPLT { $$ = new MincArgOpExpr(getloc(@1, @4), (int)'<', "<", ">", $1, $3); }

	// Tertiary operators
	| expr '?' expr ':' expr { $$ = new MincTerOpExpr(getloc(@1, @5), (int)'?', (int)':', "?", ":", $1, $3, $5); }

	// Binary operators
	| expr '=' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'=', "=", $1, $3); }
	| expr CADD expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CADD, "+=", $1, $3); }
	| expr CSUB expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CSUB, "-=", $1, $3); }
	| expr CMUL expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CMUL, "*=", $1, $3); }
	| expr CDIV expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CDIV, "/=", $1, $3); }
	| expr CBA expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CBA, "&=", $1, $3); }
	| expr CBO expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CBO, "|=", $1, $3); }
	| expr CBN expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CBN, "~=", $1, $3); }
	| expr CBX expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CBX, "^=", $1, $3); }
	| expr '.' id_or_plchld { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'.', ".", $1, $3); }
	| expr '.' ELLIPSIS { $$ = new MincVarBinOpExpr(getloc(@1, @3), (int)'.', ".", $1); }
	| expr DM id_or_plchld { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::DM, "->", $1, $3); }
	| expr SR id_or_plchld { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::SR, "::", $1, $3); }
	| expr '+' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'+', "+", $1, $3); }
	| expr '-' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'-', "-", $1, $3); }
	| expr '*' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'*', "*", $1, $3); }
	| expr '/' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'/', "/", $1, $3); }
	| expr '&' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'&', "&", $1, $3); }
	| expr EQ expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::EQ, "==", $1, $3); }
	| expr NE expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::NE, "!=", $1, $3); }
	| expr GEQ expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::GEQ, ">=", $1, $3); }
	| expr LEQ expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::LEQ, "<=", $1, $3); }
	| expr GR expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::GR, ">>", $1, $3); }
	| expr LE expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::LE, "<<", $1, $3); }
	| expr AND expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::AND, "&&", $1, $3); }
	| expr OR expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::OR, "||", $1, $3); }
	| FOR single_expr_list IN expr { $$ = new MincBinOpExpr(getloc(@1, @4), (int)token::FOR, "forin", $2, $4); } //TODO: Create new expression type: `op1 $E op2 $E`
	| FOR single_expr_list ',' IN expr { $$ = new MincBinOpExpr(getloc(@1, @5), (int)token::FOR, "forin", $2, $5); }

	// Unary operators
	| '+' expr %prec PREFIX { $$ = new MincPrefixExpr(getloc(@1, @2), (int)'+', "+", $2); } //TODO: Precedence
	| '-' expr %prec PREFIX { $$ = new MincPrefixExpr(getloc(@1, @2), (int)'-', "-", $2); } //TODO: Precedence
	| '*' expr %prec PREFIX { $$ = new MincPrefixExpr(getloc(@1, @2), (int)'*', "*", $2); }
	| expr '*' %prec POSTFIX { $$ = new MincPostfixExpr(getloc(@1, @2), (int)'*', "*", $1); }
	| '!' expr %prec PREFIX { $$ = new MincPrefixExpr(getloc(@1, @2), (int)'!', "!", $2); }
	| '&' expr %prec PREFIX { $$ = new MincPrefixExpr(getloc(@1, @2), (int)'&', "&", $2); }
	| expr ':' %prec POSTFIX { $$ = new MincPostfixExpr(getloc(@1, @2), (int)':', ":", $1); }
	| AWT expr %prec PREFIX { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::AWT, "await", $2); }
	| NEW expr %prec PREFIX { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::NEW, "new", $2); }
	| IF expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::IF, "if", $2); }
	| FOR expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::FOR, "for", $2); }
	| WHL expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::WHL, "while", $2); }
	| NOT expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::NOT, "not", $2); }
	| INC id_or_plchld %prec PREFIX { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::INC, "++", $2); }
	| DEC id_or_plchld %prec PREFIX { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::DEC, "--", $2); }
	| id_or_plchld INC %prec POSTFIX { $$ = new MincPostfixExpr(getloc(@1, @2), (int)token::INC, "++", $1); }
	| id_or_plchld DEC %prec POSTFIX { $$ = new MincPostfixExpr(getloc(@1, @2), (int)token::DEC, "--", $1); }
;

%%

void yy::CParser::error(const location_type &l, const std::string &err_message)
{
	throw CompileError(err_message, getloc(l, l));
}

extern "C"
{
	MincBlockExpr* parseCStream(std::istream& stream)
	{
		// Parse stream into rootBlock
		MincBlockExpr* rootBlock;
		CLexer lexer(stream, std::cout);
		yy::CParser parser(lexer, "", &rootBlock);
		parser.parse();

		return rootBlock;
	}

	MincBlockExpr* parseCFile(const char* filename)
	{
		// Open source file
		std::ifstream in(filename);
		if (!in.good())
			throw CompileError(std::string(filename) + ": No such file or directory");

		// Parse file into rootBlock
		MincBlockExpr* rootBlock;
		CLexer lexer(in, std::cout);
		yy::CParser parser(lexer, filename, &rootBlock);
		parser.parse();

		// Close source file
		in.close();

		return rootBlock;
	}

	MincBlockExpr* parseCCode(const char* code)
	{
		std::stringstream ss(code);

		// Parse file into rootBlock
		MincBlockExpr* rootBlock;
		CLexer lexer(ss, std::cout);
		yy::CParser parser(lexer, nullptr, &rootBlock);
		parser.parse();

		return rootBlock;
	}

	const std::vector<MincExpr*> parseCTplt(const char* tpltStr)
	{
		// Append STOP expr to make tpltStr a valid statement
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';

		// Parse tpltStr into tpltBlock
		CLexer lexer(ss, std::cout);
		MincBlockExpr* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr));

		// Remove appended STOP expr if last expr is `$B` or `expr ':'`
		assert(tpltBlock->exprs->back()->exprtype == MincExpr::ExprType::STOP);
		if (tpltBlock->exprs->size() >= 2)
		{
			const MincExpr* lastExpr = (const MincExpr*)tpltBlock->exprs->at(tpltBlock->exprs->size() - 2);
			if ((lastExpr->exprtype == MincExpr::ExprType::PLCHLD && ((const MincPlchldExpr*)lastExpr)->p1 == 'B') ||
				(lastExpr->exprtype == MincExpr::ExprType::POSTOP && ((const MincPostfixExpr*)lastExpr)->opstr == ":"))
				tpltBlock->exprs->pop_back();
		}

		return *tpltBlock->exprs;
	}
}