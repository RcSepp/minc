#include "minc_api.hpp"

StreamingMincExprIter::StreamingMincExprIter()
	: resolveScope(nullptr), exprs(nullptr)
{
}

StreamingMincExprIter::StreamingMincExprIter(const MincBlockExpr* resolveScope, const std::vector<MincExpr*>* exprs)
	: resolveScope(resolveScope), exprs(exprs)
{
	assert(resolveScope != nullptr && exprs != nullptr);
	current = exprs->begin();
}

StreamingMincExprIter::StreamingMincExprIter(const MincBlockExpr* resolveScope, const std::vector<MincExpr*>* exprs, MincExprIter current)
	: resolveScope(resolveScope), exprs(exprs), current(current)
{
	assert(resolveScope != nullptr && exprs != nullptr);
}

bool StreamingMincExprIter::done()
{
	return current == exprs->end();
}

MincExpr* StreamingMincExprIter::operator*()
{
	MincExpr* const expr = *current;
	expr->resolve(resolveScope);
	return expr;
}

MincExpr* StreamingMincExprIter::operator[](int i)
{
	MincExpr* const expr = *(current + i);
	expr->resolve(resolveScope);
	return expr;
}

size_t StreamingMincExprIter::operator-(const StreamingMincExprIter& other) const
{
	return exprs == nullptr || other.exprs == nullptr ? 0 : current - other.current;
}

StreamingMincExprIter StreamingMincExprIter::operator+(int n) const
{
	return StreamingMincExprIter(resolveScope, exprs, current + n);
}

StreamingMincExprIter StreamingMincExprIter::operator++(int)
{
	return StreamingMincExprIter(resolveScope, exprs, current++);
}

StreamingMincExprIter& StreamingMincExprIter::operator++()
{
	++current;
	return *this;
}

MincExprIter StreamingMincExprIter::iter() const
{
	return current;
}