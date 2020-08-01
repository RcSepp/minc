// Match scores:
// Score | MincExpr type | MincExpr subtype
// --------------------------------------
// 7     | PLCHLD       | $L<MATCH>
// 6     | ID           |
// 6     | PLCHLD       | $L
// 6     | PLCHLD       | $B
// 6     | PLCHLD       | $P
// 6     | PLCHLD       | $V
// 6     | PLCHLD       | $I<MATCH>
// 5     | PLCHLD       | $E<MATCH>
// 4     | PLCHLD       | $I<CAST>
// 3     | PLCHLD       | $E<CAST>
// 2     | PLCHLD       | $I
// 1     | PLCHLD       | $E
//
// Match score penalties:
// Penalty | Match type
//---------------------
// -1      | Type-cast
// -1      | Ellipsis

//#define DEBUG_STMTREG

#include <assert.h>
#include "minc_api.hpp"

extern unsigned long long EXPR_RESOLVE_COUNTER, STMT_RESOLVE_COUNTER;
unsigned long long EXPR_RESOLVE_COUNTER = 0, STMT_RESOLVE_COUNTER = 0;
#ifdef DEBUG_STMTREG
std::string indent;
#endif

// A mock kernel applied to expressions that can't be resolved, to avoid repeated attempts to resolve those expressions.
//
// Example: Keyword identifiers such as "if" or "while" can't be resolved to symbols. If the kernel of such expressions
// were left at null, subsequent calls to MincExpr::resolve() would keep reattempting to resolve these identifiers.
// Instead we mark the kernel as UNRESOLVABLE_KERNEL.
static struct UnresolvableMincKernel : public MincKernel
{
	MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) { return MincSymbol(nullptr, nullptr); }
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return nullptr; }
} UNRESOLVABLE_KERNEL;

void storeParam(MincExpr* param, std::vector<MincExpr*>& params, size_t paramIdx)
{
	if (paramIdx >= params.size())
		params.push_back(param);
	else
	{
		if (params[paramIdx]->exprtype != MincExpr::ExprType::LIST)
			params[paramIdx] = new MincListExpr('\0', { params[paramIdx] });
		((MincListExpr*)params[paramIdx])->exprs.push_back(param);
	}
}

