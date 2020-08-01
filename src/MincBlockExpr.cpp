#include "minc_api.h"
#include "minc_api.hpp"

#define DETECT_UNDEFINED_TYPE_CASTS

const MincSymbol VOID = MincSymbol(new MincObject(), nullptr);
MincBlockExpr* const rootBlock = new MincBlockExpr({0}, {});
MincBlockExpr* fileBlock = nullptr;
MincBlockExpr* topLevelBlock = nullptr;
std::map<std::pair<MincScopeType*, MincScopeType*>, std::map<MincObject*, ImptBlock>> importRules;

extern "C"
{
	const MincSymbol& getVoid()
	{
		return VOID;
	}

	MincBlockExpr* getRootScope()
	{
		return rootBlock;
	}

	MincBlockExpr* getFileScope()
	{
		return fileBlock;
	}

	void defineImportRule(MincScopeType* fromScope, MincScopeType* toScope, MincObject* symbolType, ImptBlock imptBlock)
	{
		const auto& key = std::pair<MincScopeType*, MincScopeType*>(fromScope, toScope);
		auto rules = importRules.find(key);
		if (rules == importRules.end())
			importRules[key] = { {symbolType, imptBlock } };
		else
			rules->second[symbolType] = imptBlock;
	}
}

void raiseStepEvent(const MincExpr* loc, StepEventType type);

MincBlockExpr::MincBlockExpr(const MincLocation& loc, std::vector<MincExpr*>* exprs, std::vector<MincStmt>* resolvedStmts)
	: MincExpr(loc, MincExpr::ExprType::BLOCK), castreg(this), resolvedStmts(resolvedStmts), ownesResolvedStmts(false), parent(nullptr), exprs(exprs),
	  stmtIdx(0), scopeType(nullptr), resultCacheIdx(0), isBlockSuspended(false), isStmtSuspended(false), isExprSuspended(false), isBusy(false), user(nullptr), userType(nullptr)
{
}

MincBlockExpr::MincBlockExpr(const MincLocation& loc, std::vector<MincExpr*>* exprs)
	: MincExpr(loc, MincExpr::ExprType::BLOCK), castreg(this), resolvedStmts(new std::vector<MincStmt>()), ownesResolvedStmts(true), parent(nullptr), exprs(exprs),
	  stmtIdx(0), scopeType(nullptr), resultCacheIdx(0), isBlockSuspended(false), isStmtSuspended(false), isExprSuspended(false), isBusy(false), user(nullptr), userType(nullptr)
{
}

MincBlockExpr::~MincBlockExpr()
{
	if (ownesResolvedStmts)
		delete resolvedStmts;
}

void MincBlockExpr::defineStmt(const std::vector<MincExpr*>& tplt, MincKernel* stmt)
{
	for (MincExpr* tpltExpr: tplt)
		tpltExpr->resolveTypes(this);
	stmtreg.defineStmt(new MincListExpr('\0', tplt), stmt);
}

void MincBlockExpr::defineStmt(const std::vector<MincExpr*>& tplt, std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> codegen)
{
	struct StmtKernel : public MincKernel
	{
		const std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> codegenCtx;
		StmtKernel(std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> codegen)
			: codegenCtx(codegen) {}
		virtual ~StmtKernel() {}
		MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) { codegenCtx(parentBlock, params); return VOID; }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return VOID.type; }
	};
	defineStmt(tplt, new StmtKernel(codegen));
}

void MincBlockExpr::lookupStmtCandidates(const MincListExpr* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates) const
{
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		block->stmtreg.lookupStmtCandidates(this, stmt, candidates);
		for (const MincBlockExpr* ref: block->references)
			ref->stmtreg.lookupStmtCandidates(this, stmt, candidates);
	}
}

size_t MincBlockExpr::countStmts() const
{
	return stmtreg.countStmts();
}

void MincBlockExpr::iterateStmts(std::function<void(const MincListExpr* tplt, const MincKernel* stmt)> cbk) const
{
	stmtreg.iterateStmts(cbk);
}

void MincBlockExpr::defineDefaultStmt(MincKernel* stmt)
{
	stmtreg.defineDefaultStmt(stmt);
}

