%require "3.3"
%debug
%defines
%language "C++"
%locations
%define api.parser.class {PyParser}
%define api.value.type variant
%parse-param {PyLexer& scanner}
%parse-param {const char* filename}
%parse-param {MincBlockExpr** rootBlock}

%code requires{
	class PyLexer;
}

%{
#include <vector>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include "../../src/libminc/pyparser.h"

#undef yylex
#define yylex scanner.yylex

#define getloc(b, e) MincLocation{filename, (unsigned)b.begin.line, (unsigned)b.begin.column, (unsigned)e.end.line, (unsigned)e.end.column}
%}

%token ELLIPSIS // ...
%token EQ NE GEQ LEQ GR LE DM SR INC DEC RS LS AND OR IDIV CADD CSUB CMUL CMML CDIV CMOD CAND COR CXOR CLS CRS CPOW CIDV // Operators
%token AWT NEW PAND POR IF ELSE FOR WHL NOT IN NIN IS // Keywords
%token NEWLINE INDENT OUTDENT // Language specific tokens
%token<const char*> LITERAL ID PLCHLD2
%token<char> PLCHLD1
%token<int> PARAM
%type<MincBlockExpr*> block
%type<std::vector<MincExpr*>*> stmt_string
%type<MincListExpr*> stmt optional_expr kvexpr_list optional_kvexpr_list expr_idx
%type<MincExpr*> id_or_plchld expr kvexpr

%start file
%right CADD CSUB CMUL CMML CDIV CMOD CAND COR CXOR CLS CRS CPOW CIDV
%right IF ELSE FOR WHL
%left IN IS
%left ','
%right '=' ':'
%left OR POR
%left AND PAND
%left '&'
%left EQ NE
%left GEQ LEQ RS LS '>' '<'
%left '+' '-'
%left '*' '/' '%' IDIV
%right AWT NEW REF
%right DCOR
%left '.' CALL SUBSCRIPT TPLT DM NOT
%left ENC

%%

file
	: stmt_string { *rootBlock = new MincBlockExpr(getloc(@1, @1), $1); }
	| %empty { *rootBlock = new MincBlockExpr({0}, new std::vector<MincExpr*>()); } // Empty file
;

block
	: INDENT stmt_string OUTDENT { $$ = new MincBlockExpr(MincLocation{filename, (unsigned)@1.begin.line, (unsigned)@1.begin.column, $2->back()->loc.end_line, $2->back()->loc.end_column}, $2); }
;

stmt_string
	: stmt NEWLINE { $$ = &$1->exprs; $$->push_back(new MincStopExpr(MincLocation{filename, (unsigned)@1.end.line, (unsigned)@1.end.column, (unsigned)@1.end.line, (unsigned)@1.end.column})); }
	| stmt ':' NEWLINE block { $$ = &$1->exprs; $$->push_back($4); }
	| stmt ':' stmt NEWLINE { $$ = &$1->exprs; $3->exprs.push_back(new MincStopExpr(MincLocation{filename, (unsigned)@3.end.line, (unsigned)@3.end.column, (unsigned)@3.end.line, (unsigned)@3.end.column})); $$->push_back(new MincBlockExpr(getloc(@3, @3), new std::vector<MincExpr*>($3->exprs))); }
	| stmt_string NEWLINE { $$ = $1; } // Blank line
	| stmt_string stmt NEWLINE { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); $$->push_back(new MincStopExpr(MincLocation{filename, (unsigned)@2.end.line, (unsigned)@2.end.column, (unsigned)@2.end.line, (unsigned)@2.end.column})); }
	| stmt_string stmt ':' NEWLINE block { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); $$->push_back($5); }
	| stmt_string stmt ':' stmt NEWLINE { ($$ = $1)->insert($1->end(), $2->cbegin(), $2->cend()); $4->exprs.push_back(new MincStopExpr(MincLocation{filename, (unsigned)@4.end.line, (unsigned)@4.end.column, (unsigned)@4.end.line, (unsigned)@4.end.column})); $$->push_back(new MincBlockExpr(getloc(@4, @4), new std::vector<MincExpr*>($4->exprs))); }
;

stmt
	: expr	{
								MincListExpr* stmt = new MincListExpr('\0');
								stmt->exprs.push_back($1);
								$$ = stmt;
							}
	| stmt expr	{
								MincListExpr* stmt = $1;
								stmt->exprs.push_back($2);
								$$ = stmt;
							}
	| stmt '=' expr	{
								MincListExpr* stmt = $1;
								const MincLocation& loc = MincLocation{filename, stmt->exprs.back()->loc.begin_line, stmt->exprs.back()->loc.begin_column, (unsigned)@3.end.line, (unsigned)@3.end.column};
								stmt->exprs.back() = new MincBinOpExpr(loc, (int)'=', "=", stmt->exprs.back(), $3);
								$$ = stmt;
							}
;