bool matchStmt(const MincBlockExpr* block, MincExprIter tplt, const MincExprIter tpltEnd, StreamingMincExprIter expr, MatchScore& score, StreamingMincExprIter* stmtEnd=nullptr)
{
	while (tplt != tpltEnd && !expr.done())
	{
		if (tplt[0]->exprtype == MincExpr::ExprType::ELLIPSIS)
		{
			++tplt; // Eat ellipsis

			// Eat multiple end-to-end ellipses
			while (tplt != tpltEnd && tplt[0]->exprtype == MincExpr::ExprType::ELLIPSIS) ++tplt;

			const MincExpr* ellipsis = tplt[-1];

			if (tplt == tpltEnd) // If ellipsis is last template expression
			{
				while (!expr.done() && ellipsis->match(block, expr[0], score)) ++expr; // Match while ellipsis expression matches
			}
			else // If ellipsis is not last template expression
			{
				// Match while ellipsis expression matches and template expression after ellipsis doesn't match
				const MincExpr* ellipsisTerminator = tplt[0];
				while (!expr.done() && ellipsis->match(block, expr[0], score))
				{
					if (ellipsisTerminator->match(block, (expr++)[0], score)
						// At this point both ellipsis and terminator match. Both cases must be handled
						// 1) We handle ellipsis match by continuing the loop
						// 2) We handle terminator match calling matchStmt() starting after the terminator match
						// If case 2 succeeds, return true
						&& matchStmt(block, tplt + 1, tpltEnd, expr, score, stmtEnd))
					{
						return true;
					}
				}
			}
		}
		else if (tplt[0]->exprtype == MincExpr::ExprType::PLCHLD && ((MincPlchldExpr*)tplt[0])->p1 == 'S')
		{
			++tplt; // Eat $S

			StreamingMincExprIter subStmtEnd;
			MatchScore subStmtScore;
			if (block->lookupStmt(expr, expr, subStmtScore).first == nullptr)
				return false;

			if (tplt[0]->exprtype == MincExpr::ExprType::STOP) ++tplt; // Eat STOP as part of $S
		}
		else if (expr[0]->exprtype == MincExpr::ExprType::BINOP && tplt + 1 != tpltEnd)
		{
			MincBinOpExpr* binopExpr = (MincBinOpExpr*)expr[0];

			// Match non-ellipsis template expression against binop expression
			MatchScore binopScore = score;
			if (!tplt[0]->match(block, expr[0], binopScore))
				binopScore = (MatchScore)-0x80000000;

			// Match non-ellipsis template expression against binop.a/binop.b_pre expression
			MatchScore preopScore = score;
			if(!(tplt[0]->match(block, binopExpr->a, preopScore) && tplt[1]->match(block, &binopExpr->b_pre, preopScore)))
				preopScore = (MatchScore)-0x80000000;

			// Match non-ellipsis template expression against binop.a_post/binop.b expression
			MatchScore postopScore = score;
			if(!(tplt[0]->match(block, &binopExpr->a_post, postopScore) && tplt[1]->match(block, binopExpr->b, postopScore)))
				postopScore = (MatchScore)-0x80000000;

			if (binopScore == (MatchScore)-0x80000000 && preopScore == (MatchScore)-0x80000000 && postopScore == (MatchScore)-0x80000000)
				return false; // Neither binop, nor binop.a/binop.b_pre or binop.a_post/binop.b matched

			// Set score to max(binopScore, preopScore, postopScore) and advance tplt and expr pointers
			if (preopScore > binopScore)
			{
				if (postopScore > preopScore)
				{
					score = postopScore;
					++tplt;
				}
				else
				{
					score = preopScore;
					++tplt;
				}
			}
			else
			{
				if (postopScore > binopScore)
				{
					score = postopScore;
					++tplt;
				}
				else
				{
					score = binopScore;
				}
			}
			++tplt, ++expr;
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
	StreamingMincExprIter listExprEnd;
	while (
		tplt != tpltEnd && (
			tplt[0]->exprtype == MincExpr::ExprType::ELLIPSIS ||
			(tplt[0]->exprtype == MincExpr::ExprType::LIST && ((MincListExpr*)tplt[0])->exprs.size() && matchStmt(block, ((MincListExpr*)tplt[0])->exprs.cbegin(), ((MincListExpr*)tplt[0])->exprs.cend(), expr, score, &listExprEnd) && listExprEnd.done())
		)) ++tplt;

	if (stmtEnd)
		*stmtEnd = expr;
	return tplt == tpltEnd; // We have a match if tplt has been fully traversed
}

void collectStmt(const MincBlockExpr* block, MincExprIter tplt, const MincExprIter tpltEnd, StreamingMincExprIter expr, std::vector<MincExpr*>& params, size_t& paramIdx)
{
	MatchScore score;
	while (tplt != tpltEnd && !expr.done())
	{
		if (tplt[0]->exprtype == MincExpr::ExprType::ELLIPSIS)
		{
			++tplt; // Eat ellipsis

			// Eat multiple end-to-end ellipses
			while (tplt != tpltEnd && tplt[0]->exprtype == MincExpr::ExprType::ELLIPSIS) ++tplt;

			const MincExpr* ellipsis = tplt[-1];
			size_t ellipsisBegin = paramIdx;

			if (tplt == tpltEnd) // If ellipsis is last template expression
			{
				while (!expr.done() && ellipsis->match(block, expr[0], score))
				{
					paramIdx = ellipsisBegin;
					ellipsis->collectParams(block, expr[0], params, paramIdx);
					++expr; // Match while ellipsis expression matches
				}
			}
			else // If ellipsis is not last template expression
			{
				// Match while ellipsis expression matches and template expression after ellipsis doesn't match
				const MincExpr* ellipsisTerminator = tplt[0];
				while (!expr.done() && ellipsis->match(block, expr[0], score))
				{
					if (ellipsisTerminator->match(block, expr[0], score)
						// At this point both ellipsis and terminator match. Both cases must be handled
						// 1) We handle ellipsis match by continuing the loop
						// 2) We handle terminator match calling matchStmt() starting after the terminator match
						// If case 2 succeeds, continue collecting after the terminator match
						&& matchStmt(block, tplt + 1, tpltEnd, expr + 1, score))
					{
						// If ellipsisTerminator == $V, collect the ellipsis expression as part of the template ellipsis
						if (expr[0]->exprtype == MincExpr::ExprType::ELLIPSIS)
							ellipsis->collectParams(block, expr[0], params, paramIdx);

						// Replace all non-list parameters that are part of this ellipsis with single-element lists,
						// because ellipsis parameters are expected to always be lists
						for (size_t i = ellipsisBegin; i < paramIdx; ++i)
							if (params[i]->exprtype != MincExpr::ExprType::LIST)
								params[i] = new MincListExpr('\0', { params[i] });

						ellipsisTerminator->collectParams(block, expr[0], params, paramIdx);
						return collectStmt(block, tplt + 1, tpltEnd, expr + 1, params, paramIdx);
					}
					else
					{
						paramIdx = ellipsisBegin;
						ellipsis->collectParams(block, (expr++)[0], params, paramIdx);
					}
				}
			}

			// Replace all non-list parameters that are part of this ellipsis with single-element lists,
			// because ellipsis parameters are expected to always be lists
			for (size_t i = ellipsisBegin; i < paramIdx; ++i)
				if (params[i]->exprtype != MincExpr::ExprType::LIST)
					params[i] = new MincListExpr('\0', { params[i] });
		}
		else if (tplt[0]->exprtype == MincExpr::ExprType::PLCHLD && ((MincPlchldExpr*)tplt[0])->p1 == 'S')
		{
			++tplt; // Eat $S

			StreamingMincExprIter subStmtBegin = expr;
			MatchScore subStmtScore;
			const std::pair<const MincListExpr*, MincKernel*> stmtKernel = block->lookupStmt(subStmtBegin, expr, subStmtScore);
			assert(stmtKernel.first != nullptr);
			MincStmt* subStmt = new MincStmt(subStmtBegin.iter(), expr.iter(), stmtKernel.second);
			size_t subStmtParamIdx = 0;
			collectStmt(block, stmtKernel.first->cbegin(), stmtKernel.first->cend(), subStmtBegin, subStmt->resolvedParams, subStmtParamIdx);
			storeParam(subStmt, params, paramIdx++);

			if (tplt[0]->exprtype == MincExpr::ExprType::STOP) ++tplt; // Eat STOP as part of $S
		}
		else if (expr[0]->exprtype == MincExpr::ExprType::BINOP && tplt + 1 != tpltEnd)
		{
			MincBinOpExpr* binopExpr = (MincBinOpExpr*)expr[0];

			// Match non-ellipsis template expression against binop expression
			MatchScore binopScore = score;
			if (!tplt[0]->match(block, expr[0], binopScore))
				binopScore = (MatchScore)-0x80000000;

			// Match non-ellipsis template expression against binop.a/binop.b_pre expression
			MatchScore preopScore = score;
			if(!(tplt[0]->match(block, binopExpr->a, preopScore) && tplt[1]->match(block, &binopExpr->b_pre, preopScore)))
				preopScore = (MatchScore)-0x80000000;

			// Match non-ellipsis template expression against binop.a_post/binop.b expression
			MatchScore postopScore = score;
			if(!(tplt[0]->match(block, &binopExpr->a_post, postopScore) && tplt[1]->match(block, binopExpr->b, postopScore)))
				postopScore = (MatchScore)-0x80000000;

			// Set score to max(binopScore, preopScore, postopScore) and advance tplt and expr pointers
			if (preopScore > binopScore)
			{
				if (postopScore > preopScore)
				{
					tplt[0]->collectParams(block, &binopExpr->a_post, params, paramIdx);
					(++tplt)[0]->collectParams(block, binopExpr->b, params, paramIdx);
				}
				else
				{
					tplt[0]->collectParams(block, binopExpr->a, params, paramIdx);
					(++tplt)[0]->collectParams(block, &binopExpr->b_pre, params, paramIdx);
				}
			}
			else
			{
				if (postopScore > binopScore)
				{
					tplt[0]->collectParams(block, &binopExpr->a_post, params, paramIdx);
					(++tplt)[0]->collectParams(block, binopExpr->b, params, paramIdx);
				}
				else
				{
					tplt[0]->collectParams(block, expr[0], params, paramIdx);
				}
			}
			++tplt, ++expr;
		}
		else
		{
			// Collect non-ellipsis expression
			tplt[0]->collectParams(block, expr[0], params, paramIdx);
			++tplt, ++expr;
		}
	}

	// Eat unused trailing ellipses and lists only consisting of ellises
	size_t trailingEllipsesBegin = paramIdx;
	while (
		tplt != tpltEnd && (
			tplt[0]->exprtype == MincExpr::ExprType::ELLIPSIS ||
			(tplt[0]->exprtype == MincExpr::ExprType::LIST && ((MincListExpr*)tplt[0])->exprs.size() && matchStmt(block, ((MincListExpr*)tplt[0])->exprs.cbegin(), ((MincListExpr*)tplt[0])->exprs.cend(), expr, score))
		))
	{
		// Match ellipsis expression against itself
		// This will append all trailing template placeholders to params
		MincExpr* ellipsisExpr = ((MincEllipsisExpr*)(tplt[0]->exprtype == MincExpr::ExprType::ELLIPSIS ? tplt[0] : ((MincListExpr*)tplt[0])->exprs[0]))->expr;
		ellipsisExpr->collectParams(block, ellipsisExpr, params, paramIdx);
		++tplt;
	}
	// Replace trailing placeholders with empty lists
	for (size_t i = trailingEllipsesBegin; i < paramIdx; ++i)
		params[i] = new MincListExpr('\0');

	assert(tplt == tpltEnd); // We have a match if tplt has been fully traversed
}

void StatementRegister::defineStmt(const MincListExpr* tplt, MincKernel* stmt)
{
	stmtreg[tplt] = stmt;
}

std::pair<const MincListExpr*, MincKernel*> StatementRegister::lookupStmt(const MincBlockExpr* block, StreamingMincExprIter stmt, StreamingMincExprIter& bestStmtEnd, MatchScore& bestScore) const
{
	MatchScore currentScore;
	StreamingMincExprIter currentStmtEnd;
	std::pair<const MincListExpr*, MincKernel*> bestStmt = {nullptr, nullptr};
	for (const std::pair<const MincListExpr*, MincKernel*>& iter: stmtreg)
	{
#ifdef DEBUG_STMTREG
		printf("%scandidate `%s`", indent.c_str(), iter.first->str().c_str());
#endif
		currentScore = 0;
		if (matchStmt(block, iter.first->exprs.cbegin(), iter.first->exprs.cend(), stmt, currentScore, &currentStmtEnd))
#ifdef DEBUG_STMTREG
		{
#endif
			if (currentScore > bestScore)
			{
				bestScore = currentScore;
				bestStmt = iter;
				bestStmtEnd = currentStmtEnd;
			}
#ifdef DEBUG_STMTREG
			printf(" \e[94mMATCH(score=%i)\e[0m", currentScore);
		}
		printf("\n");
#endif
	}
	if (antiStmt != nullptr && bestScore == -2147483648)
	{
		bestScore = 2147483647;
		return std::pair<const MincListExpr*, MincKernel*>(new MincListExpr('\0'), antiStmt);
	}
	return bestStmt;
}

void StatementRegister::lookupStmtCandidates(const MincBlockExpr* block, const MincListExpr* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates) const
{
	MatchScore score;
	StreamingMincExprIter stmtEnd;
	for (const std::pair<const MincListExpr*, MincKernel*>& iter: stmtreg)
	{
		score = 0;
		if (matchStmt(block, iter.first->exprs.cbegin(), iter.first->exprs.cend(), StreamingMincExprIter(block, &stmt->exprs), score, &stmtEnd) && stmtEnd.done())
			candidates.insert({ score, iter });
	}
}

size_t StatementRegister::countStmts() const
{
	return stmtreg.size();
}

void StatementRegister::iterateStmts(std::function<void(const MincListExpr* tplt, const MincKernel* stmt)> cbk) const
{
	for (const std::pair<const MincListExpr*, MincKernel*>& iter: stmtreg)
		cbk(iter.first, iter.second);
}

void StatementRegister::defineDefaultStmt(MincKernel* stmt)
{
	antiStmt = stmt;
}

void StatementRegister::defineExpr(const MincExpr* tplt, MincKernel* expr)
{
	exprreg[tplt->exprtype][tplt] = expr;
}

std::pair<const MincExpr*, MincKernel*> StatementRegister::lookupExpr(const MincBlockExpr* block, MincExpr* expr, MatchScore& bestScore) const
{
	MatchScore currentScore;
	std::pair<const MincExpr*, MincKernel*> bestExpr = {nullptr, nullptr};
	for (auto& iter: exprreg[expr->exprtype])
	{
#ifdef DEBUG_STMTREG
		printf("%scandidate `%s`", indent.c_str(), iter.first->str().c_str());
#endif
		currentScore = 0;
		if (iter.first->match(block, expr, currentScore))
#ifdef DEBUG_STMTREG
		{
#endif
			if (currentScore > bestScore)
			{
				bestScore = currentScore;
				bestExpr = iter;
			}
#ifdef DEBUG_STMTREG
			printf(" \e[94mMATCH(score=%i)\e[0m", currentScore);
		}
		printf("\n");
#endif
	}

	expr->resolvedParams.push_back(expr); // Set first kernel parameter to self to enable type-aware matching
	for (auto& iter: exprreg[MincExpr::PLCHLD])
	{
#ifdef DEBUG_STMTREG
		printf("%scandidate `%s`", indent.c_str(), iter.first->str().c_str());
#endif
		currentScore = 0;
		expr->resolvedKernel = iter.second; // Set kernel to enable type-aware matching
		if (iter.first->match(block, expr, currentScore))
#ifdef DEBUG_STMTREG
		{
#endif
			if (currentScore > bestScore)
			{
				bestScore = currentScore;
				bestExpr = iter;
			}
#ifdef DEBUG_STMTREG
			printf(" \e[94mMATCH(score=%i)\e[0m", currentScore);
		}
		printf("\n");
#endif
	}
	expr->resolvedParams.pop_back(); // Remove first kernel parameter
	expr->resolvedKernel = nullptr; // Reset kernel

	if (antiExpr != nullptr && bestScore == -2147483648)
	{
		bestScore = 2147483647;
		return std::pair<const MincExpr*, MincKernel*>(nullptr, antiExpr);
	}

	return bestExpr;
}

void StatementRegister::lookupExprCandidates(const MincBlockExpr* block, const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates) const
{
	MatchScore score;
	for (auto& iter: exprreg[expr->exprtype])
	{
		score = 0;
		if (iter.first->match(block, expr, score))
			candidates.insert({ score, iter });
	}
	for (auto& iter: exprreg[MincExpr::PLCHLD])
	{
		score = 0;
		if (iter.first->match(block, expr, score))
			candidates.insert({ score, iter });
	}
}

size_t StatementRegister::countExprs() const
{
	size_t numExprs = 0;
	for (const std::map<const MincExpr*, MincKernel*>& exprreg: this->exprreg)
		numExprs += exprreg.size();
	return numExprs;
}

void StatementRegister::iterateExprs(std::function<void(const MincExpr* tplt, const MincKernel* expr)> cbk) const
{
	for (const std::map<const MincExpr*, MincKernel*>& exprreg: this->exprreg)
		for (const std::pair<const MincExpr*, MincKernel*>& iter: exprreg)
			cbk(iter.first, iter.second);
}

bool MincBlockExpr::lookupExpr(MincExpr* expr) const
{
	++EXPR_RESOLVE_COUNTER;

#ifdef DEBUG_STMTREG
	printf("%slookupExpr(%s)\n", indent.c_str(), expr->str().c_str());
	indent += '\t';
#endif
	expr->resolvedParams.clear();
	MatchScore currentScore, score = -2147483648;
	std::pair<const MincExpr*, MincKernel*> currentKernel, kernel = {nullptr, nullptr};
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		currentScore = score;
		currentKernel = block->stmtreg.lookupExpr(this, expr, currentScore);
		if (currentScore > score)
		{
			kernel = currentKernel;
			score = currentScore;
		}
		for (const MincBlockExpr* ref: block->references)
		{
			currentScore = score;
			currentKernel = ref->stmtreg.lookupExpr(this, expr, currentScore);
			if (currentScore > score)
			{
				kernel = currentKernel;
				score = currentScore;
			}
		}
	}
#ifdef DEBUG_STMTREG
	indent = indent.substr(0, indent.size() - 1);
#endif

	if (kernel.first != nullptr) // If a matching kernel was found, ...
	{
		// Set resolved kernel and collect kernel parameters
		size_t paramIdx = 0;
		if (kernel.first->exprtype == MincExpr::PLCHLD)
		{
			expr->resolvedKernel = kernel.second; // Set kernel before collectParams() to enable type-aware matching
			expr->resolvedParams.push_back(expr); // Set first kernel parameter to self to enable type-aware matching
			std::vector<MincExpr*> collectedParams;
			kernel.first->collectParams(this, expr, collectedParams, paramIdx);
			expr->resolvedParams.pop_back(); // Remove first kernel parameter
			expr->resolvedParams = collectedParams; // Replace parameters with collected parameters
		}
		else
		{
			// Don't set kernel before collectParams(), because resolvedParams are not yet set, which results in undefined behavior when using the kernel
			kernel.first->collectParams(this, expr, expr->resolvedParams, paramIdx);
			expr->resolvedKernel = kernel.second;
		}
		return true;
	}
	else // If no matching kernel was found, ...
	{
		// Mark expr as unresolvable
		expr->resolvedKernel = &UNRESOLVABLE_KERNEL;
		return false;
	}
}

bool MincBlockExpr::lookupStmt(MincExprIter beginExpr, MincStmt& stmt) const
{
	++STMT_RESOLVE_COUNTER;

	// Initialize stmt
	stmt.resolvedParams.clear();
	stmt.begin = beginExpr;

	// Setup streaming expression iterator
	StreamingMincExprIter stmtBegin(this, exprs, beginExpr);

#ifdef DEBUG_STMTREG
	std::vector<MincExpr*> _exprs;
	for (MincExprIter exprIter = beginExpr; exprIter != exprs->cend() && (*exprIter)->exprtype != MincExpr::ExprType::STOP && (*exprIter)->exprtype != MincExpr::ExprType::BLOCK; ++exprIter)
		_exprs.push_back(*exprIter);
	printf("%slookupStmt(%s)\n", indent.c_str(), MincListExpr('\0', _exprs).str().c_str());
	indent += '\t';
#endif

	// Lookup statement in current block and all parents
	// Get kernel of best match
	StreamingMincExprIter stmtEnd;
	MatchScore score;
	std::pair<const MincListExpr*, MincKernel*> kernel = lookupStmt(stmtBegin, stmtEnd, score);

#ifdef DEBUG_STMTREG
	indent = indent.substr(0, indent.size() - 1);
#endif

	// Advance stmt.end to beginning of next statement
	if (stmtEnd - stmtBegin != 0)
	{
		// End of statement = beginning of statement + length of resolved statement + length of trailing STOP expression
		stmt.end = stmt.begin + (stmtEnd - stmtBegin);
		if (stmt.end != exprs->end() && (*stmt.end)->exprtype == MincExpr::ExprType::STOP)
			++stmt.end;
	}
	else // If the statement couldn't be resolved
	{
		// End of statement = beginning of statement + length of unresolved statement
		stmt.end = stmt.begin;
		while (stmt.end != exprs->end() && (*stmt.end)->exprtype != MincExpr::ExprType::STOP && (*stmt.end)->exprtype != MincExpr::ExprType::BLOCK)
			++stmt.end;
		if (stmt.end != exprs->end())
			++stmt.end;
	}

	// Update location
	stmt.loc.filename = stmt.begin[0]->loc.filename;
	stmt.loc.begin_line = stmt.begin[0]->loc.begin_line;
	stmt.loc.begin_column = stmt.begin[0]->loc.begin_column;
	stmt.loc.end_line = stmt.end[-(int)(stmt.end != stmt.begin)]->loc.end_line;
	stmt.loc.end_column = stmt.end[-(int)(stmt.end != stmt.begin)]->loc.end_column;

	if (kernel.first != nullptr) // If a matching kernel was found, ...
	{
		// Set resolved kernel and collect kernel parameters
		size_t paramIdx = 0;
		collectStmt(this, kernel.first->cbegin(), kernel.first->cend(), stmtBegin, stmt.resolvedParams, paramIdx);
		stmt.resolvedKernel = kernel.second;
		return true;
	}
	else // If no matching kernel was found, ...
	{
		// Mark stmt as unresolvable
		stmt.resolvedKernel = &UNRESOLVABLE_KERNEL;
		return false;
	}
}

std::pair<const MincListExpr*, MincKernel*> MincBlockExpr::lookupStmt(StreamingMincExprIter stmt, StreamingMincExprIter& bestStmtEnd, MatchScore& bestScore) const
{
	bestScore = -2147483648;
	MatchScore currentScore;
	StreamingMincExprIter currentStmtEnd;
	std::pair<const MincListExpr*, MincKernel*> currentKernel, bestKernel = {nullptr, nullptr};
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		currentScore = bestScore;
		currentKernel = block->stmtreg.lookupStmt(this, stmt, currentStmtEnd, currentScore);
		if (currentScore > bestScore)
		{
			bestKernel = currentKernel;
			bestScore = currentScore;
			bestStmtEnd = currentStmtEnd;
		}
		for (const MincBlockExpr* ref: block->references)
		{
			currentScore = bestScore;
			currentKernel = ref->stmtreg.lookupStmt(this, stmt, currentStmtEnd, currentScore);
			if (currentScore > bestScore)
			{
				bestKernel = currentKernel;
				bestScore = currentScore;
				bestStmtEnd = currentStmtEnd;
			}
		}
	}
	return bestKernel;
}

std::vector<MincExpr*>::iterator begin(MincListExpr& exprs) { return exprs.begin(); }
std::vector<MincExpr*>::iterator begin(MincListExpr* exprs) { return exprs->begin(); }
std::vector<MincExpr*>::iterator end(MincListExpr& exprs) { return exprs.end(); }
std::vector<MincExpr*>::iterator end(MincListExpr* exprs) { return exprs->end(); }