void MincBlockExpr::defineExpr(MincExpr* tplt, MincKernel* expr)
{
	tplt->resolveTypes(this);
	stmtreg.defineExpr(tplt, expr);
}

void MincBlockExpr::defineExpr(MincExpr* tplt, std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> codegen, MincObject* type)
{
	struct ExprKernel : public MincKernel
	{
		const std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> codegenCbk;
		MincObject* const type;
		ExprKernel(std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> codegen, MincObject* type)
			: codegenCbk(codegen), type(type) {}
		virtual ~ExprKernel() {}
		MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) { return codegenCbk(parentBlock, params); }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return type; }
	};
	defineExpr(tplt, new ExprKernel(codegen, type));
}

void MincBlockExpr::defineExpr(MincExpr* tplt, std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> codegen, std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getType)
{
	struct ExprKernel : public MincKernel
	{
		const std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> codegenCbk;
		const std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getTypeCbk;
		ExprKernel(std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> codegen, std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getType)
			: codegenCbk(codegen), getTypeCbk(getType) {}
		virtual ~ExprKernel() {}
		MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) { return codegenCbk(parentBlock, params); }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getTypeCbk(parentBlock, params); }
	};
	defineExpr(tplt, new ExprKernel(codegen, getType));
}

void MincBlockExpr::lookupExprCandidates(const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates) const
{
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		block->stmtreg.lookupExprCandidates(this, expr, candidates);
		for (const MincBlockExpr* ref: block->references)
			ref->stmtreg.lookupExprCandidates(this, expr, candidates);
	}
}

size_t MincBlockExpr::countExprs() const
{
	return stmtreg.countExprs();
}

void MincBlockExpr::iterateExprs(std::function<void(const MincExpr* tplt, const MincKernel* expr)> cbk) const
{
	stmtreg.iterateExprs(cbk);
}

void MincBlockExpr::defineDefaultExpr(MincKernel* expr)
{
	stmtreg.defineDefaultExpr(expr);
}

void MincBlockExpr::defineCast(MincCast* cast)
{
	// Skip if one of the following is true
	// 1. fromType == toType
	// 2. A cast exists from fromType to toType with a lower or equal cost
	// 3. Cast is an inheritance and another inheritance cast exists from toType to fromType (inheritance loop avoidance)
	const MincCast* existingCast;
	if (cast->fromType == cast->toType
		|| ((existingCast = lookupCast(cast->fromType, cast->toType)) != nullptr && existingCast->getCost() <= cast->getCost())
		|| ((existingCast = lookupCast(cast->toType, cast->fromType)) != nullptr && existingCast->getCost() == 0 && cast->getCost() == 0))
		return;

#ifdef DETECT_UNDEFINED_TYPE_CASTS
	if (lookupSymbolName1(this, cast->fromType) == nullptr || lookupSymbolName1(this, cast->toType) == nullptr)
		throw CompileError("type-cast defined from " + lookupSymbolName2(this, cast->fromType, "UNKNOWN_TYPE") + " to " + lookupSymbolName2(this, cast->toType, "UNKNOWN_TYPE"));
#endif

	castreg.defineDirectCast(cast);

	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		castreg.defineIndirectCast(block->castreg, cast);
		for (const MincBlockExpr* ref: block->references)
			castreg.defineIndirectCast(ref->castreg, cast);
	}
}

const MincCast* MincBlockExpr::lookupCast(MincObject* fromType, MincObject* toType) const
{
	const MincCast* cast;
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		if ((cast = block->castreg.lookupCast(fromType, toType)) != nullptr)
			return cast;
		for (const MincBlockExpr* ref: block->references)
			if ((cast = ref->castreg.lookupCast(fromType, toType)) != nullptr)
				return cast;
	}
	return nullptr;
}

bool MincBlockExpr::isInstance(MincObject* derivedType, MincObject* baseType) const
{
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		if (block->castreg.isInstance(derivedType, baseType))
			return true;
		for (const MincBlockExpr* ref: block->references)
			if (ref->castreg.isInstance(derivedType, baseType))
				return true;
	}
	return false;
}

