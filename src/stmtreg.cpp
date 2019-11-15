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

void storeParam(ExprAST* param, std::vector<ExprAST*>& params, size_t paramIdx)
{
	if (paramIdx >= params.size())
		params.push_back(param);
	else
	{
		if (params[paramIdx]->exprtype != ExprAST::ExprType::LIST)
			params[paramIdx] = new ExprListAST('\0', { params[paramIdx] });
		((ExprListAST*)params[paramIdx])->exprs.push_back(param);
	}
}

bool matchStatement(const BlockExprAST* block, ExprASTIter tplt, const ExprASTIter tpltEnd, ExprASTIter expr, const ExprASTIter exprEnd, MatchScore& score, ExprASTIter* stmtEnd=nullptr)
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
					{
						if (stmtEnd)
							*stmtEnd = expr;
						return true;
					}
				}
			}
		}
		else if (tplt[0]->exprtype == ExprAST::ExprType::PLCHLD && ((PlchldExprAST*)tplt[0])->p1 == 'S')
		{
			++tplt; // Eat $S

			if (!block->lookupStatement(expr, exprEnd))
				return false;

			if (tplt[0]->exprtype == ExprAST::ExprType::STOP) ++tplt; // Eat STOP as part of $S
		}
		else if (expr[0]->exprtype == ExprAST::ExprType::BINOP && tplt + 1 != tpltEnd)
		{
			BinOpExprAST* binopExpr = (BinOpExprAST*)expr[0];

			// Match non-ellipsis template expression against binop expression
			MatchScore binopScore = score;
			if (!tplt[0]->match(block, expr[0], binopScore))
				binopScore = -0x80000000;

			// Match non-ellipsis template expression against binop.a/binop.b_pre expression
			MatchScore preopScore = score;
			if(!(tplt[0]->match(block, binopExpr->a, preopScore) && tplt[1]->match(block, &binopExpr->b_pre, preopScore)))
				preopScore = -0x80000000;

			// Match non-ellipsis template expression against binop.a_post/binop.b expression
			MatchScore postopScore = score;
			if(!(tplt[0]->match(block, &binopExpr->a_post, postopScore) && tplt[1]->match(block, binopExpr->b, postopScore)))
				postopScore = -0x80000000;

			if (binopScore == -0x80000000 && preopScore == -0x80000000 && postopScore == -0x80000000)
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
	ExprASTIter listExprEnd;
	while (
		tplt != tpltEnd && (
			tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS ||
			tplt[0]->exprtype == ExprAST::ExprType::LIST && matchStatement(block, ((ExprListAST*)tplt[0])->exprs.cbegin(), ((ExprListAST*)tplt[0])->exprs.cend(), expr, exprEnd, score, &listExprEnd) && listExprEnd == exprEnd
		)) ++tplt;

	if (stmtEnd)
		*stmtEnd = expr;
	return tplt == tpltEnd; // We have a match if tplt has been fully traversed
}

