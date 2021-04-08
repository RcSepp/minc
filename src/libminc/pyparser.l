%{
#include <stdio.h>
#include <fstream>
#include <math.h>
#include <algorithm>
#include "../../src/libminc/pyparser.h"
using namespace std;
using token = yy::PyParser::token;
#undef YY_DECL
#define YY_DECL int PyLexer::yylex(yy::PyParser::semantic_type* const lval, yy::PyParser::location_type* loc)
#define YY_USER_ACTION loc->step(); loc->columns(yyleng);

#define TAB_WIDTH 4
#define YYSTYPE int
#define YY_USER_INIT { g_indent_levels.push(0); BEGIN(initial); }
%}

%option warn debug
%option c++
%option noyywrap
%option yyclass="PyLexer"
%option prefix="Py"

/* Sub-parser (state) for indentation-sensitive scoping */
%x initial
%x indent
%s normal
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
RS			>>
LS			<<
AND			&&
OR			\|\|
IDIV		\/\/
CADD		\+=
CSUB		-=
CMUL		\*=
CMML		@=
CDIV		\/=
CMOD		%=
CAND		&=
COR			\|=
CXOR		^=
CLS			<<=
CRS			>>=
CPOW		\*\*=
CIDV		\/\/=
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

%%
	int indent_caller = normal;

	/* This helps to keep track of the column number.
	* Note that it won't work if you have a rule which includes a newline and is
	* longer than one character because in that case that rule will be favored
	* and this one here won't get called.
	* TL;DR: If you have a rule which includes \n and more, you need to reset
	*        yycolumn inside that rule!
	*/
<*>\n { loc->lines(); REJECT; }

<*>#[^\n]* // Single-line comment
<*>\"\"\"([^\"]|\"[^\"]|\"\"[^\"])*\"\"\" while (*(++yytext)) if (*yytext == '\n') loc->lines(); // Multi-line comment

	/* Everything runs in the <normal> mode and enters the <indent> mode
		when a newline symbol is encountered.
		There is no newline symbol before the first line, so we need to go
		into the <indent> mode by hand there.
	*/
<initial>.  { indent_caller = normal; BEGIN(indent); loc->columns(-1); yyless(0); }
<initial>\n { indent_caller = normal; BEGIN(indent); loc->columns(-1); REJECT; }

	/* The following are the rules that keep track of indentation. */
<indent>" "		{ g_current_line_indent++; }
<indent>\t		{ g_current_line_indent = (g_current_line_indent + TAB_WIDTH) & ~(TAB_WIDTH - 1); }
<indent>\n		{ g_current_line_indent = 0; /* ignoring blank line */ }
<indent><<EOF>>	{
					// When encountering the end of file, we want to emit an
					// outdent for all indents currently left.
					if(g_indent_levels.top() != 0) {
						g_indent_levels.pop();

						// See the same code below (<indent>.) for a rationale.
						if(g_current_line_indent != g_indent_levels.top()) {
							unput('\n');
							for(int i = 0 ; i < g_indent_levels.top() ; ++i) {
								unput(' ');
							}
						} else {
							BEGIN(indent_caller);
						}

						return token::OUTDENT;
					} else {
						yyterminate();
					}
				}

<normal><<EOF>>	{ BEGIN(indent); return token::NEWLINE; }

<indent>.		{
					if(!g_is_fake_outdent_symbol) {
						unput(*yytext);
					}
					loc->columns(-2);
					g_is_fake_outdent_symbol = 0;

					// Indentation level has increased. It can only ever
					// increase by one level at a time. Remember how many
					// spaces this level has and emit an indentation token.
					if(g_current_line_indent > g_indent_levels.top()) {
						g_indent_levels.push(g_current_line_indent);
						BEGIN(indent_caller);
						return token::INDENT;
					} else if(g_current_line_indent < g_indent_levels.top()) {
						// Outdenting is the most difficult, as we might need to
						// outdent multiple times at once, but flex doesn't allow
						// emitting multiple tokens at once! So we fake this by
						// 'unput'ting fake lines which will give us the next
						// outdent.
						g_indent_levels.pop();

						if(g_current_line_indent != g_indent_levels.top()) {
							// Unput the rest of the current line, including the newline.
							// We want to keep it untouched.
							for(int i = 0 ; i < g_current_line_indent ; ++i) {
								unput(' ');
							}
							unput('\n');
							// Now, insert a fake character indented just so
							// that we get a correct outdent the next time.
							unput('.');
							// Though we need to remember that it's a fake one
							// so we can ignore the symbol.
							g_is_fake_outdent_symbol = 1;
							for(int i = 0 ; i < g_indent_levels.top() ; ++i) {
								unput(' ');
							}
							unput('\n');
							// Ignore the two unput('\n')s when tracking locations
							loc->lines(-2);
						} else {
							BEGIN(indent_caller);
						}

						return token::OUTDENT;
					} else {
						// No change in indentation, not much to do here...
						BEGIN(indent_caller);
					}
				}

