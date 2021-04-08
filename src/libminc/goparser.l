%{
#include <stdio.h>
#include <fstream>
#include <math.h>
#include <algorithm>
#include "../../src/libminc/goparser.h"
using namespace std;
using token = yy::GoParser::token;
#undef YY_DECL
#define YY_DECL int GoLexer::yylex(yy::GoParser::semantic_type* const lval, yy::GoParser::location_type* loc)
#define YY_USER_ACTION loc->step(); loc->columns(yyleng);

#define TAB_WIDTH 4
#define YYSTYPE int
%}

%option warn debug
%option c++
%option noyywrap
%option yyclass="GoLexer"
%option prefix="Go"

%x end

LITERAL		(-?[0-9]*\.?[0-9][0-9a-zA-Z_]*)|([0-9a-zA-Z_]*\"[^\"\n]*\")|([0-9a-zA-Z_]*\'[^\'\n]*\')
ID			([a-zA-Z_][0-9a-zA-Z_]*)|$#
PLCHLD1		$[A-Z]
PLCHLD2		$[A-Z]<[a-zA-Z_][0-9a-zA-Z_]*!?!?>
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
CADD		\+=
CSUB		-=
CMUL		\*=
CDIV		\/=
CBA			&=
CBO			\|=
CBN			~=
CBX			\^=
AWT			await
NEW			new
PAND		and
POR			or
IF			if
ELSE		else
FOR			for
WHL			while
NOT			not
IN			in
IS			is
DM			->
SR			::
INC			\+\+
DEC			--

%%

"//"[^\n]*						// Single-line comment
"/*"([^*]|(\*+[^*/]))*\*+\/		while (*(++yytext)) if (*yytext == '\n') loc->lines(); // Multi-line comment

\(								{ ++g_current_rbkt_level; return yytext[0]; }
\)								{ if (--g_current_rbkt_level < 0) {} return yytext[0]; } //TODO: Throw "missing opening bracket"

\[								{ ++g_current_sbkt_level; return yytext[0]; }
\]								{ if (--g_current_sbkt_level < 0) {} return yytext[0]; } //TODO: Throw "missing opening bracket"

\}								{
									if (autoNewline)
									{
										autoNewline = false;
										unput('}');
										return token::NEWLINE;
									}
									else
									{
										autoNewline = true;
										return '}';
									}
								}

[ \t]+							// White space
\n								{
									loc->lines();
									if ((g_current_rbkt_level | g_current_sbkt_level) == 0)
										return token::NEWLINE;
									else
										REJECT;
								}
<end><<EOF>>					yyterminate();
<<EOF>>							{ BEGIN(end); return token::NEWLINE; }

{AWT}							return token::AWT;
{NEW}							return token::NEW;
{PAND}							return token::PAND;
{POR}							return token::POR;
{IF}							return token::IF;
{ELSE}							return token::ELSE;
{FOR}							return token::FOR;
{WHL}							return token::WHL;
{NOT}							return token::NOT;
{IN}							return token::IN;
{IS}							return token::IS;
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

{ELLIPSIS}						return token::ELLIPSIS;
{EQ}							return token::EQ;
{NE}							return token::NE;
{GEQ}							return token::GEQ;
{LEQ}							return token::LEQ;
{GR}							return token::GR;
{LE}							return token::LE;
{AND}							return token::AND;
{OR}							return token::OR;
{CADD}							return token::CADD;
{CSUB}							return token::CSUB;
{CMUL}							return token::CMUL;
{CDIV}							return token::CDIV;
{CBA}							return token::CBA;
{CBO}							return token::CBO;
{CBN}							return token::CBN;
{CBX}							return token::CBX;

;|=|\+|\(|\)|,|\{|\}|\[|\]|.|&|!|:|\?	return yytext[0];

%%