void collectStatement(const BlockExprAST* block, ExprASTIter tplt, const ExprASTIter tpltEnd, ExprASTIter expr, const ExprASTIter exprEnd, std::vector<ExprAST*>& params, size_t& paramIdx)
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
			size_t ellipsisBegin = paramIdx;

			if (tplt == tpltEnd) // If ellipsis is last template expression
			{
				while (expr != exprEnd && ellipsis->match(block, expr[0], score))
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
				while (expr != exprEnd && ellipsis->match(block, expr[0], score))
				{
					if (ellipsisTerminator->match(block, expr[0], score)
						// At this point both ellipsis and terminator match. Both cases must be handled
						// 1) We handle ellipsis match by continuing the loop
						// 2) We handle terminator match calling matchStatement() starting after the terminator match
						// If case 2 succeeds, continue collecting after the terminator match
						&& matchStatement(block, tplt + 1, tpltEnd, expr + 1, exprEnd, score))
					{
						// If ellipsisTerminator == $V, collect the ellipsis expression as part of the template ellipsis
						if (expr[0]->exprtype == ExprAST::ExprType::ELLIPSIS)
							ellipsis->collectParams(block, expr[0], params, paramIdx);

						// Replace all non-list parameters that are part of this ellipsis with single-element lists,
						// because ellipsis parameters are expected to always be lists
						for (size_t i = ellipsisBegin; i < paramIdx; ++i)
							if (params[i]->exprtype != ExprAST::ExprType::LIST)
								params[i] = new ExprListAST('\0', { params[i] });

						ellipsisTerminator->collectParams(block, expr[0], params, paramIdx);
						return collectStatement(block, tplt + 1, tpltEnd, expr + 1, exprEnd, params, paramIdx);
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
					params[i] = new ExprListAST('\0', { params[i] });
		}
		else if (tplt[0]->exprtype == ExprAST::ExprType::PLCHLD && ((PlchldExprAST*)tplt[0])->p1 == 'S')
		{
			++tplt; // Eat $S

			const ExprASTIter beginExpr = expr;
			const std::pair<const ExprListAST, CodegenContext*>* stmtContext = block->lookupStatement(expr, exprEnd);
			const ExprASTIter endExpr = expr;
			assert(stmtContext);
			StmtAST* stmt = new StmtAST(beginExpr, endExpr, stmtContext->second);
			stmt->collectParams(block, stmtContext->first);
			storeParam(stmt, params, paramIdx++);

			if (tplt[0]->exprtype == ExprAST::ExprType::STOP) ++tplt; // Eat STOP as part of $S
		}
		else if (expr[0]->exprtype == ExprAST::ExprType::BINOP && tplt + 1 != tpltEnd)
		{
			BinOpExprAST* binopExpr = (BinOpExprAST*)expr[0];

			// Match non-ellipsis template expression against binop expression
			MatchScore binopScore = score;
			if (!tplt[0]->match(block, expr[0], binopScore))
				binopScore = -0x80000000;

			// Match non-ellipsis template expression against binop.a/binop.b_pre expression
			MatchScore preopScore = score;
			if(!(tplt[0]->match(block, binopExpr->a, preopScore) && tplt[1]->match(block, &binopExpr->b_pre, preopScore)))
				preopScore = -0x80000000;

			// Match non-ellipsis template expression against binop.a_post/binop.b expression
			MatchScore postopScore = score;
			if(!(tplt[0]->match(block, &binopExpr->a_post, postopScore) && tplt[1]->match(block, binopExpr->b, postopScore)))
				postopScore = -0x80000000;

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
			tplt[0]->exprtype == ExprAST::ExprType::LIST && matchStatement(block, ((ExprListAST*)tplt[0])->exprs.cbegin(), ((ExprListAST*)tplt[0])->exprs.cend(), expr, exprEnd, score)
		))
	{
		// Match ellipsis expression against itself
		// This will append all trailing template placeholders to params
		ExprAST* ellipsisExpr = ((EllipsisExprAST*)(tplt[0]->exprtype == ExprAST::ExprType::ELLIPSIS ? tplt[0] : ((ExprListAST*)tplt[0])->exprs[0]))->expr;
		ellipsisExpr->collectParams(block, ellipsisExpr, params, paramIdx);
		++tplt;
	}
	// Replace trailing placeholders with empty lists
	for (size_t i = trailingEllipsesBegin; i < paramIdx; ++i)
		params[i] = new ExprListAST('\0');

	assert(tplt == tpltEnd); // We have a match if tplt has been fully traversed
}

bool ExprListAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	ExprASTIter listExprEnd;
	if (expr->exprtype == ExprAST::ExprType::LIST && matchStatement(block, this->exprs.cbegin(), this->exprs.cend(), ((ExprListAST*)expr)->exprs.cbegin(), ((ExprListAST*)expr)->exprs.cend(), score, &listExprEnd) && listExprEnd == ((ExprListAST*)expr)->exprs.cend())
		return true;
	if (expr->exprtype != ExprAST::ExprType::LIST && this->exprs.size() == 1)
		return this->exprs[0]->match(block, expr, score);
	return false;
}

void ExprListAST::collectParams(const BlockExprAST* block, ExprAST* exprs, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	if (exprs->exprtype == ExprAST::ExprType::LIST)
		collectStatement(block, this->exprs.cbegin(), this->exprs.cend(), ((ExprListAST*)exprs)->exprs.cbegin(), ((ExprListAST*)exprs)->exprs.cend(), params, paramIdx);
	else
		this->exprs[0]->collectParams(block, exprs, params, paramIdx);
}

void StmtAST::collectParams(const BlockExprAST* block, const ExprListAST& tplt)
{
	size_t paramIdx = 0;
	collectStatement(block, tplt.cbegin(), tplt.cend(), begin, end, resolvedParams, paramIdx);
}