optional_expr
	: %empty { $$ = new MincListExpr(','); }
	| expr { $$ = new MincListExpr(',', { $1 }); }
;

expr_idx
	: %empty { $$ = new MincListExpr(':', { new MincListExpr('\0') }); }
	| expr_idx ':' { ($$ = $1)->exprs.push_back(new MincListExpr('\0')); }
	| expr_idx ':' ELLIPSIS { ($$ = $1)->exprs.push_back(new MincEllipsisExpr(getloc(@3, @3), new MincListExpr('\0'))); }
	| expr_idx expr { ((MincListExpr*)($$ = $1)->exprs.back())->exprs.push_back($2); }
;

id_or_plchld
	: ID { $$ = new MincIdExpr(getloc(@1, @1), $1); }
	| PLCHLD1 { $$ = new MincPlchldExpr(getloc(@1, @1), $1); }
	| PLCHLD2 { $$ = new MincPlchldExpr(getloc(@1, @1), $1); }
	| ELSE { $$ = new MincIdExpr(getloc(@1, @1), "else"); }
;

optional_kvexpr_list
	: %empty { $$ = new MincListExpr(','); }
	| kvexpr_list { $$ = $1; }
;

kvexpr_list
	: kvexpr { $$ = new MincListExpr(',', { $1 }); }
	| kvexpr_list ',' { ($$ = $1)->exprs.push_back(new MincListExpr('\0')); } //TODO: Not working
	| kvexpr_list ',' kvexpr { ($$ = $1)->exprs.push_back($3); } //TODO: Not working
;

kvexpr
	: expr { $$ = $1; }
	| expr ':' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)':', ":", $1, $3); }
	| expr ':' expr FOR expr IN expr { $$ = new MincTerOpExpr(getloc(@1, @7), (int)token::FOR, (int)token::IN, "for", "in", new MincBinOpExpr(getloc(@1, @3), (int)':', ":", $1, $3), $5, $7); }
;

