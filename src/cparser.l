%{
#include <stdio.h>
#include <fstream>
#include <math.h>
#include <algorithm>
#include "../src/cparser.h"
using namespace std;
using token = yy::CParser::token;
#undef YY_DECL
#define YY_DECL int CLexer::yylex(yy::CParser::semantic_type* const lval, yy::CParser::location_type* loc)
#define YY_USER_ACTION loc->step(); loc->columns(yyleng);
%}

%option warn debug
%option c++
%option noyywrap
%option yyclass="CLexer"
%option prefix="C"

LITERAL		(-?[0-9]*\.?[0-9][0-9a-zA-Z_]*)|([0-9a-zA-Z_]*\"[^\"\n]*\")|([0-9a-zA-Z_]*\'[^\'\n]*\')
ID			([a-zA-Z_][0-9a-zA-Z_]*)|$#
PLCHLD1		$[A-Z]
PLCHLD2		$[A-Z]<[a-zA-Z_][0-9a-zA-Z_]*!?>
PARAM		$0|($[1-9][0-9]*)
ELLIPSIS	\.\.\.
EQ			==
NE			!=
GEQ			>=
LEQ			<=
GR			>>
LE			<<
AND			&&
OR			\|\|
NEW			new
DM			->
SR			::
INC			\+\+
DEC			--

%%

{NEW}							return token::NEW;
{DM}							return token::DM;
{SR}							return token::SR;
{INC}							return token::INC;
{DEC}							return token::DEC;
{LITERAL}						{
									size_t len = strlen(yytext);
									char* value = new char[len + 1];
									for (char *s = yytext, *d = value;; ++s, ++d)
									{
										if (*s == '\\')
										{
											switch(*(++s))
											{
											case '\'': case '"': case '?': case '\\': *d = *s; break;
											case 'a': *d = '\a'; break;
											case 'b': *d = '\b'; break;
											case 'f': *d = '\f'; break;
											case 'n': *d = '\n'; break;
											case 'r': *d = '\r'; break;
											case 't': *d = '\t'; break;
											case 'v': *d = '\v'; break;
											//TODO: Handle \nnn and \xnn (see https://en.cppreference.com/w/cpp/language/escape)
											default: break;//TODO: Throw "unrecognized escape sequence"
											}
										}
										else
											*d = *s;

										if (*d == '\0')
											break;
									}
									lval->build<const char*>(value);
									return token::LITERAL;
								}

{ID}							{
									char* name = new char[strlen(yytext) + 1];
									strcpy(name, yytext);
									lval->build<const char*>(name);
									return token::ID;
								}

{PLCHLD1}						{
									lval->build<char>(yytext[1]);
									return token::PLCHLD1;
								}
{PLCHLD2}						{
									int len = strlen(yytext) - 3;
									char* value = new char[len + 1];
									value[0] = yytext[1];
									memcpy(value + 1, yytext + 3, len);
									value[len] = '\0';
									lval->build<const char*>(value);
									return token::PLCHLD2;
								}

{PARAM}							{
									lval->build<int>(atoi(yytext + 1));
									return token::PARAM;
								}

[ \t]+							// White space
\n								loc->lines(); // New line
"//"[^\n]*						// Single-line comment
"/*"([^*]|(\*+[^*/]))*\*+\/		while (*(++yytext)) if (*yytext == '\n') loc->lines(); // Multi-line comment

{ELLIPSIS}						return token::ELLIPSIS;
{EQ}							return token::EQ;
{NE}							return token::NE;
{GEQ}							return token::GEQ;
{LEQ}							return token::LEQ;
{GR}							return token::GR;
{LE}							return token::LE;
{AND}							return token::AND;
{OR}							return token::OR;

;|=|\+|\(|\)|,|\{|\}|\[|\]|.|&|!|:|\?	return yytext[0];

%%