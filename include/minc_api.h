#ifndef __MINC_API_H
#define __MINC_API_H

#include <map>
#include <string>
#include <vector>
#include <functional>

#include "minc_types.h"

extern "C"
{
	// >>> C Parser

	MincBlockExpr* parseCFile(const char* filename);
	const std::vector<MincExpr*> parseCTplt(const char* tpltStr);

	// >>> Python Parser

	MincBlockExpr* parsePythonFile(const char* filename);
	const std::vector<MincExpr*> parsePythonTplt(const char* tpltStr);

	// >>> MincException

	void raiseCompileError(const char* msg, const MincExpr* loc=nullptr);

	// >>> MincExpr

	MincSymbol codegenExpr(MincExpr* expr, MincBlockExpr* scope);
	MincObject* getType(const MincExpr* expr, const MincBlockExpr* scope);
	void collectParams(const MincBlockExpr* scope, const MincExpr* tplt, MincExpr* expr, std::vector<MincExpr*>& params);
	void resolveExpr(MincExpr* expr, MincBlockExpr* scope);
	void forgetExpr(MincExpr* expr);
	char* ExprToString(const MincExpr* expr);
	char* ExprToShortString(const MincExpr* expr);
	const MincLocation& getLocation(const MincExpr* expr);
	const char* getExprFilename(const MincExpr* expr);
	unsigned getExprLine(const MincExpr* expr);
	unsigned getExprColumn(const MincExpr* expr);
	unsigned getExprEndLine(const MincExpr* expr);
	unsigned getExprEndColumn(const MincExpr* expr);
	MincObject* getErrorType();

	// >>> MincListExpr

	bool ExprIsList(const MincExpr* expr);
	std::vector<MincExpr*>& getListExprExprs(MincListExpr* expr);
	MincExpr* getListExprExpr(MincListExpr* expr, size_t index);
	size_t getListExprSize(MincListExpr* expr);

	// >>> MincStmt

	bool ExprIsStmt(const MincExpr* expr);
	void codegenStmt(MincStmt* stmt, MincBlockExpr* scope);

	// >>> MincBlockExpr

	bool ExprIsBlock(const MincExpr* expr);
	MincBlockExpr* createEmptyBlockExpr();
	MincBlockExpr* wrapExpr(MincExpr* expr);
	void defineStmt1(MincBlockExpr* scope, const std::vector<MincExpr*>& tplt, StmtBlock codeBlock, void* stmtArgs=nullptr);
	void defineStmt2(MincBlockExpr* scope, const char* tpltStr, StmtBlock codeBlock, void* stmtArgs=nullptr);
	void defineStmt3(MincBlockExpr* scope, const std::vector<MincExpr*>& tplt, MincKernel* stmt);
	void defineStmt4(MincBlockExpr* scope, const char* tpltStr, MincKernel* stmt);
	void lookupStmtCandidates(const MincBlockExpr* scope, const MincStmt* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates);
	size_t countBlockExprStmts(const MincBlockExpr* expr);
	void iterateBlockExprStmts(const MincBlockExpr* expr, std::function<void(const MincListExpr* tplt, const MincKernel* stmt)> cbk);
	void defineDefaultStmt2(MincBlockExpr* scope, StmtBlock codeBlock, void* stmtArgs=nullptr);
	void defineDefaultStmt3(MincBlockExpr* scope, MincKernel* stmt);
	void defineExpr2(MincBlockExpr* scope, const char* tpltStr, ExprBlock codeBlock, MincObject* type, void* exprArgs=nullptr);
	void defineExpr3(MincBlockExpr* scope, const char* tpltStr, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs=nullptr);
	void defineExpr5(MincBlockExpr* scope, MincExpr* tplt, MincKernel* expr);
	void defineExpr6(MincBlockExpr* scope, const char* tpltStr, MincKernel* expr);
	void lookupExprCandidates(const MincBlockExpr* scope, const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates);
	std::string reportExprCandidates(const MincBlockExpr* scope, const MincExpr* expr);
	size_t countBlockExprExprs(const MincBlockExpr* expr);
	void iterateBlockExprExprs(const MincBlockExpr* expr, std::function<void(const MincExpr* tplt, const MincKernel* expr)> cbk);
	void defineDefaultExpr2(MincBlockExpr* scope, ExprBlock codeBlock, MincObject* type, void* exprArgs=nullptr);
	void defineDefaultExpr3(MincBlockExpr* scope, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs=nullptr);
	void defineDefaultExpr5(MincBlockExpr* scope, MincKernel* expr);
	void defineTypeCast2(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, ExprBlock codeBlock, void* castArgs=nullptr);
	void defineInheritanceCast2(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, ExprBlock codeBlock, void* castArgs=nullptr);
	void defineTypeCast3(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, MincKernel* cast);
	void defineInheritanceCast3(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, MincKernel* cast);
	void defineOpaqueTypeCast(MincBlockExpr* scope, MincObject* fromType, MincObject* toType);
	void defineOpaqueInheritanceCast(MincBlockExpr* scope, MincObject* fromType, MincObject* toType);
	MincExpr* lookupCast(const MincBlockExpr* scope, MincExpr* expr, MincObject* toType);
	bool isInstance(const MincBlockExpr* scope, MincObject* fromType, MincObject* toType);
	std::string reportCasts(const MincBlockExpr* scope);
	size_t countBlockExprCasts(const MincBlockExpr* expr);
	void iterateBlockExprCasts(const MincBlockExpr* expr, std::function<void(const MincCast* cast)> cbk);
	void importBlock(MincBlockExpr* scope, MincBlockExpr* block);
	void defineSymbol(MincBlockExpr* scope, const char* name, MincObject* type, MincObject* value);
	const MincSymbol* lookupSymbol(const MincBlockExpr* scope, const char* name);
	const std::string* lookupSymbolName1(const MincBlockExpr* scope, const MincObject* value);
	const std::string& lookupSymbolName2(const MincBlockExpr* scope, const MincObject* value, const std::string& defaultName);
	size_t countBlockExprSymbols(const MincBlockExpr* expr);
	MincSymbol* importSymbol(MincBlockExpr* scope, const char* name);
	void iterateBlockExprSymbols(const MincBlockExpr* expr, std::function<void(const std::string& name, const MincSymbol& symbol)> cbk);
	MincBlockExpr* cloneBlockExpr(MincBlockExpr* expr);
	void resetBlockExpr(MincBlockExpr* expr);
	void resetBlockExprCache(MincBlockExpr* block, size_t targetState);
	const MincStmt* getCurrentBlockExprStmt(const MincBlockExpr* expr);
	MincBlockExpr* getBlockExprParent(const MincBlockExpr* expr);
	void setBlockExprParent(MincBlockExpr* expr, MincBlockExpr* parent);
	const std::vector<MincBlockExpr*>& getBlockExprReferences(const MincBlockExpr* expr);
	void setBlockExprParams(MincBlockExpr* expr, std::vector<MincSymbol>& blockParams);
	MincScopeType* getScopeType(const MincBlockExpr* scope);
	void setScopeType(MincBlockExpr* scope, MincScopeType* scopeType);
	const char* getBlockExprName(const MincBlockExpr* expr);
	void setBlockExprName(MincBlockExpr* expr, const char* name);
	void* getBlockExprUser(const MincBlockExpr* expr);
	void setBlockExprUser(MincBlockExpr* expr, void* user);
	void* getBlockExprUserType(const MincBlockExpr* expr);
	void setBlockExprUserType(MincBlockExpr* expr, void* userType);
	size_t getBlockExprCacheState(MincBlockExpr* block);
	bool isBlockExprBusy(MincBlockExpr* block);
	void removeBlockExpr(MincBlockExpr* expr);
	const MincSymbol& getVoid();
	MincBlockExpr* getRootScope();
	MincBlockExpr* getFileScope();
	void defineImportRule(MincScopeType* fromScope, MincScopeType* toScope, MincObject* symbolType, ImptBlock imptBlock);
	void registerStepEventListener(StepEvent listener, void* eventArgs=nullptr);
	void deregisterStepEventListener(StepEvent listener);

	// >>> MincStopExpr

	bool ExprIsStop(const MincExpr* expr);

	// >>> MincLiteralExpr

	bool ExprIsLiteral(const MincExpr* expr);
	const char* getLiteralExprValue(const MincLiteralExpr* expr);

	// >>> MincIdExpr

	bool ExprIsId(const MincExpr* expr);
	const char* getIdExprName(const MincIdExpr* expr);

	// >>> MincCastExpr

	bool ExprIsCast(const MincExpr* expr);
	MincExpr* getDerivedExpr(MincExpr* expr);
	MincExpr* getCastExprSource(const MincCastExpr* expr);

	// >>> MincPlchldExpr

	bool ExprIsPlchld(const MincExpr* expr);
	char getPlchldExprLabel(const MincPlchldExpr* expr);
	const char* getPlchldExprSublabel(const MincPlchldExpr* expr);

	// >>> MincParamExpr

	bool ExprIsParam(const MincExpr* expr);

	// >>> MincEllipsisExpr

	bool ExprIsEllipsis(const MincExpr* expr);

	// >>> MincArgOpExpr

	bool ExprIsArgOp(const MincExpr* expr);

	// >>> MincEncOpExpr

	bool ExprIsEncOp(const MincExpr* expr);

	// >>> MincTerOpExpr

	bool ExprIsTerOp(const MincExpr* expr);

	// >>> MincPrefixExpr

	bool ExprIsPrefixOp(const MincExpr* expr);

	// >>> MincPostfixExpr

	bool ExprIsPostfixOp(const MincExpr* expr);

	// >>> MincBinOpExpr

	bool ExprIsBinOp(const MincExpr* expr);
	const char* getBinOpExprOpStr(const MincBinOpExpr* expr);
	MincExpr* getBinOpExprOperand1(const MincBinOpExpr* expr);
	MincExpr* getBinOpExprOperand2(const MincBinOpExpr* expr);

	// >>> MincVarBinOpExpr

	bool ExprIsVarBinOp(const MincExpr* expr);
}

#endif