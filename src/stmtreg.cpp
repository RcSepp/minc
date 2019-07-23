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

			if (!block->lookupStatement(expr))
				return false;

			if (tplt[0]->exprtype == ExprAST::ExprType::STOP) ++tplt; // Eat STOP as part of $S
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

	if (stmtEnd)
		*stmtEnd = expr;
	return tplt == tpltEnd; // We have a match if tplt has been fully traversed
}

void collectStatement(const BlockExprAST* block, ExprASTIter tplt, const ExprASTIter tpltEnd, ExprASTIter expr, const ExprASTIter exprEnd, std::vector<ExprAST*>& params)
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
		else if (tplt[0]->exprtype == ExprAST::ExprType::PLCHLD && ((PlchldExprAST*)tplt[0])->p1 == 'S')
		{
			++tplt; // Eat $S

			const ExprASTIter beginExpr = expr;
			const std::pair<const std::vector<ExprAST*>, CodegenContext*>* stmtContext = block->lookupStatement(expr);
			const ExprASTIter endExpr = expr;
			assert(stmtContext);
			StmtAST* stmt = new StmtAST(beginExpr, endExpr, stmtContext->second);
			stmt->collectParams(block, stmtContext->first);
			params.push_back(stmt);

			if (tplt[0]->exprtype == ExprAST::ExprType::STOP) ++tplt; // Eat STOP as part of $S
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

	assert(tplt == tpltEnd); // We have a match if tplt has been fully traversed
}

bool ExprListAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && matchStatement(block, this->exprs.cbegin(), this->exprs.cend(), ((ExprListAST*)expr)->exprs.cbegin(), ((ExprListAST*)expr)->exprs.cend(), score);
}

void ExprListAST::collectParams(const BlockExprAST* block, ExprAST* exprs, std::vector<ExprAST*>& params) const
{
	collectStatement(block, this->exprs.cbegin(), this->exprs.cend(), ((ExprListAST*)exprs)->exprs.cbegin(), ((ExprListAST*)exprs)->exprs.cend(), params);
}

void StmtAST::collectParams(const BlockExprAST* block, const std::vector<ExprAST*> tplt)
{
	collectStatement(block, tplt.cbegin(), tplt.cend(), begin, end, resolvedParams);
}

const std::pair<const std::vector<ExprAST*>, CodegenContext*>* StatementRegister::lookupStatement(const BlockExprAST* block, const ExprASTIter stmt, MatchScore& bestScore, ExprASTIter& bestStmtEnd) const
{
	MatchScore currentScore;
	ExprASTIter currentStmtEnd;
	const std::pair<const std::vector<ExprAST*>, CodegenContext*>* bestStmt = nullptr;
	for (const std::pair<const std::vector<ExprAST*>, CodegenContext*>& iter: stmtreg)
	{
#ifdef DEBUG_STMTREG
auto foo = ExprListAST('\0', iter.first).str();
		printf("%scandidate `%s`", indent.c_str(), ExprListAST('\0', iter.first).str().c_str());
#endif
		currentScore = 0;
		if (matchStatement(block, iter.first.cbegin(), iter.first.cend(), stmt, block->exprs->cend(), currentScore, &currentStmtEnd) && currentScore > bestScore)
		{
			bestScore = currentScore;
			bestStmt = &iter;
			bestStmtEnd = currentStmtEnd;
#ifdef DEBUG_STMTREG
			printf(" MATCH(score=%i)", currentScore);
#endif
		}
#ifdef DEBUG_STMTREG
		printf("\n");
#endif
	}
	return bestStmt;
}

const std::pair<const ExprAST*, CodegenContext*>* StatementRegister::lookupExpr(const BlockExprAST* block, const ExprAST* expr) const
{
	MatchScore currentScore, bestScore = -2147483648;
	const std::pair<const ExprAST*, CodegenContext*>* bestStmt = nullptr;
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
	return bestStmt;
}

void StatementRegister::lookupExprCandidates(const BlockExprAST* block, const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*, CodegenContext*>&>& candidates) const
{
	MatchScore score;
	const std::pair<const ExprAST*, CodegenContext*>* bestStmt = nullptr;
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

bool BlockExprAST::lookupExpr(ExprAST* expr) const
{
#ifdef DEBUG_STMTREG
	printf("%slookupExpr(%s)\n", indent.c_str(), expr->str().c_str());
	indent += '\t';
#endif
	expr->resolvedParams.clear();
	const std::pair<const ExprAST*, CodegenContext*>* context = nullptr;
	for (const BlockExprAST* block = this; block && !context; block = block->parent)
		context = block->stmtreg.lookupExpr(this, expr);
#ifdef DEBUG_STMTREG
	indent = indent.substr(0, indent.size() - 1);
#endif

	if (context)
	{
		context->first->collectParams(this, expr, expr->resolvedParams);
		expr->resolvedContext = context->second;
		return true;
	}
	else
		return false;
}

const std::pair<const std::vector<ExprAST*>, CodegenContext*>* BlockExprAST::lookupStatement(ExprASTIter& exprs) const
{
//TODO: Figure out logic for looking up expressions ahead of looking up statements
for (ExprASTIter exprIter = exprs; exprIter != this->exprs->cend() && (*exprIter)->exprtype != ExprAST::ExprType::STOP && (*exprIter)->exprtype != ExprAST::ExprType::BLOCK; ++exprIter)
	(*exprIter)->resolveTypes(const_cast<BlockExprAST*>(this));

#ifdef DEBUG_STMTREG
	std::vector<ExprAST*> _exprs;
	for (ExprASTIter exprIter = exprs; exprIter != this->exprs->cend() && (*exprIter)->exprtype != ExprAST::ExprType::STOP && (*exprIter)->exprtype != ExprAST::ExprType::BLOCK; ++exprIter)
		_exprs.push_back(*exprIter);
	printf("%slookupStmt(%s)\n", indent.c_str(), ExprListAST('\0', _exprs).str().c_str());
	indent += '\t';
#endif

	// Lookup statement in current block and all parents
	// Get context of best match
	MatchScore currentScore, score = -2147483648;
	ExprASTIter currentStmtEnd, stmtEnd;
	const std::pair<const std::vector<ExprAST*>, CodegenContext*> *currentContext, *context = nullptr;
	for (const BlockExprAST* block = this; block; block = block->parent)
	{
		currentScore = score;
		currentContext = block->stmtreg.lookupStatement(this, exprs, currentScore, currentStmtEnd);
		if (currentScore > score)
		{
			context = currentContext;
			score = currentScore;
			stmtEnd = currentStmtEnd;
		}
	}
#ifdef DEBUG_STMTREG
	indent = indent.substr(0, indent.size() - 1);
#endif

	// Advance exprs parameter to beginning of next statement
	if (context)
		exprs = stmtEnd;
	else
		while (exprs != this->exprs->cend() && (*exprs)->exprtype != ExprAST::ExprType::STOP && (*exprs++)->exprtype != ExprAST::ExprType::BLOCK) {}

	return context;
}

bool PlchldExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	if (p1 == 'E')
	{
		score -= 1; // Penalize vague template
		return true;
	}

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
			CodegenContext* castContext = block->lookupCast(exprType, tpltType);
			assert(castContext != nullptr);
			ExprAST* castExpr = new CastExprAST(expr->loc);
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