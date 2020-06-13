#include "minc_api.hpp"

StreamingExprASTIter::StreamingExprASTIter(const std::vector<ExprAST*>* buffer, size_t idx, std::function<bool()> next)
	: buffer(buffer), idx(idx), next(next)
{
	if (buffer != nullptr)
		for(size_t i = buffer->size(); i <= idx; ++i)
			if (!next())
			{
				idx = buffer->size();
				break;
			}
}

bool StreamingExprASTIter::done()
{
	return idx == buffer->size() && !next();
}

ExprAST* StreamingExprASTIter::operator*()
{
	return idx != buffer->size() ? buffer->at(idx) : nullptr;
}

ExprAST* StreamingExprASTIter::operator[](int i)
{
	return idx + i < buffer->size() ? buffer->at(idx + i) : nullptr;
}

size_t StreamingExprASTIter::operator-(const StreamingExprASTIter& other) const
{
	return idx - other.idx;
}

StreamingExprASTIter StreamingExprASTIter::operator+(int n) const
{
	return StreamingExprASTIter(buffer, idx + n, next);
}

StreamingExprASTIter StreamingExprASTIter::operator++(int)
{
	if (done())
		return *this;
	else
		return StreamingExprASTIter(buffer, idx++, next);
}

StreamingExprASTIter& StreamingExprASTIter::operator++()
{
	if (!done())
		++idx;
	return *this;
}

ExprASTIter StreamingExprASTIter::iter() const
{
	return buffer->cbegin() + idx;
}