//#define DEBUG_STMTREG

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

#ifdef DEBUG_STMTREG
std::string indent;
#endif

bool matchStatement(const BlockExprAST* block, std::vector<ExprAST*>::const_iterator tplt, const std::vector<ExprAST*>::const_iterator tpltEnd, std::vector<ExprAST*>::const_iterator expr, const std::vector<ExprAST*>::const_iterator exprEnd, MatchScore& score)
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
				while (expr != exprEnd && ellipsis->match(block, expr[0], score)) ++expr; // Match while ellipsis expression matches
			}
			else // If ellipsis is not last template expression
			{
				// Match while ellipsis expression matches and template expression after ellipsis doesn't match
				const ExprAST* ellipsisTerminator = tplt[0];
				while (expr != exprEnd && ellipsis->match(block, expr[0], score))
				{
					if (ellipsisTerminator->match(block, (expr++)[0], score)
						// At this point both ellipsis and terminator match. Both cases must be handled
						// 1) We handle ellipsis match by continuing the loop
						// 2) We handle terminator match calling matchStatement() starting after the terminator match
						// If case 2 succeeds, return true
						&& matchStatement(block, tplt + 1, tpltEnd, expr, exprEnd, score))
						return true;
				}
			}
		}
		else
		{
			// Match non-ellipsis template expression
			if (!tplt[0]->match(block, expr[0], score))
				return false;
			++tplt, ++expr;
		}
	}

	// Eat unused trailing ellipses and lists only consisting of ellises
	while (
		tplt != tpltEnd && (
			tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS ||
			tplt[0]->exprtype == ExprAST::ExprType::LIST && matchStatement(block, ((ExprListAST*)tplt[0])->exprs.cbegin(), ((ExprListAST*)tplt[0])->exprs.cend(), expr, expr, score)
		)) ++tplt;

	return tplt == tpltEnd && expr == exprEnd; // We have a match if both tplt and exprs have been fully traversed
}

void collectStatement(const BlockExprAST* block, std::vector<ExprAST*>::const_iterator tplt, const std::vector<ExprAST*>::const_iterator tpltEnd, std::vector<ExprAST*>::const_iterator expr, const std::vector<ExprAST*>::const_iterator exprEnd, std::vector<ExprAST*>& params)
{
	MatchScore score;
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
				while (expr != exprEnd && ellipsis->match(block, expr[0], score))
				{
					ellipsis->collectParams(block, expr[0], params);
					++expr; // Match while ellipsis expression matches
				}
			}
			else // If ellipsis is not last template expression
			{
				// Match while ellipsis expression matches and template expression after ellipsis doesn't match
				const ExprAST* ellipsisTerminator = tplt[0];
				while (expr != exprEnd && ellipsis->match(block, expr[0], score))
				{
					ellipsis->collectParams(block, expr[0], params);
					if (ellipsisTerminator->match(block, (expr++)[0], score)
						// At this point both ellipsis and terminator match. Both cases must be handled
						// 1) We handle ellipsis match by continuing the loop
						// 2) We handle terminator match calling matchStatement() starting after the terminator match
						// If case 2 succeeds, continue collecting after the terminator match
						&& matchStatement(block, tplt + 1, tpltEnd, expr, exprEnd, score))
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
			tplt[0]->exprtype == ExprAST::ExprType::LIST && matchStatement(block, ((ExprListAST*)tplt[0])->exprs.cbegin(), ((ExprListAST*)tplt[0])->exprs.cend(), expr, expr, score)
		)) ++tplt;

	assert(tplt == tpltEnd && expr == exprEnd); // We have a match if both tplt and exprs have been fully traversed
}

bool ExprListAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && matchStatement(block, this->exprs.cbegin(), this->exprs.cend(), ((ExprListAST*)expr)->exprs.cbegin(), ((ExprListAST*)expr)->exprs.cend(), score);
}

void ExprListAST::collectParams(const BlockExprAST* block, ExprAST* exprs, std::vector<ExprAST*>& params) const
{
	collectStatement(block, this->exprs.cbegin(), this->exprs.cend(), ((ExprListAST*)exprs)->exprs.cbegin(), ((ExprListAST*)exprs)->exprs.cend(), params);
}

const std::pair<const ExprListAST*, IStmtContext*>* StatementRegister::lookupStatement(const BlockExprAST* block, const StmtAST* stmt) const
{
#ifdef DEBUG_STMTREG
	printf("%slookupStmt(%s)\n", indent.c_str(), stmt->str().c_str());
	indent += '\t';
#endif
	MatchScore currentScore, bestScore = -2147483648;
	const std::pair<const ExprListAST*, IStmtContext*>* bestStmt = nullptr;
	for (const std::pair<const ExprListAST*, IStmtContext*>& iter: stmtreg)
	{
#ifdef DEBUG_STMTREG
		printf("%scandidate `%s`", indent.c_str(), iter.first->str().c_str());
#endif
		currentScore = 0;
		if (iter.first->match(block, stmt->exprs, currentScore) && currentScore > bestScore)
		{
			bestScore = currentScore;
			bestStmt = &iter;
#ifdef DEBUG_STMTREG
			printf(" MATCH(score=%i)", currentScore);
#endif
		}
#ifdef DEBUG_STMTREG
		printf("\n");
#endif
	}
#ifdef DEBUG_STMTREG
	indent = indent.substr(0, indent.size() - 1);
#endif
	return bestStmt;
}

