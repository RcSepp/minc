#include "minc_api.h"
#include "minc_api.hpp"

const Variable VOID = Variable(new BaseType(), nullptr);
BlockExprAST* const rootBlock = new BlockExprAST({0}, {});
BlockExprAST* fileBlock = nullptr;
BlockExprAST* topLevelBlock = nullptr;
std::map<std::pair<BaseScopeType*, BaseScopeType*>, std::map<BaseType*, ImptBlock>> importRules;

extern "C"
{
	const Variable& getVoid()
	{
		return VOID;
	}

	BlockExprAST* getRootScope()
	{
		return rootBlock;
	}

	BlockExprAST* getFileScope()
	{
		return fileBlock;
	}

	void defineImportRule(BaseScopeType* fromScope, BaseScopeType* toScope, BaseType* symbolType, ImptBlock imptBlock)
	{
		const auto& key = std::pair<BaseScopeType*, BaseScopeType*>(fromScope, toScope);
		auto rules = importRules.find(key);
		if (rules == importRules.end())
			importRules[key] = { {symbolType, imptBlock } };
		else
			rules->second[symbolType] = imptBlock;
	}
}

void raiseStepEvent(const ExprAST* loc, StepEventType type);

BlockExprAST::BlockExprAST(const Location& loc, std::vector<ExprAST*>* exprs)
	: ExprAST(loc, ExprAST::ExprType::BLOCK), castreg(this), parent(nullptr), exprs(exprs), exprIdx(0), scopeType(nullptr), resultCacheIdx(0), isBlockSuspended(false), isStmtSuspended(false), isExprSuspended(false), isBusy(false)
{
}

void BlockExprAST::defineStmt(const std::vector<ExprAST*>& tplt, CodegenContext* stmt)
{
	for (ExprAST* tpltExpr: tplt)
		tpltExpr->resolveTypes(this);
	stmtreg.defineStmt(new ListExprAST('\0', tplt), stmt);
}

void BlockExprAST::lookupStmtCandidates(const ListExprAST* stmt, std::multimap<MatchScore, const std::pair<const ListExprAST*, CodegenContext*>>& candidates) const
{
	for (const BlockExprAST* block = this; block; block = block->parent)
	{
		block->stmtreg.lookupStmtCandidates(this, stmt, candidates);
		for (const BlockExprAST* ref: block->references)
			ref->stmtreg.lookupStmtCandidates(this, stmt, candidates);
	}
}

size_t BlockExprAST::countStmts() const
{
	return stmtreg.countStmts();
}

void BlockExprAST::iterateStmts(std::function<void(const ListExprAST* tplt, const CodegenContext* stmt)> cbk) const
{
	stmtreg.iterateStmts(cbk);
}

void BlockExprAST::defineAntiStmt(CodegenContext* stmt)
{
	stmtreg.defineAntiStmt(stmt);
}

void BlockExprAST::defineExpr(ExprAST* tplt, CodegenContext* expr)
{
	tplt->resolveTypes(this);
	stmtreg.defineExpr(tplt, expr);
}

void BlockExprAST::lookupExprCandidates(const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*, CodegenContext*>>& candidates) const
{
	for (const BlockExprAST* block = this; block; block = block->parent)
	{
		block->stmtreg.lookupExprCandidates(this, expr, candidates);
		for (const BlockExprAST* ref: block->references)
			ref->stmtreg.lookupExprCandidates(this, expr, candidates);
	}
}

size_t BlockExprAST::countExprs() const
{
	return stmtreg.countExprs();
}

void BlockExprAST::iterateExprs(std::function<void(const ExprAST* tplt, const CodegenContext* expr)> cbk) const
{
	stmtreg.iterateExprs(cbk);
}

void BlockExprAST::defineAntiExpr(CodegenContext* expr)
{
	stmtreg.defineAntiExpr(expr);
}

void BlockExprAST::defineCast(Cast* cast)
{
	// Skip if one of the following is true
	// 1. fromType == toType
	// 2. A cast exists from fromType to toType with a lower or equal cost
	// 3. Cast is an inheritance and another inheritance cast exists from toType to fromType (inheritance loop avoidance)
	const Cast* existingCast;
	if (cast->fromType == cast->toType
		|| ((existingCast = lookupCast(cast->fromType, cast->toType)) != nullptr && existingCast->getCost() <= cast->getCost())
		|| ((existingCast = lookupCast(cast->toType, cast->fromType)) != nullptr && existingCast->getCost() == 0 && cast->getCost() == 0))
		return;

	castreg.defineDirectCast(cast);

	for (const BlockExprAST* block = this; block; block = block->parent)
	{
		castreg.defineIndirectCast(block->castreg, cast);
		for (const BlockExprAST* ref: block->references)
			castreg.defineIndirectCast(ref->castreg, cast);
	}
}

