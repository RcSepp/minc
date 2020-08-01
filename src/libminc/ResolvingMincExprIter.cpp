#include "minc_api.hpp"

ResolvingMincExprIter::ResolvingMincExprIter()
	: resolveScope(nullptr), exprs(nullptr)
{
}

ResolvingMincExprIter::ResolvingMincExprIter(const MincBlockExpr* resolveScope, const std::vector<MincExpr*>* exprs)
	: resolveScope(resolveScope), exprs(exprs)
{
	assert(resolveScope != nullptr && exprs != nullptr);
	current = exprs->begin();
}

ResolvingMincExprIter::ResolvingMincExprIter(const MincBlockExpr* resolveScope, const std::vector<MincExpr*>* exprs, MincExprIter current)
	: resolveScope(resolveScope), exprs(exprs), current(current)
{
	assert(resolveScope != nullptr && exprs != nullptr);
}

bool ResolvingMincExprIter::done()
{
	return current == exprs->end();
}

MincExpr* ResolvingMincExprIter::operator*()
{
	MincExpr* const expr = *current;
	expr->resolve(resolveScope);
	return expr;
}

MincExpr* ResolvingMincExprIter::operator[](int i)
{
	MincExpr* const expr = *(current + i);
	expr->resolve(resolveScope);
	return expr;
}

size_t ResolvingMincExprIter::operator-(const ResolvingMincExprIter& other) const
{
	return exprs == nullptr || other.exprs == nullptr ? 0 : current - other.current;
}

ResolvingMincExprIter ResolvingMincExprIter::operator+(int n) const
{
	return ResolvingMincExprIter(resolveScope, exprs, current + n);
}

ResolvingMincExprIter ResolvingMincExprIter::operator++(int)
{
	return ResolvingMincExprIter(resolveScope, exprs, current++);
}

ResolvingMincExprIter& ResolvingMincExprIter::operator++()
{
	++current;
	return *this;
}

MincExprIter ResolvingMincExprIter::iter() const
{
	return current;
}