const std::pair<const ExprAST*, IExprContext*>* StatementRegister::lookupExpr(const BlockExprAST* block, const ExprAST* expr) const
{
#ifdef DEBUG_STMTREG
	printf("%slookupExpr(%s)\n", indent.c_str(), expr->str().c_str());
	indent += '\t';
#endif
	MatchScore currentScore, bestScore = -2147483648;
	const std::pair<const ExprAST*, IExprContext*>* bestStmt = nullptr;
	for (auto& iter: exprreg[expr->exprtype])
	{
#ifdef DEBUG_STMTREG
		printf("%scandidate `%s`", indent.c_str(), iter.first->str().c_str());
#endif
		currentScore = 0;
		if (iter.first->match(block, expr, currentScore) && currentScore > bestScore)
		{
			bestScore = currentScore;
			bestStmt = &iter;
#ifdef DEBUG_STMTREG
			printf(" MATCH(score=%i)", currentScore);
#endif
		}
#ifdef DEBUG_STMTREG
		printf("\n");
#endif
	}
	for (auto& iter: exprreg[ExprAST::PLCHLD])
	{
#ifdef DEBUG_STMTREG
		printf("%scandidate `%s`", indent.c_str(), iter.first->str().c_str());
#endif
		currentScore = 0;
		if (iter.first->match(block, expr, currentScore) && currentScore > bestScore)
		{
			bestScore = currentScore;
			bestStmt = &iter;
#ifdef DEBUG_STMTREG
			printf(" MATCH(score=%i)", currentScore);
#endif
		}
#ifdef DEBUG_STMTREG
		printf("\n");
#endif
	}
#ifdef DEBUG_STMTREG
	indent = indent.substr(0, indent.size() - 1);
#endif
	return bestStmt;
}

void StatementRegister::lookupExprCandidates(const BlockExprAST* block, const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*, IExprContext*>&>& candidates) const
{
	MatchScore score;
	const std::pair<const ExprAST*, IExprContext*>* bestStmt = nullptr;
	for (auto& iter: exprreg[expr->exprtype])
	{
		score = 0;
		if (iter.first->match(block, expr, score))
			candidates.insert({ score, iter });
	}
	for (auto& iter: exprreg[ExprAST::PLCHLD])
	{
		score = 0;
		if (iter.first->match(block, expr, score))
			candidates.insert({ score, iter });
	}
}

bool PlchldExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	if (p1 == 'E')
	{
		score -= 1; // Penalize vague template
		return true;
	}

	if (expr->exprtype == ExprAST::ExprType::PLCHLD)
		return ((PlchldExprAST*)expr)->p1 == p1 && strcmp(((PlchldExprAST*)expr)->p2, p2) == 0;

	const XXXValue* var;
	switch(p1)
	{
	case 'I': return expr->exprtype == ExprAST::ExprType::ID;
	case 'L': return expr->exprtype == ExprAST::ExprType::LITERAL;
	case 'B': return expr->exprtype == ExprAST::ExprType::BLOCK;
	case 'P': return expr->exprtype == ExprAST::ExprType::PLCHLD;
	case '\0':
	{
		BaseType* exprType = expr->getType(block);
		BaseType* tpltType = getType(block);
		if (exprType == tpltType)
		{
			score += 1; // Reward exact match
			return true;
		}
		score -= 1; // Penalize implicit cast
		return block->lookupCast(exprType, tpltType) != nullptr;
	}
	default: throw CompileError(std::string("Invalid placeholder: $") + p1, loc);
	}
}

void PlchldExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const
{
	if (p1 == '\0')
	{
		BaseType* exprType = expr->getType(block);
		BaseType* tpltType = getType(block);
		if (exprType != tpltType)
		{
//printf("implicit cast from %s to %s in %s:%i\n", ((BuiltinType*)exprType)->name, ((BuiltinType*)tpltType)->name, expr->loc.filename, expr->loc.begin_line);
			IExprContext* castContext = block->lookupCast(exprType, tpltType);
			assert(castContext != nullptr);
			ExprAST* castExpr = new PlchldExprAST(expr->loc, this->p2);
			castExpr->resolvedContext = castContext;
			castExpr->resolvedParams.push_back(expr);
			params.push_back(castExpr);
			return;
		}
	}
	params.push_back(expr);
}

std::vector<ExprAST*>::iterator begin(ExprListAST& exprs) { return exprs.begin(); }
std::vector<ExprAST*>::iterator begin(ExprListAST* exprs) { return exprs->begin(); }
std::vector<ExprAST*>::iterator end(ExprListAST& exprs) { return exprs.end(); }
std::vector<ExprAST*>::iterator end(ExprListAST* exprs) { return exprs->end(); }