<normal>{AWT}							return token::AWT;
<normal>{NEW}							return token::NEW;
<normal>{PAND}							return token::PAND;
<normal>{POR}							return token::POR;
<normal>{IF}							return token::IF;
<normal>{ELSE}							return token::ELSE;
<normal>{FOR}							return token::FOR;
<normal>{WHL}							return token::WHL;
<normal>{NOT}							return token::NOT;
<normal>{IN}							return token::IN;
<normal>{IS}							return token::IS;
<normal>{DM}							return token::DM;
<normal>{LITERAL}						{
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

<normal>{ID}							{
									char* name = new char[strlen(yytext) + 1];
									strcpy(name, yytext);
									lval->build<const char*>(name);
									return token::ID;
								}

<normal>{PLCHLD1}						{
									lval->build<char>(yytext[1]);
									return token::PLCHLD1;
								}
<normal>{PLCHLD2}						{
									int len = strlen(yytext) - 3;
									char* value = new char[len + 1];
									value[0] = yytext[1];
									memcpy(value + 1, yytext + 3, len);
									value[len] = '\0';
									lval->build<const char*>(value);
									return token::PLCHLD2;
								}

<normal>{PARAM}							{
									lval->build<int>(atoi(yytext + 1));
									return token::PARAM;
								}

<normal>\(		{ ++g_current_rbkt_level; return yytext[0]; }
<normal>\)		{ if (--g_current_rbkt_level < 0) {} return yytext[0]; } //TODO: Throw "missing opening bracket"

<normal>\[		{ ++g_current_sbkt_level; return yytext[0]; }
<normal>\]		{ if (--g_current_sbkt_level < 0) {} return yytext[0]; } //TODO: Throw "missing opening bracket"

<normal>\{		{ ++g_current_cbkt_level; return yytext[0]; }
<normal>\}		{ if (--g_current_cbkt_level < 0) {} return yytext[0]; } //TODO: Throw "missing opening bracket"

<normal>[ \t]+							// White space
<normal>\n								{ // New line
									if ((g_current_rbkt_level | g_current_sbkt_level | g_current_cbkt_level) == 0)
									{
										g_current_line_indent = 0;
										indent_caller = YY_START;
										BEGIN(indent);
										return token::NEWLINE;
									}
								}

<normal>{ELLIPSIS}						return token::ELLIPSIS;
<normal>{EQ}							return token::EQ;
<normal>{NE}							return token::NE;
<normal>{GEQ}							return token::GEQ;
<normal>{LEQ}							return token::LEQ;
<normal>{RS}							return token::RS;
<normal>{LS}							return token::LS;
<normal>{AND}							return token::AND;
<normal>{OR}							return token::OR;
<normal>{IDIV}							return token::IDIV;
<normal>{CADD}							return token::CADD;
<normal>{CSUB}							return token::CSUB;
<normal>{CMUL}							return token::CMUL;
<normal>{CMML}							return token::CMML;
<normal>{CDIV}							return token::CDIV;
<normal>{CMOD}							return token::CMOD;
<normal>{CAND}							return token::CAND;
<normal>{COR}							return token::COR;
<normal>{CXOR}							return token::CXOR;
<normal>{CLS}							return token::CLS;
<normal>{CRS}							return token::CRS;
<normal>{CPOW}							return token::CPOW;
<normal>{CIDV}							return token::CIDV;

<normal>;|=|\+|,|.|:|<|>|%				return yytext[0];

%%