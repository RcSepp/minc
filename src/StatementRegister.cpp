// Match scores:
// Score | ExprAST type | ExprAST subtype
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

#ifdef DEBUG_STMTREG
std::string indent;
#endif

void storeParam(ExprAST* param, std::vector<ExprAST*>& params, size_t paramIdx)
{
	if (paramIdx >= params.size())
		params.push_back(param);
	else
	{
		if (params[paramIdx]->exprtype != ExprAST::ExprType::LIST)
			params[paramIdx] = new ListExprAST('\0', { params[paramIdx] });
		((ListExprAST*)params[paramIdx])->exprs.push_back(param);
	}
}

bool matchStmt(const BlockExprAST* block, ExprASTIter tplt, const ExprASTIter tpltEnd, StreamingExprASTIter expr, MatchScore& score, StreamingExprASTIter* stmtEnd=nullptr)
{
	while (tplt != tpltEnd && !expr.done())
	{
		if (tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS)
		{
			++tplt; // Eat ellipsis

			// Eat multiple end-to-end ellipses
			while (tplt != tpltEnd && tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS) ++tplt;

			const ExprAST* ellipsis = tplt[-1];

			if (tplt == tpltEnd) // If ellipsis is last template expression
			{
				while (!expr.done() && ellipsis->match(block, expr[0], score)) ++expr; // Match while ellipsis expression matches
			}
			else // If ellipsis is not last template expression
			{
				// Match while ellipsis expression matches and template expression after ellipsis doesn't match
				const ExprAST* ellipsisTerminator = tplt[0];
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
		else if (tplt[0]->exprtype == ExprAST::ExprType::PLCHLD && ((PlchldExprAST*)tplt[0])->p1 == 'S')
		{
			++tplt; // Eat $S

			StreamingExprASTIter subStmtEnd;
			MatchScore subStmtScore;
			if (block->lookupStmt(expr, expr, subStmtScore).first == nullptr)
				return false;

			if (tplt[0]->exprtype == ExprAST::ExprType::STOP) ++tplt; // Eat STOP as part of $S
		}
		else if (expr[0]->exprtype == ExprAST::ExprType::BINOP && tplt + 1 != tpltEnd)
		{
			BinOpExprAST* binopExpr = (BinOpExprAST*)expr[0];

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
	StreamingExprASTIter listExprEnd;
	while (
		tplt != tpltEnd && (
			tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS ||
			(tplt[0]->exprtype == ExprAST::ExprType::LIST && ((ListExprAST*)tplt[0])->exprs.size() && matchStmt(block, ((ListExprAST*)tplt[0])->exprs.cbegin(), ((ListExprAST*)tplt[0])->exprs.cend(), expr, score, &listExprEnd) && listExprEnd.done())
		)) ++tplt;

	if (stmtEnd)
		*stmtEnd = expr;
	return tplt == tpltEnd; // We have a match if tplt has been fully traversed
}

void collectStmt(const BlockExprAST* block, ExprASTIter tplt, const ExprASTIter tpltEnd, StreamingExprASTIter expr, std::vector<ExprAST*>& params, size_t& paramIdx)
{
	MatchScore score;
	while (tplt != tpltEnd && !expr.done())
	{
		if (tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS)
		{
			++tplt; // Eat ellipsis

			// Eat multiple end-to-end ellipses
			while (tplt != tpltEnd && tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS) ++tplt;

			const ExprAST* ellipsis = tplt[-1];
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
				const ExprAST* ellipsisTerminator = tplt[0];
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
						if (expr[0]->exprtype == ExprAST::ExprType::ELLIPSIS)
							ellipsis->collectParams(block, expr[0], params, paramIdx);

						// Replace all non-list parameters that are part of this ellipsis with single-element lists,
						// because ellipsis parameters are expected to always be lists
						for (size_t i = ellipsisBegin; i < paramIdx; ++i)
							if (params[i]->exprtype != ExprAST::ExprType::LIST)
								params[i] = new ListExprAST('\0', { params[i] });

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
				if (params[i]->exprtype != ExprAST::ExprType::LIST)
					params[i] = new ListExprAST('\0', { params[i] });
		}
		else if (tplt[0]->exprtype == ExprAST::ExprType::PLCHLD && ((PlchldExprAST*)tplt[0])->p1 == 'S')
		{
			++tplt; // Eat $S

			StreamingExprASTIter subStmtBegin = expr;
			MatchScore subStmtScore;
			const std::pair<const ListExprAST*, CodegenContext*> stmtContext = block->lookupStmt(subStmtBegin, expr, subStmtScore);
			assert(stmtContext.first != nullptr);
			StmtAST* subStmt = new StmtAST(subStmtBegin.iter(), expr.iter(), stmtContext.second);
			size_t subStmtParamIdx = 0;
			collectStmt(block, stmtContext.first->cbegin(), stmtContext.first->cend(), subStmtBegin, subStmt->resolvedParams, subStmtParamIdx);
			storeParam(subStmt, params, paramIdx++);

			if (tplt[0]->exprtype == ExprAST::ExprType::STOP) ++tplt; // Eat STOP as part of $S
		}
		else if (expr[0]->exprtype == ExprAST::ExprType::BINOP && tplt + 1 != tpltEnd)
		{
			BinOpExprAST* binopExpr = (BinOpExprAST*)expr[0];

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
			tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS ||
			(tplt[0]->exprtype == ExprAST::ExprType::LIST && ((ListExprAST*)tplt[0])->exprs.size() && matchStmt(block, ((ListExprAST*)tplt[0])->exprs.cbegin(), ((ListExprAST*)tplt[0])->exprs.cend(), expr, score))
		))
	{
		// Match ellipsis expression against itself
		// This will append all trailing template placeholders to params
		ExprAST* ellipsisExpr = ((EllipsisExprAST*)(tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS ? tplt[0] : ((ListExprAST*)tplt[0])->exprs[0]))->expr;
		ellipsisExpr->collectParams(block, ellipsisExpr, params, paramIdx);
		++tplt;
	}
	// Replace trailing placeholders with empty lists
	for (size_t i = trailingEllipsesBegin; i < paramIdx; ++i)
		params[i] = new ListExprAST('\0');

	assert(tplt == tpltEnd); // We have a match if tplt has been fully traversed
}

void StatementRegister::defineStmt(const ListExprAST* tplt, CodegenContext* stmt)
{
	stmtreg[tplt] = stmt;
}

std::pair<const ListExprAST*, CodegenContext*> StatementRegister::lookupStmt(const BlockExprAST* block, StreamingExprASTIter stmt, StreamingExprASTIter& bestStmtEnd, MatchScore& bestScore) const
{
	MatchScore currentScore;
	StreamingExprASTIter currentStmtEnd;
	std::pair<const ListExprAST*, CodegenContext*> bestStmt = {nullptr, nullptr};
	for (const std::pair<const ListExprAST*, CodegenContext*>& iter: stmtreg)
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
		return std::pair<const ListExprAST*, CodegenContext*>(new ListExprAST('\0'), antiStmt);
	}
	return bestStmt;
}

void StatementRegister::lookupStmtCandidates(const BlockExprAST* block, const ListExprAST* stmt, std::multimap<MatchScore, const std::pair<const ListExprAST*, CodegenContext*>>& candidates) const
{
	MatchScore score;
	StreamingExprASTIter stmtEnd;
	for (const std::pair<const ListExprAST*, CodegenContext*>& iter: stmtreg)
	{
		score = 0;
		if (matchStmt(block, iter.first->exprs.cbegin(), iter.first->exprs.cend(), StreamingExprASTIter(&stmt->exprs), score, &stmtEnd) && stmtEnd.done())
			candidates.insert({ score, iter });
	}
}

size_t StatementRegister::countStmts() const
{
	return stmtreg.size();
}

void StatementRegister::iterateStmts(std::function<void(const ListExprAST* tplt, const CodegenContext* stmt)> cbk) const
{
	for (const std::pair<const ListExprAST*, CodegenContext*>& iter: stmtreg)
		cbk(iter.first, iter.second);
}

void StatementRegister::defineDefaultStmt(CodegenContext* stmt)
{
	antiStmt = stmt;
}

void StatementRegister::defineExpr(const ExprAST* tplt, CodegenContext* expr)
{
	exprreg[tplt->exprtype][tplt] = expr;
}

std::pair<const ExprAST*, CodegenContext*> StatementRegister::lookupExpr(const BlockExprAST* block, ExprAST* expr, MatchScore& bestScore) const
{
	MatchScore currentScore;
	std::pair<const ExprAST*, CodegenContext*> bestExpr = {nullptr, nullptr};
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

	expr->resolvedParams.push_back(expr); // Set first context parameter to self to enable type-aware matching
	for (auto& iter: exprreg[ExprAST::PLCHLD])
	{
#ifdef DEBUG_STMTREG
		printf("%scandidate `%s`", indent.c_str(), iter.first->str().c_str());
#endif
		currentScore = 0;
		expr->resolvedContext = iter.second; // Set context to enable type-aware matching
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
	expr->resolvedParams.pop_back(); // Remove first context parameter
	expr->resolvedContext = nullptr; // Reset context

	if (antiExpr != nullptr && bestScore == -2147483648)
	{
		bestScore = 2147483647;
		return std::pair<const ExprAST*, CodegenContext*>(nullptr, antiExpr);
	}

	return bestExpr;
}

void StatementRegister::lookupExprCandidates(const BlockExprAST* block, const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*, CodegenContext*>>& candidates) const
{
	MatchScore score;
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

size_t StatementRegister::countExprs() const
{
	size_t numExprs = 0;
	for (const std::map<const ExprAST*, CodegenContext*>& exprreg: this->exprreg)
		numExprs += exprreg.size();
	return numExprs;
}

void StatementRegister::iterateExprs(std::function<void(const ExprAST* tplt, const CodegenContext* expr)> cbk) const
{
	for (const std::map<const ExprAST*, CodegenContext*>& exprreg: this->exprreg)
		for (const std::pair<const ExprAST*, CodegenContext*>& iter: exprreg)
			cbk(iter.first, iter.second);
}

bool BlockExprAST::lookupExpr(ExprAST* expr) const
{
#ifdef DEBUG_STMTREG
	printf("%slookupExpr(%s)\n", indent.c_str(), expr->str().c_str());
	indent += '\t';
#endif
	expr->resolvedParams.clear();
	MatchScore currentScore, score = -2147483648;
	std::pair<const ExprAST*, CodegenContext*> currentContext, context = {nullptr, nullptr};
	for (const BlockExprAST* block = this; block; block = block->parent)
	{
		currentScore = score;
		currentContext = block->stmtreg.lookupExpr(this, expr, currentScore);
		if (currentScore > score)
		{
			context = currentContext;
			score = currentScore;
		}
		for (const BlockExprAST* ref: block->references)
		{
			currentScore = score;
			currentContext = ref->stmtreg.lookupExpr(this, expr, currentScore);
			if (currentScore > score)
			{
				context = currentContext;
				score = currentScore;
			}
		}
	}
#ifdef DEBUG_STMTREG
	indent = indent.substr(0, indent.size() - 1);
#endif

	if (context.first != nullptr)
	{
		size_t paramIdx = 0;
		if (context.first->exprtype ==ExprAST::PLCHLD)
		{
			expr->resolvedContext = context.second; // Set context before collectParams() to enable type-aware matching
			expr->resolvedParams.push_back(expr); // Set first context parameter to self to enable type-aware matching
			std::vector<::ExprAST*> collectedParams;
			context.first->collectParams(this, expr, collectedParams, paramIdx);
			expr->resolvedParams.pop_back(); // Remove first context parameter
			expr->resolvedParams = collectedParams; // Replace parameters with collected parameters
		}
		else
		{
			// Don't set context before collectParams(), because resolvedParams are not yet set, which results in undefined behavior when using the context
			context.first->collectParams(this, expr, expr->resolvedParams, paramIdx);
			expr->resolvedContext = context.second;
		}
		return true;
	}
	else
		return false;
}

bool BlockExprAST::lookupStmt(ExprASTIter beginExpr, StmtAST& stmt) const
{
	// Initialize stmt
	stmt.resolvedContext = nullptr;
	stmt.resolvedParams.clear();
	stmt.resolvedExprs.clear();
	stmt.begin = beginExpr;

	// Define a callback to resolve an expression from this block and append it to stmt.resolvedExprs
	stmt.sourceExprPtr = beginExpr;
	auto resolveNextExprs = [&]() -> bool {
		if (stmt.sourceExprPtr != exprs->cend()) // If expressions are available
		{
			// Resolve next expression
			::ExprAST* const clone = (*stmt.sourceExprPtr++)->clone(); //TODO: Make BlockExprAST::exprs a list of const ExprAST's
			clone->resolveTypes(this);
			stmt.resolvedExprs.push_back(clone);
			return true;
		}
		else // If no more expressions are available
			return false;
	};

	// Setup streaming expression iterator
	resolveNextExprs(); // Resolve first expression manually to make sure &stmt.resolvedExprs is valid
	StreamingExprASTIter stmtBegin(&stmt.resolvedExprs, 0, resolveNextExprs);

#ifdef DEBUG_STMTREG
	std::vector<ExprAST*> _exprs;
	for (ExprASTIter exprIter = beginExpr; exprIter != exprs->cend() && (*exprIter)->exprtype != ExprAST::ExprType::STOP && (*exprIter)->exprtype != ExprAST::ExprType::BLOCK; ++exprIter)
		_exprs.push_back(*exprIter);
	printf("%slookupStmt(%s)\n", indent.c_str(), ListExprAST('\0', _exprs).str().c_str());
	indent += '\t';
#endif

	// Lookup statement in current block and all parents
	// Get context of best match
	StreamingExprASTIter stmtEnd;
	MatchScore score;
	std::pair<const ListExprAST*, CodegenContext*> context = lookupStmt(stmtBegin, stmtEnd, score);

#ifdef DEBUG_STMTREG
	indent = indent.substr(0, indent.size() - 1);
#endif

	// Advance stmt.end to beginning of next statement
	if (stmtEnd - stmtBegin != 0)
	{
		// End of statement = beginning of statement + length of resolved statement + length of trailing STOP expression
		stmt.end = stmt.begin + (stmtEnd - stmtBegin);
		if (stmt.end != exprs->end() && (*stmt.end)->exprtype == ExprAST::ExprType::STOP)
			++stmt.end;
	}
	else // If the statement couldn't be resolved
	{
		// End of statement = beginning of statement + length of unresolved statement
		stmt.end = stmt.begin;
		while (stmt.end != exprs->end() && (*stmt.end)->exprtype != ExprAST::ExprType::STOP && (*stmt.end)->exprtype != ExprAST::ExprType::BLOCK)
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

	if (context.first != nullptr)
	{
		size_t paramIdx = 0;
		collectStmt(this, context.first->cbegin(), context.first->cend(), stmtBegin, stmt.resolvedParams, paramIdx);
		stmt.resolvedContext = context.second;
		return true;
	}
	else
		return false;
}

std::pair<const ListExprAST*, CodegenContext*> BlockExprAST::lookupStmt(StreamingExprASTIter stmt, StreamingExprASTIter& bestStmtEnd, MatchScore& bestScore) const
{
	bestScore = -2147483648;
	MatchScore currentScore;
	StreamingExprASTIter currentStmtEnd;
	std::pair<const ListExprAST*, CodegenContext*> currentContext, bestContext = {nullptr, nullptr};
	for (const BlockExprAST* block = this; block; block = block->parent)
	{
		currentScore = bestScore;
		currentContext = block->stmtreg.lookupStmt(this, stmt, currentStmtEnd, currentScore);
		if (currentScore > bestScore)
		{
			bestContext = currentContext;
			bestScore = currentScore;
			bestStmtEnd = currentStmtEnd;
		}
		for (const BlockExprAST* ref: block->references)
		{
			currentScore = bestScore;
			currentContext = ref->stmtreg.lookupStmt(this, stmt, currentStmtEnd, currentScore);
			if (currentScore > bestScore)
			{
				bestContext = currentContext;
				bestScore = currentScore;
				bestStmtEnd = currentStmtEnd;
			}
		}
	}
	return bestContext;
}

std::vector<ExprAST*>::iterator begin(ListExprAST& exprs) { return exprs.begin(); }
std::vector<ExprAST*>::iterator begin(ListExprAST* exprs) { return exprs->begin(); }
std::vector<ExprAST*>::iterator end(ListExprAST& exprs) { return exprs.end(); }
std::vector<ExprAST*>::iterator end(ListExprAST* exprs) { return exprs->end(); }