void MincBlockExpr::listAllCasts(std::list<std::pair<MincObject*, MincObject*>>& casts) const
{
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		block->castreg.listAllCasts(casts);
		for (const MincBlockExpr* ref: block->references)
			ref->castreg.listAllCasts(casts);
	}
}

size_t MincBlockExpr::countCasts() const
{
	return castreg.countCasts();
}

void MincBlockExpr::iterateCasts(std::function<void(const MincCast* cast)> cbk) const
{
	castreg.iterateCasts(cbk);
}

void MincBlockExpr::import(MincBlockExpr* importBlock)
{
	const MincBlockExpr* block;

	// Import all references of importBlock
	for (MincBlockExpr* importRef: importBlock->references)
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

void MincBlockExpr::defineSymbol(std::string name, MincObject* type, MincObject* value)
{
	symbolMap[name] = MincSymbol(type, value); // Insert or replace forward mapping
	symbolNameMap[value] = name; // Insert or replace backward mapping
}

const MincSymbol* MincBlockExpr::lookupSymbol(const std::string& name) const
{
	std::map<std::string, MincSymbol>::const_iterator symbolIter;
	if ((symbolIter = symbolMap.find(name)) != symbolMap.cend())
		return &symbolIter->second; // Symbol found in local scope

	for (const MincBlockExpr* ref: references)
		if ((symbolIter = ref->symbolMap.find(name)) != ref->symbolMap.cend())
			return &symbolIter->second; // Symbol found in ref scope

	const MincSymbol* symbol;
	if (parent != nullptr && (symbol = parent->lookupSymbol(name)))
		return symbol; // Symbol found in parent scope

	return nullptr; // Symbol not found
}

const std::string* MincBlockExpr::lookupSymbolName(const MincObject* value) const
{
	std::map<const MincObject*, std::string>::const_iterator symbolIter;
	if ((symbolIter = symbolNameMap.find(value)) != symbolNameMap.cend())
		return &symbolIter->second; // Symbol found in local scope

	for (const MincBlockExpr* ref: references)
		if ((symbolIter = ref->symbolNameMap.find(value)) != ref->symbolNameMap.cend())
			return &symbolIter->second; // Symbol found in ref scope

	const std::string* name;
	if (parent != nullptr && (name = parent->lookupSymbolName(value)))
		return name; // Symbol found in parent scope

	return nullptr; // Symbol not found
}

const std::string& MincBlockExpr::lookupSymbolName(const MincObject* value, const std::string& defaultName) const
{
	std::map<const MincObject*, std::string>::const_iterator symbolIter;
	if ((symbolIter = symbolNameMap.find(value)) != symbolNameMap.cend())
		return symbolIter->second; // Symbol found in local scope

	for (const MincBlockExpr* ref: references)
		if ((symbolIter = ref->symbolNameMap.find(value)) != ref->symbolNameMap.cend())
			return symbolIter->second; // Symbol found in ref scope

	const std::string* name;
	if (parent != nullptr && (name = parent->lookupSymbolName(value)))
		return name != nullptr ? *name : defaultName; // Symbol found in parent scope

	return defaultName; // Symbol not found
}

size_t MincBlockExpr::countSymbols() const
{
	return symbolMap.size();
}

void MincBlockExpr::iterateSymbols(std::function<void(const std::string& name, const MincSymbol& symbol)> cbk) const
{
	for (const std::pair<std::string, MincSymbol>& iter: symbolMap)
		cbk(iter.first, iter.second);
}

MincSymbol* MincBlockExpr::importSymbol(const std::string& name)
{
	std::map<std::string, MincSymbol>::iterator symbolIter;
	if ((symbolIter = symbolMap.find(name)) != symbolMap.end())
		return &symbolIter->second; // Symbol found in local scope

	MincSymbol* symbol;
	for (MincBlockExpr* ref: references)
		if ((symbolIter = ref->symbolMap.find(name)) != ref->symbolMap.end())
		{
			symbol = &symbolIter->second; // Symbol found in ref scope

			if (ref->scopeType == nullptr || scopeType == nullptr)
				return symbol; // Scope type undefined for either ref scope or local scope

			const auto& key = std::pair<MincScopeType*, MincScopeType*>(ref->scopeType, scopeType);
			const auto rules = importRules.find(key);
			if (rules == importRules.end())
				return symbol; // No import rules defined from ref scope to local scope

			// Search for import rule on symbol type
			const std::map<MincObject*, ImptBlock>::iterator rule = rules->second.find(symbol->type);
			if (rule != rules->second.end())
			{
				rule->second(*symbol, ref->scopeType, scopeType); // Execute import rule
				symbolMap[name] = *symbol; // Import symbol into local scope
				return symbol; // Symbol and import rule found in ref scope
			}

			// Search for import rule on any base type of symbol type
			for (std::pair<MincObject* const, ImptBlock> rule: rules->second)
				if (isInstance(symbol->type, rule.first))
				{
					//TODO: Should we cast symbol to rule.first?
					rule.second(*symbol, ref->scopeType, scopeType); // Execute import rule
					symbolMap[name] = *symbol; // Import symbol into local scope
					return symbol; // Symbol and import rule found in ref scope
				}

			return symbol; // No import rules on symbol type defined from ref scope to local scope
		}
	
	if (parent != nullptr && (symbol = parent->importSymbol(name)))
	{
		// Symbol found in parent scope

		if (parent->scopeType == nullptr || scopeType == nullptr)
			return symbol; // Scope type undefined for either parent scope or local scope

		const auto& key = std::pair<MincScopeType*, MincScopeType*>(parent->scopeType, scopeType);
		const auto rules = importRules.find(key);
		if (rules == importRules.end())
		{
			symbolMap[name] = *symbol; // Import symbol into local scope
			return symbol; // No import rules defined from parent scope to local scope
		}

		// Search for import rule on symbol type
		const std::map<MincObject*, ImptBlock>::const_iterator rule = rules->second.find(symbol->type);
		if (rule != rules->second.end())
		{
			rule->second(*symbol, parent->scopeType, scopeType); // Execute import rule
			symbolMap[name] = *symbol; // Import symbol into local scope
			return symbol; // Symbol and import rule found in parent scope
		}

		// Search for import rule on any base type of symbol type
		for (std::pair<MincObject* const, ImptBlock> rule: rules->second)
			if (isInstance(symbol->type, rule.first))
			{
				//TODO: Should we cast symbol to rule.first?
				rule.second(*symbol, parent->scopeType, scopeType); // Execute import rule
				symbolMap[name] = *symbol; // Import symbol into local scope
				return symbol; // Symbol and import rule found in parent scope
			}

		symbolMap[name] = *symbol; // Import symbol into local scope
		return symbol; // No import rules on symbol type defined from parent scope to local scope
	}

	return nullptr; // Symbol not found
}

const std::vector<MincSymbol>* MincBlockExpr::getBlockParams() const
{
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		if (block->blockParams.size())
			return &block->blockParams;
		for (const MincBlockExpr* ref: block->references)
			if (ref->blockParams.size())
				return &ref->blockParams;
	}
	return nullptr;
}

