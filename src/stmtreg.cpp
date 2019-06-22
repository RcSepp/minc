#include <assert.h>
#include "ast.h"

UndefinedStmtException::UndefinedStmtException(const StmtAST* stmt)
	: CompileError("undefined statement " + stmt->str(), stmt->loc) {}
UndefinedExprException::UndefinedExprException(const ExprAST* expr)
	: CompileError("undefined expression " + expr->str(), expr->loc) {}
UndefinedIdentifierException::UndefinedIdentifierException(const IdExprAST* id)
	: CompileError('`' + id->str() + "` was not declared in this scope", id->loc) {}
InvalidTypeException::InvalidTypeException(const PlchldExprAST* plchld)
	: CompileError('`' + std::string(plchld->p2) + "` is not a type", plchld->loc) {}

bool matchStatement(const BlockExprAST* block, std::vector<ExprAST*>::const_iterator tplt, const std::vector<ExprAST*>::const_iterator tpltEnd, std::vector<ExprAST*>::const_iterator expr, const std::vector<ExprAST*>::const_iterator exprEnd)
{
	while (tplt != tpltEnd && expr != exprEnd)
	{
		if (tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS)
		{
			++tplt; // Eat ellipsis

			// Eat multiple end-to-end ellipses
			while (tplt != tpltEnd && tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS) ++tplt;

			const ExprAST* ellipsis = tplt[-1];

			if (tplt == tpltEnd) // If ellipsis is last template expression
			{
				while (expr != exprEnd && ellipsis->match(block, expr[0])) ++expr; // Match while ellipsis expression matches
			}
			else // If ellipsis is not last template expression
			{
				// Match while ellipsis expression matches and template expression after ellipsis doesn't match
				const ExprAST* ellipsisTerminator = tplt[0];
				while (expr != exprEnd && ellipsis->match(block, expr[0]))
				{
					if (ellipsisTerminator->match(block, (expr++)[0])
						// At this point both ellipsis and terminator match. Both cases must be handled
						// 1) We handle ellipsis match by continuing the loop
						// 2) We handle terminator match calling matchStatement() starting after the terminator match
						// If case 2 succeeds, return true
						&& matchStatement(block, tplt + 1, tpltEnd, expr, exprEnd))
						return true;
				}
			}
		}
		else
		{
			// Match non-ellipsis template expression
			if (!tplt[0]->match(block, expr[0]))
				return false;
			++tplt, ++expr;
		}
	}

	// Eat unused trailing ellipses and lists only consisting of ellises
	while (
		tplt != tpltEnd && (
			tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS ||
			tplt[0]->exprtype == ExprAST::ExprType::LIST && matchStatement(block, ((ExprListAST*)tplt[0])->exprs.cbegin(), ((ExprListAST*)tplt[0])->exprs.cend(), expr, expr)
		)) ++tplt;

	return tplt == tpltEnd && expr == exprEnd; // We have a match if both tplt and exprs have been fully traversed
}

void collectStatement(const BlockExprAST* block, std::vector<ExprAST*>::const_iterator tplt, const std::vector<ExprAST*>::const_iterator tpltEnd, std::vector<ExprAST*>::const_iterator expr, const std::vector<ExprAST*>::const_iterator exprEnd, std::vector<ExprAST*>& params)
{
	while (tplt != tpltEnd && expr != exprEnd)
	{
		if (tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS)
		{
			++tplt; // Eat ellipsis

			// Eat multiple end-to-end ellipses
			while (tplt != tpltEnd && tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS) ++tplt;

			const ExprAST* ellipsis = tplt[-1];

			if (tplt == tpltEnd) // If ellipsis is last template expression
			{
				while (expr != exprEnd && ellipsis->match(block, expr[0]))
				{
					ellipsis->collectParams(block, expr[0], params);
					++expr; // Match while ellipsis expression matches
				}
			}
			else // If ellipsis is not last template expression
			{
				// Match while ellipsis expression matches and template expression after ellipsis doesn't match
				const ExprAST* ellipsisTerminator = tplt[0];
				while (expr != exprEnd && ellipsis->match(block, expr[0]))
				{
					ellipsis->collectParams(block, expr[0], params);
					if (ellipsisTerminator->match(block, (expr++)[0])
						// At this point both ellipsis and terminator match. Both cases must be handled
						// 1) We handle ellipsis match by continuing the loop
						// 2) We handle terminator match calling matchStatement() starting after the terminator match
						// If case 2 succeeds, continue collecting after the terminator match
						&& matchStatement(block, tplt + 1, tpltEnd, expr, exprEnd))
						return collectStatement(block, tplt + 1, tpltEnd, expr, exprEnd, params);
				}
			}
		}
		else
		{
			// Collect non-ellipsis expression
			tplt[0]->collectParams(block, expr[0], params);
			++tplt, ++expr;
		}
	}

	// Eat unused trailing ellipses and lists only consisting of ellises
	while (
		tplt != tpltEnd && (
			tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS ||
			tplt[0]->exprtype == ExprAST::ExprType::LIST && matchStatement(block, ((ExprListAST*)tplt[0])->exprs.cbegin(), ((ExprListAST*)tplt[0])->exprs.cend(), expr, expr)
		)) ++tplt;

	assert(tplt == tpltEnd && expr == exprEnd); // We have a match if both tplt and exprs have been fully traversed
}

bool ExprListAST::match(const BlockExprAST* block, const ExprAST* expr) const
{
	return expr->exprtype == this->exprtype && matchStatement(block, this->exprs.cbegin(), this->exprs.cend(), ((ExprListAST*)expr)->exprs.cbegin(), ((ExprListAST*)expr)->exprs.cend());
}

void ExprListAST::collectParams(const BlockExprAST* block, ExprAST* exprs, std::vector<ExprAST*>& params) const
{
	collectStatement(block, this->exprs.cbegin(), this->exprs.cend(), ((ExprListAST*)exprs)->exprs.cbegin(), ((ExprListAST*)exprs)->exprs.cend(), params);
}

const std::pair<const ExprListAST*, IStmtContext*>* StatementRegister::lookupStatement(const BlockExprAST* block, const StmtAST* stmt) const
{
	for (const std::pair<const ExprListAST*, IStmtContext*>& iter: stmtreg)
		if (iter.first->match(block, stmt->exprs))
			return &iter;
	return nullptr;
}

const std::pair<const ExprAST*, IExprContext*>* StatementRegister::lookupExpr(const BlockExprAST* block, const ExprAST* expr) const
{
	for (auto& iter: exprreg[expr->exprtype])
		if (iter.first->match(block, expr))
			return &iter;
	for (auto& iter: exprreg[ExprAST::PLCHLD])
		if (iter.first->match(block, expr))
			return &iter;
	return nullptr;
}

std::vector<ExprAST*>::iterator begin(ExprListAST& exprs) { return exprs.begin(); }
std::vector<ExprAST*>::iterator begin(ExprListAST* exprs) { return exprs->begin(); }
std::vector<ExprAST*>::iterator end(ExprListAST& exprs) { return exprs.end(); }
std::vector<ExprAST*>::iterator end(ExprListAST* exprs) { return exprs->end(); }
