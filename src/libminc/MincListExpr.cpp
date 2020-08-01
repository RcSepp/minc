#include "minc_api.hpp"

bool matchStmt(const MincBlockExpr* block, MincExprIter tplt, const MincExprIter tpltEnd, ResolvingMincExprIter expr, MatchScore& score, ResolvingMincExprIter* stmtEnd=nullptr);
void collectStmt(const MincBlockExpr* block, MincExprIter tplt, const MincExprIter tpltEnd, ResolvingMincExprIter expr, std::vector<MincExpr*>& params, size_t& paramIdx);

MincListExpr::MincListExpr(char separator)
	: MincExpr({0}, MincExpr::ExprType::LIST), separator(separator)
{
}

MincListExpr::MincListExpr(char separator, std::vector<MincExpr*> exprs)
	: MincExpr({0}, MincExpr::ExprType::LIST), separator(separator), exprs(exprs)
{
}

MincSymbol MincListExpr::codegen(MincBlockExpr* parentBlock)
{
	assert(0);
	return MincSymbol(nullptr, nullptr); // Unreachable
}

bool MincListExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	ResolvingMincExprIter listExprEnd;
	if (expr->exprtype == MincExpr::ExprType::LIST && matchStmt(block, this->exprs.cbegin(), this->exprs.cend(), ResolvingMincExprIter(block, &((MincListExpr*)expr)->exprs), score, &listExprEnd) && listExprEnd.done())
		return true;
	if (expr->exprtype != MincExpr::ExprType::LIST && this->exprs.size() == 1)
		return this->exprs[0]->match(block, expr, score);
	return false;
}

void MincListExpr::collectParams(const MincBlockExpr* block, MincExpr* exprs, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	if (exprs->exprtype == MincExpr::ExprType::LIST)
		collectStmt(block, this->exprs.cbegin(), this->exprs.cend(), ResolvingMincExprIter(block, &((MincListExpr*)exprs)->exprs), params, paramIdx);
	else if (exprs->exprtype == MincExpr::ExprType::STMT)
		collectStmt(block, this->exprs.cbegin(), this->exprs.cend(), ResolvingMincExprIter(block, block->exprs, ((MincStmt*)exprs)->begin), params, paramIdx);
	else
		this->exprs[0]->collectParams(block, exprs, params, paramIdx);
}

void MincListExpr::resolve(const MincBlockExpr* block)
{
	if (!isResolved())
		for (auto expr: exprs)
			expr->resolve(block);
}

void MincListExpr::forget()
{
	for (auto expr: exprs)
		expr->forget();
}

std::string MincListExpr::str() const
{
	if (exprs.empty())
		return "";

	std::string s;
	const std::string _(1, ' ');
	switch(separator)
	{
	case '\0': s = _; break;
	case ',': case ';': s = separator + _; break;
	default: s = _ + separator + _; break;
	}

	std::string result = exprs[0]->str();
	for (auto expriter = exprs.begin() + 1; expriter != exprs.end(); ++expriter)
		result += (*expriter)->exprtype == MincExpr::ExprType::STOP ? (*expriter)->str() : s + (*expriter)->str();
	return result;
}

std::string MincListExpr::shortStr() const
{
	if (exprs.empty())
		return "";

	std::string s;
	const std::string _(1, ' ');
	switch(separator)
	{
	case '\0': s = _; break;
	case ',': case ';': s = separator + _; break;
	default: s = _ + separator + _; break;
	}

	std::string result = exprs[0]->shortStr();
	for (auto expriter = exprs.begin() + 1; expriter != exprs.end(); ++expriter)
		result += (*expriter)->exprtype == MincExpr::ExprType::STOP ? (*expriter)->shortStr() : s + (*expriter)->shortStr();
	return result;
}

int MincListExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincListExpr* _other = (const MincListExpr*)other;
	c = (int)this->exprs.size() - (int)_other->exprs.size();
	if (c) return c;
	for (std::vector<MincExpr*>::const_iterator t = this->exprs.cbegin(), o = _other->exprs.cbegin(); t != this->exprs.cend(); ++t, ++o)
	{
		c = (*t)->comp(*o);
		if (c) return c;
	}
	return 0;
}

std::vector<MincExpr*>::iterator MincListExpr::begin()
{
	return exprs.begin();
}

std::vector<MincExpr*>::const_iterator MincListExpr::cbegin() const
{
	return exprs.cbegin();
}

std::vector<MincExpr*>::iterator MincListExpr::end()
{
	return exprs.end();
}

std::vector<MincExpr*>::const_iterator MincListExpr::cend() const
{
	return exprs.cend();
}

size_t MincListExpr::size() const
{
	return exprs.size();
}

MincExpr* MincListExpr::at(size_t index)
{
	return exprs.at(index);
}

const MincExpr* MincListExpr::at(size_t index) const
{
	return exprs.at(index);
}

MincExpr* MincListExpr::operator[](size_t index)
{
	return exprs[index];
}

const MincExpr* MincListExpr::operator[](size_t index) const
{
	return exprs[index];
}

void MincListExpr::push_back(MincExpr* expr)
{
	return exprs.push_back(expr);
}

MincExpr* MincListExpr::clone() const
{
	MincListExpr* clone = new MincListExpr(separator);
	for (MincExpr* expr: this->exprs)
		clone->exprs.push_back(expr->clone());
	return clone;
}