const std::pair<const ExprListAST, CodegenContext*>* StatementRegister::lookupStatement(const BlockExprAST* block, const ExprASTIter stmt, ExprASTIter& bestStmtEnd, MatchScore& bestScore) const
{
	MatchScore currentScore;
	ExprASTIter currentStmtEnd, stmtEnd = bestStmtEnd;
	const std::pair<const ExprListAST, CodegenContext*>* bestStmt = nullptr;
	for (const std::pair<const ExprListAST, CodegenContext*>& iter: stmtreg)
	{
#ifdef DEBUG_STMTREG
		printf("%scandidate `%s`", indent.c_str(), iter.first.str().c_str());
#endif
		currentScore = 0;
		if (matchStatement(block, iter.first.exprs.cbegin(), iter.first.exprs.cend(), stmt, stmtEnd, currentScore, &currentStmtEnd))
#ifdef DEBUG_STMTREG
		{
#endif
			if (currentScore > bestScore)
			{
				bestScore = currentScore;
				bestStmt = &iter;
				bestStmtEnd = currentStmtEnd;
			}
#ifdef DEBUG_STMTREG
			printf(" \e[94mMATCH(score=%i)\e[0m", currentScore);
		}
		printf("\n");
#endif
	}
	return bestStmt;
}

const std::pair<const ExprAST*const, CodegenContext*>* StatementRegister::lookupExpr(const BlockExprAST* block, const ExprAST* expr, MatchScore& bestScore) const
{
	MatchScore currentScore;
	const std::pair<const ExprAST*const, CodegenContext*>* bestExpr = nullptr;
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
				bestExpr = &iter;
			}
#ifdef DEBUG_STMTREG
			printf(" \e[94mMATCH(score=%i)\e[0m", currentScore);
		}
		printf("\n");
#endif
	}
	for (auto& iter: exprreg[ExprAST::PLCHLD])
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
				bestExpr = &iter;
			}
#ifdef DEBUG_STMTREG
			printf(" \e[94mMATCH(score=%i)\e[0m", currentScore);
		}
		printf("\n");
#endif
	}
	return bestExpr;
}

void StatementRegister::lookupExprCandidates(const BlockExprAST* block, const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*const, CodegenContext*>&>& candidates) const
{
	MatchScore score;
	const std::pair<const ExprAST*const, CodegenContext*>* bestStmt = nullptr;
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
	const std::pair<const ExprAST*const, CodegenContext*> *currentContext, *context = nullptr;
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

	if (context)
	{
		size_t paramIdx = 0;
		context->first->collectParams(this, expr, expr->resolvedParams, paramIdx);
		expr->resolvedContext = context->second;
		return true;
	}
	else
		return false;
}

const std::pair<const ExprListAST, CodegenContext*>* BlockExprAST::lookupStatement(ExprASTIter& exprs, const ExprASTIter exprEnd) const
{
//TODO: Figure out logic for looking up expressions ahead of looking up statements
for (ExprASTIter exprIter = exprs; exprIter != exprEnd && (*exprIter)->exprtype != ExprAST::ExprType::STOP && (*exprIter)->exprtype != ExprAST::ExprType::BLOCK; ++exprIter)
	if ((*exprIter)->resolvedContext == nullptr)
		(*exprIter)->resolveTypes(const_cast<BlockExprAST*>(this));

#ifdef DEBUG_STMTREG
	std::vector<ExprAST*> _exprs;
	for (ExprASTIter exprIter = exprs; exprIter != exprEnd && (*exprIter)->exprtype != ExprAST::ExprType::STOP && (*exprIter)->exprtype != ExprAST::ExprType::BLOCK; ++exprIter)
		_exprs.push_back(*exprIter);
	printf("%slookupStmt(%s)\n", indent.c_str(), ExprListAST('\0', _exprs).str().c_str());
	indent += '\t';
#endif

	// Lookup statement in current block and all parents
	// Get context of best match
	MatchScore currentScore, score = -2147483648;
	ExprASTIter currentStmtEnd, stmtEnd;
	const std::pair<const ExprListAST, CodegenContext*> *currentContext, *context = nullptr;
	for (const BlockExprAST* block = this; block; block = block->parent)
	{
		currentScore = score;
		currentStmtEnd = exprEnd;
		currentContext = block->stmtreg.lookupStatement(this, exprs, currentStmtEnd, currentScore);
		if (currentScore > score)
		{
			context = currentContext;
			score = currentScore;
			stmtEnd = currentStmtEnd;
		}
		for (const BlockExprAST* ref: block->references)
		{
			currentScore = score;
			currentStmtEnd = exprEnd;
			currentContext = ref->stmtreg.lookupStatement(this, exprs, currentStmtEnd, currentScore);
			if (currentScore > score)
			{
				context = currentContext;
				score = currentScore;
				stmtEnd = currentStmtEnd;
			}
		}
	}
#ifdef DEBUG_STMTREG
	indent = indent.substr(0, indent.size() - 1);
#endif

	// Advance exprs parameter to beginning of next statement
	if (context)
		exprs = stmtEnd;
	else
		while (exprs != exprEnd && (*exprs)->exprtype != ExprAST::ExprType::STOP && (*exprs++)->exprtype != ExprAST::ExprType::BLOCK) {}

	return context;
}