const Cast* BlockExprAST::lookupCast(BaseType* fromType, BaseType* toType) const
{
	const Cast* cast;
	for (const BlockExprAST* block = this; block; block = block->parent)
	{
		if ((cast = block->castreg.lookupCast(fromType, toType)) != nullptr)
			return cast;
		for (const BlockExprAST* ref: block->references)
			if ((cast = ref->castreg.lookupCast(fromType, toType)) != nullptr)
				return cast;
	}
	return nullptr;
}

bool BlockExprAST::isInstance(BaseType* derivedType, BaseType* baseType) const
{
	for (const BlockExprAST* block = this; block; block = block->parent)
	{
		if (block->castreg.isInstance(derivedType, baseType))
			return true;
		for (const BlockExprAST* ref: block->references)
			if (ref->castreg.isInstance(derivedType, baseType))
				return true;
	}
	return false;
}

void BlockExprAST::listAllCasts(std::list<std::pair<BaseType*, BaseType*>>& casts) const
{
	for (const BlockExprAST* block = this; block; block = block->parent)
	{
		block->castreg.listAllCasts(casts);
		for (const BlockExprAST* ref: block->references)
			ref->castreg.listAllCasts(casts);
	}
}

size_t BlockExprAST::countCasts() const
{
	return castreg.countCasts();
}

void BlockExprAST::iterateCasts(std::function<void(const Cast* cast)> cbk) const
{
	castreg.iterateCasts(cbk);
}

void BlockExprAST::import(BlockExprAST* importBlock)
{
	const BlockExprAST* block;

	// Import all references of importBlock
	for (BlockExprAST* importRef: importBlock->references)
	{
		for (block = this; block; block = block->parent)
			if (importRef == block || std::find(block->references.begin(), block->references.end(), importRef) != block->references.end())
				break;
		if (block == nullptr)
			references.insert(references.begin(), importRef);
	}

	// Import importBlock
	for (block = this; block; block = block->parent)
		if (importBlock == block || std::find(block->references.begin(), block->references.end(), importBlock) != block->references.end())
			break;
	if (block == nullptr)
		references.insert(references.begin(), importBlock);
}

void BlockExprAST::defineSymbol(std::string name, BaseType* type, BaseValue* var)
{
	scope[name] = Variable(type, var);
}

const Variable* BlockExprAST::lookupSymbol(const std::string& name) const
{
	std::map<std::string, Variable>::const_iterator symbolIter;
	if ((symbolIter = scope.find(name)) != scope.cend())
		return &symbolIter->second; // Symbol found in local scope

	const Variable* symbol;
	for (const BlockExprAST* ref: references)
		if ((symbolIter = ref->scope.find(name)) != ref->scope.cend())
			return &symbolIter->second; // Symbol found in ref scope
	
	if (parent != nullptr && (symbol = parent->lookupSymbol(name)))
		return symbol; // Symbol found in parent scope

	return nullptr; // Symbol not found
}

size_t BlockExprAST::countSymbols() const
{
	return scope.size();
}

void BlockExprAST::iterateSymbols(std::function<void(const std::string& name, const Variable& symbol)> cbk) const
{
	for (const std::pair<std::string, Variable>& iter: scope)
		cbk(iter.first, iter.second);
}

