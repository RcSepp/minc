#ifndef __MINC_API_H
#define __MINC_API_H

#include <map>
#include <string>
#include <vector>
#include <functional>
#include <iostream>

#include "minc_types.h"

extern "C"
{
	// >>> Parser

	MincBlockExpr* parseCFile(const char* filename);
	const std::vector<MincExpr*> parseCTplt(const char* tpltStr);

	MincBlockExpr* parsePythonFile(const char* filename);
	const std::vector<MincExpr*> parsePythonTplt(const char* tpltStr);

	// >>> Code Generator

	MincSymbol codegenExpr(MincExpr* expr, MincBlockExpr* scope);
	void codegenStmt(MincStmt* stmt, MincBlockExpr* scope);
	MincObject* getType(const MincExpr* expr, const MincBlockExpr* scope);
	const MincLocation& getLocation(const MincExpr* expr);
	void importBlock(MincBlockExpr* scope, MincBlockExpr* block);
	void collectParams(const MincBlockExpr* scope, const MincExpr* tplt, MincExpr* expr, std::vector<MincExpr*>& params);
	char* ExprToString(const MincExpr* expr);
	char* ExprToShortString(const MincExpr* expr);
	bool ExprIsId(const MincExpr* expr);
	bool ExprIsCast(const MincExpr* expr);
	bool ExprIsParam(const MincExpr* expr);
	bool ExprIsBlock(const MincExpr* expr);
	bool ExprIsStmt(const MincExpr* expr);
	bool ExprIsList(const MincExpr* expr);
	bool ExprIsPlchld(const MincExpr* expr);
	bool ExprIsEllipsis(const MincExpr* expr);
	void resolveExpr(MincBlockExpr* scope, MincExpr* expr);
	MincBlockExpr* wrapExpr(MincExpr* expr);
	MincBlockExpr* createEmptyBlockExpr();
	MincBlockExpr* cloneBlockExpr(MincBlockExpr* block);
	void resetBlockExpr(MincBlockExpr* block);
	size_t getBlockExprCacheState(MincBlockExpr* block);
	void resetBlockExprCache(MincBlockExpr* block, size_t targetState);
	bool isBlockExprBusy(MincBlockExpr* block);
	void removeBlockExpr(MincBlockExpr* expr);
	std::vector<MincExpr*>& getListExprExprs(MincListExpr* expr);
	const char* getIdExprName(const MincIdExpr* expr);
	const char* getLiteralExprValue(const MincLiteralExpr* expr);
	const std::vector<MincBlockExpr*>& getBlockExprReferences(const MincBlockExpr* expr);
	size_t countBlockExprStmts(const MincBlockExpr* expr);
	void iterateBlockExprStmts(const MincBlockExpr* expr, std::function<void(const MincListExpr* tplt, const MincKernel* stmt)> cbk);
	size_t countBlockExprExprs(const MincBlockExpr* expr);
	void iterateBlockExprExprs(const MincBlockExpr* expr, std::function<void(const MincExpr* tplt, const MincKernel* expr)> cbk);
	size_t countBlockExprCasts(const MincBlockExpr* expr);
	void iterateBlockExprCasts(const MincBlockExpr* expr, std::function<void(const MincCast* cast)> cbk);
	size_t countBlockExprSymbols(const MincBlockExpr* expr);
	void iterateBlockExprSymbols(const MincBlockExpr* expr, std::function<void(const std::string& name, const MincSymbol& symbol)> cbk);
	MincBlockExpr* getBlockExprParent(const MincBlockExpr* expr);
	void setBlockExprParent(MincBlockExpr* expr, MincBlockExpr* parent);
	void setBlockExprParams(MincBlockExpr* expr, std::vector<MincSymbol>& blockParams);
	const char* getBlockExprName(const MincBlockExpr* expr);
	void setBlockExprName(MincBlockExpr* expr, const char* name);
	const MincStmt* getCurrentBlockExprStmt(const MincBlockExpr* expr);
	MincExpr* getCastExprSource(const MincCastExpr* expr);
	char getPlchldExprLabel(const MincPlchldExpr* expr);
	const char* getPlchldExprSublabel(const MincPlchldExpr* expr);
	const char* getExprFilename(const MincExpr* expr);
	unsigned getExprLine(const MincExpr* expr);
	unsigned getExprColumn(const MincExpr* expr);
	unsigned getExprEndLine(const MincExpr* expr);
	unsigned getExprEndColumn(const MincExpr* expr);
	MincExpr* getDerivedExpr(MincExpr* expr);
	MincBlockExpr* getRootScope();
	MincBlockExpr* getFileScope();
	MincScopeType* getScopeType(const MincBlockExpr* scope);
	void setScopeType(MincBlockExpr* scope, MincScopeType* scopeType);
	void defineImportRule(MincScopeType* fromScope, MincScopeType* toScope, MincObject* symbolType, ImptBlock imptBlock);

	void defineSymbol(MincBlockExpr* scope, const char* name, MincObject* type, MincObject* value);
	void defineStmt1(MincBlockExpr* scope, const std::vector<MincExpr*>& tplt, StmtBlock codeBlock, void* stmtArgs = nullptr);
	void defineStmt2(MincBlockExpr* scope, const char* tpltStr, StmtBlock codeBlock, void* stmtArgs = nullptr);
	void defineStmt3(MincBlockExpr* scope, const std::vector<MincExpr*>& tplt, MincKernel* stmt);
	void defineStmt4(MincBlockExpr* scope, const char* tpltStr, MincKernel* stmt);
	void defineDefaultStmt2(MincBlockExpr* scope, StmtBlock codeBlock, void* stmtArgs = nullptr);
	void defineDefaultStmt3(MincBlockExpr* scope, MincKernel* stmt);
	void defineExpr2(MincBlockExpr* scope, const char* tpltStr, ExprBlock codeBlock, MincObject* type, void* exprArgs = nullptr);
	void defineExpr3(MincBlockExpr* scope, const char* tpltStr, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs = nullptr);
	void defineExpr5(MincBlockExpr* scope, MincExpr* tplt, MincKernel* expr);
	void defineExpr6(MincBlockExpr* scope, const char* tpltStr, MincKernel* expr);
	void defineDefaultExpr2(MincBlockExpr* scope, ExprBlock codeBlock, MincObject* type, void* exprArgs = nullptr);
	void defineDefaultExpr3(MincBlockExpr* scope, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs = nullptr);
	void defineDefaultExpr5(MincBlockExpr* scope, MincKernel* expr);
	void defineTypeCast2(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, ExprBlock codeBlock, void* castArgs = nullptr);
	void defineTypeCast3(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, MincKernel* cast);
	void defineOpaqueTypeCast(MincBlockExpr* scope, MincObject* fromType, MincObject* toType);
	void defineInheritanceCast2(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, ExprBlock codeBlock, void* castArgs = nullptr);
	void defineInheritanceCast3(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, MincKernel* cast);
	void defineOpaqueInheritanceCast(MincBlockExpr* scope, MincObject* fromType, MincObject* toType);

	const MincSymbol* lookupSymbol(const MincBlockExpr* scope, const char* name);
	const std::string* lookupSymbolName1(const MincBlockExpr* scope, const MincObject* value);
	const std::string& lookupSymbolName2(const MincBlockExpr* scope, const MincObject* value, const std::string& defaultName);
	MincSymbol* importSymbol(MincBlockExpr* scope, const char* name);
	MincExpr* lookupCast(const MincBlockExpr* scope, MincExpr* expr, MincObject* toType);
	bool isInstance(const MincBlockExpr* scope, MincObject* fromType, MincObject* toType);
	void lookupStmtCandidates(const MincBlockExpr* scope, const MincStmt* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates);
	void lookupExprCandidates(const MincBlockExpr* scope, const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates);
	std::string reportExprCandidates(const MincBlockExpr* scope, const MincExpr* expr);
	std::string reportCasts(const MincBlockExpr* scope);

	const MincSymbol& getVoid();

	void raiseCompileError(const char* msg, const MincExpr* loc=nullptr);

	void registerStepEventListener(StepEvent listener, void* eventArgs=nullptr);
	void deregisterStepEventListener(StepEvent listener);
}

#endif