bool IdExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	score += 6; // Reward exact match (score is disregarded on mismatch)
	return expr->exprtype == this->exprtype && strcmp(((IdExprAST*)expr)->name, this->name) == 0;
}

bool PlchldExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	// Make sure collectParams(tplt, tplt) collects all placeholders
	if (expr == this)
		return true;

	switch(p1)
	{
	case 'I': if (expr->exprtype != ExprAST::ExprType::ID) return false;
		score += 1; // Reward $I (over $E or $S)
		break;
	case 'L': if (expr->exprtype != ExprAST::ExprType::LITERAL) return false;
		if (p2 == nullptr)
		{
			score += 6; // Reward vague match
			return true;
		}
		else
		{
			score += 7; // Reward exact match
			const char* value = ((const LiteralExprAST*)expr)->value;
			const char* valueEnd = value + strlen(value) - 1;
			if (*valueEnd == '"' || *valueEnd == '\'')
			{
				size_t prefixLen = strchr(value, *valueEnd) - value;
				return strncmp(p2, value, prefixLen) == 0 && p2[prefixLen] == '\0';
			}
			else
			{
				for (value = valueEnd; *value != '-' && (*value < '0' || *value > '9'); --value) {}
				return strcmp(p2, ++value) == 0;
			}
		}
	case 'B': score += 6; return expr->exprtype == ExprAST::ExprType::BLOCK;
	case 'P': score += 6; return expr->exprtype == ExprAST::ExprType::PLCHLD;
	case 'V': score += 6; return expr->exprtype == ExprAST::ExprType::ELLIPSIS;
	case 'E':
	case 'S':
		break;
	default: throw CompileError(std::string("Invalid placeholder: $") + p1, loc);
	}

	if (p2 == nullptr)
	{
		score += 1; // Reward vague match
		return true;
	}
	else
	{
		BaseType* exprType = expr->getType(block);
		BaseType* tpltType = getType(block);
		if (exprType == tpltType)
		{
			score += 5; // Reward exact match
			return true;
		}
		const Cast* cast = allowCast ? block->lookupCast(exprType, tpltType) : nullptr;
		if (cast != nullptr)
		{
			score += 3; // Reward inexact match (inheritance or type-cast)
			score -= cast->getCost(); // Penalize type-cast
			return true;
		}
		return false;
	}
}

void PlchldExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	if (p2 != nullptr && p1 != 'L')
	{
		BaseType* exprType = expr->getType(block);
		BaseType* tpltType = getType(block);
		if (exprType != tpltType)
		{
//printf("implicit cast from %s to %s in %s:%i\n", getTypeName(exprType).c_str(), getTypeName(tpltType).c_str(), expr->loc.filename, expr->loc.begin_line);
			const Cast* cast = block->lookupCast(exprType, tpltType);
			assert(cast != nullptr);
			ExprAST* castExpr = new CastExprAST(cast, expr);
			storeParam(castExpr, params, paramIdx++);
			return;
		}
	}
	storeParam(expr, params, paramIdx++);
}

std::vector<ExprAST*>::iterator begin(ExprListAST& exprs) { return exprs.begin(); }
std::vector<ExprAST*>::iterator begin(ExprListAST* exprs) { return exprs->begin(); }
std::vector<ExprAST*>::iterator end(ExprListAST& exprs) { return exprs.end(); }
std::vector<ExprAST*>::iterator end(ExprListAST* exprs) { return exprs->end(); }