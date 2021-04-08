#ifndef __PYLEXER_H
#define __PYLEXER_H

#include <stack>

#ifndef yyFlexLexer
//#undef yyFlexLexer
#define yyFlexLexer GoFlexLexer
#include <FlexLexer.h>
#endif

//class GoLexer;
#include "minc_api.hpp"

#include "../../tmp/libminc/location.hh"
#include "../../tmp/libminc/goparser.hh"

class GoLexer : public yyFlexLexer
{
private:
	int g_current_rbkt_level, g_current_sbkt_level;
	bool autoNewline;

public:
	GoLexer(std::istream& in, std::ostream& out)
		: yyFlexLexer(in, out),
		  g_current_rbkt_level(0), g_current_sbkt_level(0), autoNewline(true) {}
	GoLexer(std::istream* in, std::ostream* out)
		: yyFlexLexer(in, out),
		  g_current_rbkt_level(0), g_current_sbkt_level(0), autoNewline(true) {}
	virtual ~GoLexer() {}
	using FlexLexer::yylex;
	virtual int yylex(yy::GoParser::semantic_type * const lval, yy::GoParser::location_type *location);
};

#endif