MincSymbol MincBlockExpr::codegen(MincBlockExpr* parentBlock)
{
	if (parentBlock == this)
		throw CompileError("block expression can't be it's own parent", this->loc);
	if (isBusy)
		throw CompileError("block expression already executing. Use MincBlockExpr::clone() when executing blocks recursively", this->loc);
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

	MincBlockExpr* oldTopLevelBlock = topLevelBlock;
	if (parentBlock == nullptr)
	{
		parent = rootBlock;
		topLevelBlock = this;
	}

	MincBlockExpr* oldFileBlock = fileBlock;
	if (fileBlock == nullptr)
		fileBlock = this;

	try
	{
		for (; stmtIdx < resolvedStmts->size(); ++stmtIdx)
		{
			MincStmt& currentStmt = resolvedStmts->at(stmtIdx);
			if (!currentStmt.isResolved() && !lookupStmt(currentStmt.begin, currentStmt))
				throw UndefinedStmtException(&currentStmt);
			currentStmt.codegen(this);

			// Clear cached expressions
			// Coroutines exit codegen() without clearing resultCache by throwing an exception
			// They use the resultCache on reentry to avoid reexecuting expressions
			for (MincSymbol* cachedResult: resultCache)
				if (cachedResult)
					delete cachedResult;
			resultCache.clear();
			resultCacheIdx = 0;
		}
		stmtIdx = resolvedStmts->size(); // Handle case `stmtIdx > resolvedStmts->size()`
		for (MincExprIter stmtBeginExpr = resolvedStmts->size() ? resolvedStmts->back().end : exprs->cbegin(); stmtBeginExpr != exprs->cend(); ++stmtIdx)
		{
			resolvedStmts->push_back(MincStmt());
			MincStmt& currentStmt = resolvedStmts->back();
			if (!lookupStmt(stmtBeginExpr, currentStmt))
				throw UndefinedStmtException(&currentStmt);
			currentStmt.codegen(this);

			// Advance beginning of next statement to end of current statement
			stmtBeginExpr = currentStmt.end;

			// Clear cached expressions
			// Coroutines exit codegen() without clearing resultCache by throwing an exception
			// They use the resultCache on reentry to avoid reexecuting expressions
			for (MincSymbol* cachedResult: resultCache)
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

	stmtIdx = 0;

	if (topLevelBlock == this)
		topLevelBlock = oldTopLevelBlock;

	if (fileBlock == this)
		fileBlock = oldFileBlock;

	raiseStepEvent(this, STEP_OUT);

	isBusy = false;
	return VOID;
}

bool MincBlockExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype;
}

void MincBlockExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
}

