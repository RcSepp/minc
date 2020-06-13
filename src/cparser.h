#ifndef __CLEXER_H
#define __CLEXER_H

#ifndef yyFlexLexer
//#undef yyFlexLexer
#define yyFlexLexer CFlexLexer
#include <FlexLexer.h>
#endif

//class CLexer;
#include "minc_api.hpp"

#include "../tmp/location.hh"
#include "../tmp/cparser.hh"

class CLexer : public yyFlexLexer
{
public:
	CLexer(std::istream& in, std::ostream& out) : yyFlexLexer(in, out) {}
	CLexer(std::istream* in, std::ostream* out) : yyFlexLexer(in, out) {}
	virtual ~CLexer() {}
	using FlexLexer::yylex;
	virtual int yylex(yy::CParser::semantic_type * const lval, yy::CParser::location_type *location);
};

#endif
