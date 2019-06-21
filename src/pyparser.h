#ifndef __PYLEXER_H
#define __PYLEXER_H

#include <stack>

#ifndef yyFlexLexer
//#undef yyFlexLexer
#define yyFlexLexer PyFlexLexer
#include <FlexLexer.h>
#endif

//class PyLexer;
#include "ast.h"

#include "../tmp/location.hh"
#include "../tmp/pyparser.hh"

class PyLexer : public yyFlexLexer
{
private:
int g_current_line_indent;
std::stack<size_t> g_indent_levels;
int g_is_fake_outdent_symbol;

public:
	PyLexer(std::istream& in, std::ostream& out) : yyFlexLexer(in, out), g_current_line_indent(0), g_is_fake_outdent_symbol(0) {}
	PyLexer(std::istream* in, std::ostream* out) : yyFlexLexer(in, out), g_current_line_indent(0), g_is_fake_outdent_symbol(0) {}
	virtual ~PyLexer() {}
	using FlexLexer::yylex;
	virtual int yylex(yy::PyParser::semantic_type * const lval, yy::PyParser::location_type *location);
};

#endif