expr
	: LITERAL { $$ = new MincLiteralExpr(getloc(@1, @1), $1); }
	| PARAM { $$ = new MincParamExpr(getloc(@1, @1), $1); }
	| id_or_plchld { $$ = $1; }

	// Enclosed expressions
	| '(' optional_expr ')' %prec ENC { $$ = new MincEncOpExpr(getloc(@1, @3), (int)'(', "(", ")", $2); }
	| '[' expr_idx ']' %prec ENC { $$ = new MincEncOpExpr(getloc(@1, @3), (int)'[', "[", "]", $2); }
	| '{' optional_kvexpr_list '}' %prec ENC { $$ = new MincEncOpExpr(getloc(@1, @3), (int)'{', "{", "}", $2); }

	// Parameterized expressions
	| expr '(' optional_expr ')' %prec CALL { $$ = new MincArgOpExpr(getloc(@1, @4), (int)'(', "(", ")", $1, $3); }
	| expr '[' expr_idx ']' %prec SUBSCRIPT { $$ = new MincArgOpExpr(getloc(@1, @4), (int)'[', "[", "]", $1, $3); }

	// Tertiary operators
	| expr FOR expr IN expr { $$ = new MincTerOpExpr(getloc(@1, @5), (int)token::FOR, (int)token::IN, "for", "in", $1, $3, $5); }
	| expr IF expr ELSE expr { $$ = new MincTerOpExpr(getloc(@1, @5), (int)token::IF, (int)token::ELSE, "if", "else", $1, $3, $5); }

	// Binary operators
	| expr '=' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'=', "=", $1, $3); }
	| expr CADD expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CADD, "+=", $1, $3); }
	| expr CSUB expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CSUB, "-=", $1, $3); }
	| expr CMUL expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CMUL, "*=", $1, $3); }
	| expr CMML expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CMML, "@=", $1, $3); }
	| expr CDIV expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CDIV, "/=", $1, $3); }
	| expr CMOD expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CMOD, "%=", $1, $3); }
	| expr CAND expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CAND, "&=", $1, $3); }
	| expr COR expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::COR, "|=", $1, $3); }
	| expr CXOR expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CXOR, "^=", $1, $3); }
	| expr CLS expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CLS, "<<=", $1, $3); }
	| expr CRS expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CRS, ">>=", $1, $3); }
	| expr CPOW expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CPOW, "**=", $1, $3); }
	| expr CIDV expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::CIDV, "//=", $1, $3); }
	| expr '.' id_or_plchld { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'.', ".", $1, $3); }
	| expr '.' ELLIPSIS { $$ = new MincVarBinOpExpr(getloc(@1, @3), (int)'.', ".", $1); }
	| expr DM id_or_plchld { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::DM, "->", $1, $3); }
	| expr '+' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'+', "+", $1, $3); }
	| expr '-' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'-', "-", $1, $3); }
	| expr '*' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'*', "*", $1, $3); }
	| expr '/' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'/', "/", $1, $3); }
	| expr IDIV expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::IDIV, "//", $1, $3); }
	| expr '%' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'%', "%", $1, $3); }
	| expr '&' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'&', "&", $1, $3); }
	| expr EQ expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::EQ, "==", $1, $3); }
	| expr NE expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::NE, "!=", $1, $3); }
	| expr GEQ expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::GEQ, ">=", $1, $3); }
	| expr LEQ expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::LEQ, "<=", $1, $3); }
	| expr '>' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'>', ">", $1, $3); }
	| expr '<' expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)'<', "<", $1, $3); }
	| expr RS expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::RS, ">>", $1, $3); }
	| expr LS expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::LS, "<<", $1, $3); }
	| expr AND expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::AND, "&&", $1, $3); }
	| expr OR expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::OR, "||", $1, $3); }
	| expr PAND expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::PAND, "and", $1, $3); }
	| expr POR expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::POR, "or", $1, $3); }
	| expr IN expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::IN, "in", $1, $3); }
	| expr NOT IN expr { $$ = new MincBinOpExpr(getloc(@1, @4), (int)token::NIN, "not in", $1, $4); }
	| expr IS expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::IS, "is", $1, $3); }
	| FOR expr IN expr { $$ = new MincBinOpExpr(getloc(@1, @4), (int)token::FOR, "forin", $2, $4); } //TODO: Create new expression type: `op1 $E op2 $E`
	| expr IF expr { $$ = new MincBinOpExpr(getloc(@1, @3), (int)token::IF, "if", $1, $3); }

	// Unary operators
	| '+' expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)'+', "+", $2); } //TODO: Precedence
	| '-' expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)'-', "-", $2); } //TODO: Precedence
	| '*' expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)'*', "*", $2); }
	| expr '*' { $$ = new MincPostfixExpr(getloc(@1, @2), (int)'*', "*", $1); }
	| '!' expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)'!', "!", $2); }
	| '&' expr %prec REF { $$ = new MincPrefixExpr(getloc(@1, @2), (int)'&', "&", $2); }
	| '@' expr %prec DCOR { $$ = new MincPrefixExpr(getloc(@1, @2), (int)'@', "@", $2); }
	| AWT expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::AWT, "await", $2); }
	| NEW expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::NEW, "new", $2); }
	| IF expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::IF, "if", $2); }
	| WHL expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::WHL, "while", $2); }
	| NOT expr { $$ = new MincPrefixExpr(getloc(@1, @2), (int)token::NOT, "not", $2); }

	// List operators
	| expr ',' { $$ = new MincListExpr(',', { $1 }); }
	| expr ',' ELLIPSIS {
		if ($1->exprtype == MincExpr::ExprType::LIST)
			((MincListExpr*)($$ = $1))->exprs.back() = new MincEllipsisExpr(getloc(@3, @3), ((MincListExpr*)$1)->exprs.back());
		else
			$$ = new MincListExpr(',', { new MincEllipsisExpr(getloc(@3, @3), $1) });
	}
	| expr ',' expr {
		MincListExpr *l1 = (MincListExpr*)$1, *l3 = (MincListExpr*)$3;
		if ($1->exprtype == MincExpr::ExprType::LIST && $3->exprtype == MincExpr::ExprType::LIST)
			l1->exprs.insert(l1->end(), l3->begin(), l3->end());
		else if ($1->exprtype == MincExpr::ExprType::LIST && $3->exprtype != MincExpr::ExprType::LIST)
			l1->exprs.push_back($3);
		else if ($1->exprtype != MincExpr::ExprType::LIST && $3->exprtype == MincExpr::ExprType::LIST)
		{
			MincListExpr* const lout = new MincListExpr(',', { $1 });
			lout->exprs.insert(l1->end(), l3->begin(), l3->end());
			l1 = lout;
		}
		else
			l1 = new MincListExpr(',', { $1, $3 });
		$$ = l1;
	}
;

%%

void yy::PyParser::error(const location_type &l, const std::string &err_message)
{
	throw CompileError(err_message, getloc(l, l));
}

extern "C"
{
	MincBlockExpr* parsePythonFile(const char* filename)
	{
		// Open source file
		std::ifstream in(filename);
		if (!in.good())
		{
			std::cerr << "\033[31merror:\033[0m " << std::string(filename) << ": No such file or directory\n";
			return nullptr;
		}

		// Parse file into rootBlock
		MincBlockExpr* rootBlock;
		PyLexer lexer(&in, &std::cout);
		yy::PyParser parser(lexer, filename, &rootBlock);
		parser.parse();

		// Close source file
		in.close();

		return rootBlock;
	}

	const std::vector<MincExpr*> parsePythonTplt(const char* tpltStr)
	{
		// Parse tpltStr into tpltBlock
		std::stringstream ss(tpltStr);
		PyLexer lexer(ss, std::cout);
		MincBlockExpr* tpltBlock;
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
}