Variable* BlockExprAST::importSymbol(const std::string& name)
{
	std::map<std::string, Variable>::iterator symbolIter;
	if ((symbolIter = scope.find(name)) != scope.end())
		return &symbolIter->second; // Symbol found in local scope

	Variable* symbol;
	for (BlockExprAST* ref: references)
		if ((symbolIter = ref->scope.find(name)) != ref->scope.end())
		{
			symbol = &symbolIter->second; // Symbol found in ref scope

			if (ref->scopeType == nullptr || scopeType == nullptr)
				return symbol; // Scope type undefined for either ref scope or local scope

			const auto& key = std::pair<BaseScopeType*, BaseScopeType*>(ref->scopeType, scopeType);
			const auto rules = importRules.find(key);
			if (rules == importRules.end())
				return symbol; // No import rules defined from ref scope to local scope

			// Search for import rule on symbol type
			const std::map<BaseType*, ImptBlock>::iterator rule = rules->second.find(symbol->type);
			if (rule != rules->second.end())
			{
				rule->second(*symbol, ref->scopeType, scopeType); // Execute import rule
				scope[name] = *symbol; // Import symbol into local scope
				return symbol; // Symbol and import rule found in ref scope
			}

			// Search for import rule on any base type of symbol type
			for (std::pair<BaseType* const, ImptBlock> rule: rules->second)
				if (isInstance(symbol->type, rule.first))
				{
					//TODO: Should we cast symbol to rule.first?
					rule.second(*symbol, ref->scopeType, scopeType); // Execute import rule
					scope[name] = *symbol; // Import symbol into local scope
					return symbol; // Symbol and import rule found in ref scope
				}

			return symbol; // No import rules on symbol type defined from ref scope to local scope
		}
	
	if (parent != nullptr && (symbol = parent->importSymbol(name)))
	{
		// Symbol found in parent scope

		if (parent->scopeType == nullptr || scopeType == nullptr)
			return symbol; // Scope type undefined for either parent scope or local scope

		const auto& key = std::pair<BaseScopeType*, BaseScopeType*>(parent->scopeType, scopeType);
		const auto rules = importRules.find(key);
		if (rules == importRules.end())
		{
			scope[name] = *symbol; // Import symbol into local scope
			return symbol; // No import rules defined from parent scope to local scope
		}

		// Search for import rule on symbol type
		const std::map<BaseType*, ImptBlock>::const_iterator rule = rules->second.find(symbol->type);
		if (rule != rules->second.end())
		{
			rule->second(*symbol, parent->scopeType, scopeType); // Execute import rule
			scope[name] = *symbol; // Import symbol into local scope
			return symbol; // Symbol and import rule found in parent scope
		}

		// Search for import rule on any base type of symbol type
		for (std::pair<BaseType* const, ImptBlock> rule: rules->second)
			if (isInstance(symbol->type, rule.first))
			{
				//TODO: Should we cast symbol to rule.first?
				rule.second(*symbol, parent->scopeType, scopeType); // Execute import rule
				scope[name] = *symbol; // Import symbol into local scope
				return symbol; // Symbol and import rule found in parent scope
			}

		scope[name] = *symbol; // Import symbol into local scope
		return symbol; // No import rules on symbol type defined from parent scope to local scope
	}

	return nullptr; // Symbol not found
}

const std::vector<Variable>* BlockExprAST::getBlockParams() const
{
	for (const BlockExprAST* block = this; block; block = block->parent)
	{
		if (block->blockParams.size())
			return &block->blockParams;
		for (const BlockExprAST* ref: block->references)
			if (ref->blockParams.size())
				return &ref->blockParams;
	}
	return nullptr;
}

Variable BlockExprAST::codegen(BlockExprAST* parentBlock)
{
	if (parentBlock == this)
		throw CompileError("block expression can't be it's own parent", this->loc);
	if (isBusy)
		throw CompileError("block expression already executing. Use BlockExprAST::clone() when executing blocks recursively", this->loc);
	isBusy = true;

	try
	{
		raiseStepEvent(this, isBlockSuspended ? STEP_RESUME : STEP_IN);
	}
	catch (...)
	{
		isBlockSuspended = true;
		raiseStepEvent(this, STEP_SUSPEND);
		isBusy = false;
		throw;
	}
	isBlockSuspended = false;

	parent = parentBlock;

	BlockExprAST* oldTopLevelBlock = topLevelBlock;
	if (parentBlock == nullptr)
	{
		parent = rootBlock;
		topLevelBlock = this;
	}

	BlockExprAST* oldFileBlock = fileBlock;
	if (fileBlock == nullptr)
		fileBlock = this;

	if (exprIdx >= exprs->size())
		exprIdx = 0;

	try
	{
		for (ExprASTIter stmtBeginExpr = exprs->cbegin() + exprIdx; stmtBeginExpr != exprs->cend(); exprIdx = stmtBeginExpr - exprs->cbegin())
		{
			if (!isStmtSuspended && !lookupStmt(stmtBeginExpr, currentStmt))
				throw UndefinedStmtException(&currentStmt);
			currentStmt.codegen(this);

			// Advance beginning of next statement to end of current statement
			stmtBeginExpr = currentStmt.end;

			// Clear cached expressions
			// Coroutines exit codegen() without clearing resultCache by throwing an exception
			// They use the resultCache on reentry to avoid reexecuting expressions
			for (Variable* cachedResult: resultCache)
				if (cachedResult)
					delete cachedResult;
			resultCache.clear();
			resultCacheIdx = 0;
		}
	}
	catch (...)
	{
		resultCacheIdx = 0;

		if (topLevelBlock == this)
			topLevelBlock = oldTopLevelBlock;

		if (fileBlock == this)
			fileBlock = oldFileBlock;

		isBlockSuspended = true;
		raiseStepEvent(this, STEP_SUSPEND);

		isBusy = false;
		throw;
	}

	exprIdx = 0;

	if (topLevelBlock == this)
		topLevelBlock = oldTopLevelBlock;

	if (fileBlock == this)
		fileBlock = oldFileBlock;

	raiseStepEvent(this, STEP_OUT);

	isBusy = false;
	return VOID;
}

