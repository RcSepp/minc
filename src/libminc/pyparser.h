#ifndef __PYLEXER_H
#define __PYLEXER_H

#include <stack>

#ifndef yyFlexLexer
//#undef yyFlexLexer
#define yyFlexLexer PyFlexLexer
#include <FlexLexer.h>
#endif

//class PyLexer;
#include "minc_api.hpp"

#include "../../tmp/libminc/location.hh"
#include "../../tmp/libminc/pyparser.hh"

class PyLexer : public yyFlexLexer
{
private:
int g_current_line_indent;
std::stack<int> g_indent_levels;
int g_is_fake_outdent_symbol;
int g_current_rbkt_level, g_current_sbkt_level, g_current_cbkt_level;

public:
	PyLexer(std::istream& in, std::ostream& out)
		: yyFlexLexer(in, out), g_current_line_indent(0), g_is_fake_outdent_symbol(0),
		  g_current_rbkt_level(0), g_current_sbkt_level(0), g_current_cbkt_level(0) {}
	PyLexer(std::istream* in, std::ostream* out)
		: yyFlexLexer(in, out), g_current_line_indent(0), g_is_fake_outdent_symbol(0),
		  g_current_rbkt_level(0), g_current_sbkt_level(0), g_current_cbkt_level(0) {}
	virtual ~PyLexer() {}
	using FlexLexer::yylex;
	virtual int yylex(yy::PyParser::semantic_type * const lval, yy::PyParser::location_type *location);
};

#endif
