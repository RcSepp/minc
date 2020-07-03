#include "minc_api.hpp"

StreamingMincExprIter::StreamingMincExprIter(const std::vector<MincExpr*>* buffer, size_t idx, std::function<bool()> next)
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

bool StreamingMincExprIter::done()
{
	return idx == buffer->size() && !next();
}

MincExpr* StreamingMincExprIter::operator*()
{
	return idx != buffer->size() ? buffer->at(idx) : nullptr;
}

MincExpr* StreamingMincExprIter::operator[](int i)
{
	return idx + i < buffer->size() ? buffer->at(idx + i) : nullptr;
}

size_t StreamingMincExprIter::operator-(const StreamingMincExprIter& other) const
{
	return idx - other.idx;
}

StreamingMincExprIter StreamingMincExprIter::operator+(int n) const
{
	return StreamingMincExprIter(buffer, idx + n, next);
}

StreamingMincExprIter StreamingMincExprIter::operator++(int)
{
	if (done())
		return *this;
	else
		return StreamingMincExprIter(buffer, idx++, next);
}

StreamingMincExprIter& StreamingMincExprIter::operator++()
{
	if (!done())
		++idx;
	return *this;
}

MincExprIter StreamingMincExprIter::iter() const
{
	return buffer->cbegin() + idx;
}