bool BlockExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype;
}

void BlockExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
}

std::string BlockExprAST::str() const
{
	if (exprs->empty())
		return "{}";

	std::string result = "{\n";
	for (auto expr: *exprs)
	{
		if (expr->exprtype == ExprAST::ExprType::STOP)
		{
			if (expr->exprtype == ExprAST::ExprType::STOP)
				result.pop_back(); // Remove ' ' before STOP string
			result += expr->str() + '\n';
		}
		else
			result += expr->str() + ' ';
	}

	size_t start_pos = 0;
	while((start_pos = result.find("\n", start_pos)) != std::string::npos && start_pos + 1 != result.size()) {
		result.replace(start_pos, 1, "\n\t");
		start_pos += 2;
	}

	return result + '}';
}

std::string BlockExprAST::shortStr() const
{
	return "{ ... }";
}

int BlockExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const BlockExprAST* _other = (const BlockExprAST*)other;
	c = (int)this->exprs->size() - (int)_other->exprs->size();
	if (c) return c;
	for (std::vector<ExprAST*>::const_iterator t = this->exprs->cbegin(), o = _other->exprs->cbegin(); t != this->exprs->cend(); ++t, ++o)
	{
		c = (*t)->comp(*o);
		if (c) return c;
	}
	return 0;
}

ExprAST* BlockExprAST::clone() const
{
	BlockExprAST* clone = new BlockExprAST(this->loc, this->exprs);
	clone->parent = this->parent;
	clone->references = this->references;
	clone->name = this->name;
	clone->scopeType = this->scopeType;
	clone->blockParams = this->blockParams;
	return clone;
}

void BlockExprAST::reset()
{
	for (Variable* cachedResult: resultCache)
		if (cachedResult)
			delete cachedResult;
	resultCache.clear();
	resultCacheIdx = 0;
	exprIdx = 0;
	isBlockSuspended = false;
	isStmtSuspended = false;
	isExprSuspended = false;
}

void BlockExprAST::clearCache(size_t targetSize)
{
	if (targetSize > resultCache.size())
		targetSize = resultCache.size();

	resultCacheIdx = targetSize;
	for (std::vector<Variable*>::iterator cachedResult = resultCache.begin() + targetSize; cachedResult != resultCache.end(); ++cachedResult)
		if (*cachedResult)
			delete *cachedResult;
	resultCache.erase(resultCache.begin() + targetSize, resultCache.end());
}

const StmtAST* BlockExprAST::getCurrentStmt() const
{
	return &currentStmt;
}

BlockExprAST* BlockExprAST::parseCFile(const char* filename)
{
	return ::parseCFile(filename);
}

const std::vector<ExprAST*> BlockExprAST::parseCTplt(const char* tpltStr)
{
	return ::parseCTplt(tpltStr);
}

BlockExprAST* BlockExprAST::parsePythonFile(const char* filename)
{
	return ::parsePythonFile(filename);
}

const std::vector<ExprAST*> BlockExprAST::parsePythonTplt(const char* tpltStr)
{
	return ::parsePythonTplt(tpltStr);
}