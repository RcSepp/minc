#include "minc_api.hpp"

bool matchStmt(const BlockExprAST* block, ExprASTIter tplt, const ExprASTIter tpltEnd, StreamingExprASTIter expr, MatchScore& score, StreamingExprASTIter* stmtEnd=nullptr);
void collectStmt(const BlockExprAST* block, ExprASTIter tplt, const ExprASTIter tpltEnd, StreamingExprASTIter expr, std::vector<ExprAST*>& params, size_t& paramIdx);

ListExprAST::ListExprAST(char separator)
	: ExprAST({0}, ExprAST::ExprType::LIST), separator(separator)
{
}

ListExprAST::ListExprAST(char separator, std::vector<ExprAST*> exprs)
	: ExprAST({0}, ExprAST::ExprType::LIST), separator(separator), exprs(exprs)
{
}

Variable ListExprAST::codegen(BlockExprAST* parentBlock)
{
	assert(0);
	return Variable(nullptr, nullptr); // Unreachable
}

bool ListExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	StreamingExprASTIter listExprEnd;
	if (expr->exprtype == ExprAST::ExprType::LIST && matchStmt(block, this->exprs.cbegin(), this->exprs.cend(), StreamingExprASTIter(&((ListExprAST*)expr)->exprs), score, &listExprEnd) && listExprEnd.done())
		return true;
	if (expr->exprtype != ExprAST::ExprType::LIST && this->exprs.size() == 1)
		return this->exprs[0]->match(block, expr, score);
	return false;
}

void ListExprAST::collectParams(const BlockExprAST* block, ExprAST* exprs, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	if (exprs->exprtype == ExprAST::ExprType::LIST)
		collectStmt(block, this->exprs.cbegin(), this->exprs.cend(), StreamingExprASTIter(&((ListExprAST*)exprs)->exprs), params, paramIdx);
	else if (exprs->exprtype == ExprAST::ExprType::STMT)
		collectStmt(block, this->exprs.cbegin(), this->exprs.cend(), StreamingExprASTIter(&((StmtAST*)exprs)->resolvedExprs), params, paramIdx);
	else
		this->exprs[0]->collectParams(block, exprs, params, paramIdx);
}

void ListExprAST::resolveTypes(const BlockExprAST* block)
{
	for (auto expr: exprs)
		expr->resolveTypes(block);
}

std::string ListExprAST::str() const
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
		result += (*expriter)->exprtype == ExprAST::ExprType::STOP ? (*expriter)->str() : s + (*expriter)->str();
	return result;
}

int ListExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const ListExprAST* _other = (const ListExprAST*)other;
	c = (int)this->exprs.size() - (int)_other->exprs.size();
	if (c) return c;
	for (std::vector<ExprAST*>::const_iterator t = this->exprs.cbegin(), o = _other->exprs.cbegin(); t != this->exprs.cend(); ++t, ++o)
	{
		c = (*t)->comp(*o);
		if (c) return c;
	}
	return 0;
}

std::vector<ExprAST*>::iterator ListExprAST::begin()
{
	return exprs.begin();
}

std::vector<ExprAST*>::const_iterator ListExprAST::cbegin() const
{
	return exprs.cbegin();
}

std::vector<ExprAST*>::iterator ListExprAST::end()
{
	return exprs.end();
}

std::vector<ExprAST*>::const_iterator ListExprAST::cend() const
{
	return exprs.cend();
}

size_t ListExprAST::size() const
{
	return exprs.size();
}

ExprAST* ListExprAST::at(size_t index)
{
	return exprs.at(index);
}

const ExprAST* ListExprAST::at(size_t index) const
{
	return exprs.at(index);
}

ExprAST* ListExprAST::operator[](size_t index)
{
	return exprs[index];
}

const ExprAST* ListExprAST::operator[](size_t index) const
{
	return exprs[index];
}

void ListExprAST::push_back(ExprAST* expr)
{
	return exprs.push_back(expr);
}

ExprAST* ListExprAST::clone() const
{
	ListExprAST* clone = new ListExprAST(separator);
	for (ExprAST* expr: this->exprs)
		clone->exprs.push_back(expr->clone());
	return clone;
}