std::string MincBlockExpr::str() const
{
	if (exprs->empty())
		return "{}";

	std::string result = "{\n";
	for (auto expr: *exprs)
	{
		if (expr->exprtype == MincExpr::ExprType::STOP)
		{
			if (expr->exprtype == MincExpr::ExprType::STOP)
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

std::string MincBlockExpr::shortStr() const
{
	return "{ ... }";
}

int MincBlockExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincBlockExpr* _other = (const MincBlockExpr*)other;
	c = (int)this->exprs->size() - (int)_other->exprs->size();
	if (c) return c;
	for (std::vector<MincExpr*>::const_iterator t = this->exprs->cbegin(), o = _other->exprs->cbegin(); t != this->exprs->cend(); ++t, ++o)
	{
		c = (*t)->comp(*o);
		if (c) return c;
	}
	return 0;
}

MincExpr* MincBlockExpr::clone() const
{
	MincBlockExpr* clone = new MincBlockExpr(this->loc, this->exprs);
	clone->parent = this->parent;
	clone->references = this->references;
	clone->name = this->name;
	clone->scopeType = this->scopeType;
	clone->blockParams = this->blockParams;
	return clone;
}

void MincBlockExpr::reset()
{
	for (MincSymbol* cachedResult: resultCache)
		if (cachedResult)
			delete cachedResult;
	resultCache.clear();
	resultCacheIdx = 0;
	stmtIdx = 0;
	isBlockSuspended = false;
	isStmtSuspended = false;
	isExprSuspended = false;
}

void MincBlockExpr::clearCache(size_t targetSize)
{
	if (targetSize > resultCache.size())
		targetSize = resultCache.size();

	resultCacheIdx = targetSize;
	for (std::vector<MincSymbol*>::iterator cachedResult = resultCache.begin() + targetSize; cachedResult != resultCache.end(); ++cachedResult)
		if (*cachedResult)
			delete *cachedResult;
	resultCache.erase(resultCache.begin() + targetSize, resultCache.end());
}

const MincStmt* MincBlockExpr::getCurrentStmt() const
{
	return resolvedStmts->size() ? &resolvedStmts->back() : nullptr;
}

MincBlockExpr* MincBlockExpr::parseCFile(const char* filename)
{
	return ::parseCFile(filename);
}

const std::vector<MincExpr*> MincBlockExpr::parseCTplt(const char* tpltStr)
{
	return ::parseCTplt(tpltStr);
}

MincBlockExpr* MincBlockExpr::parsePythonFile(const char* filename)
{
	return ::parsePythonFile(filename);
}

const std::vector<MincExpr*> MincBlockExpr::parsePythonTplt(const char* tpltStr)
{
	return ::parsePythonTplt(tpltStr);
}