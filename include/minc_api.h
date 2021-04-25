#ifndef __MINC_API_H
#define __MINC_API_H

#include <istream>
#include <map>
#include <string>
#include <vector>
#include <functional>

#include "minc_types.h"

extern "C"
{
	// >>> Parser

	MincFlavor flavorFromFile(const char* filename);
	MincBlockExpr* parseStream(std::istream& stream, MincFlavor flavor);
	MincBlockExpr* parseFile(const char* filename, MincFlavor flavor=MincFlavor::C_FLAVOR);
	MincBlockExpr* parseCode(const char* code, MincFlavor flavor);
	const std::vector<MincExpr*> parseTplt(const char* tpltStr, MincFlavor flavor);

	// >>> C Parser

	MincBlockExpr* parseCStream(std::istream& stream);
	MincBlockExpr* parseCFile(const char* filename);
	MincBlockExpr* parseCCode(const char* code);
	const std::vector<MincExpr*> parseCTplt(const char* tpltStr);

	// >>> Python Parser

	MincBlockExpr* parsePythonStream(std::istream& stream);
	MincBlockExpr* parsePythonFile(const char* filename);
	MincBlockExpr* parsePythonCode(const char* code);
	const std::vector<MincExpr*> parsePythonTplt(const char* tpltStr);

	// >>> Go Parser

	MincBlockExpr* parseGoStream(std::istream& stream);
	MincBlockExpr* parseGoFile(const char* filename);
	MincBlockExpr* parseGoCode(const char* code);
	const std::vector<MincExpr*> parseGoTplt(const char* tpltStr);

	// >>> MincException

	void raiseCompileError(const char* msg, const MincExpr* loc=nullptr);

	// >>> MincExpr

	bool runExpr(MincExpr* expr, MincRuntime& runtime);
	MincObject* getType1(const MincExpr* expr, const MincBlockExpr* scope);
	MincObject* getType2(MincExpr* expr, MincBlockExpr* scope);
	void collectParams(const MincBlockExpr* scope, const MincExpr* tplt, MincExpr* expr, std::vector<MincExpr*>& params);
	void resolveExpr(MincExpr* expr, MincBlockExpr* scope);
	void forgetExpr(MincExpr* expr);
	MincSymbol& buildExpr(MincExpr* expr, MincBuildtime& buildtime);
	char* ExprToString(const MincExpr* expr);
	char* ExprToShortString(const MincExpr* expr);
	MincExpr* cloneExpr(const MincExpr* expr);
	const MincLocation& getLocation(const MincExpr* expr);
	const char* getExprFilename(const MincExpr* expr);
	unsigned getExprLine(const MincExpr* expr);
	unsigned getExprColumn(const MincExpr* expr);
	unsigned getExprEndLine(const MincExpr* expr);
	unsigned getExprEndColumn(const MincExpr* expr);
	MincObject* getErrorType();
	MincSymbol evalCExpr(const char* code, MincBlockExpr* scope);
	MincSymbol evalPythonExpr(const char* code, MincBlockExpr* scope);

	// >>> MincListExpr

	bool ExprIsList(const MincExpr* expr);
	std::vector<MincExpr*>& getListExprExprs(MincListExpr* expr);
	MincExpr* getListExprExpr(MincListExpr* expr, size_t index);
	size_t getListExprSize(MincListExpr* expr);

	// >>> MincStmt

	bool ExprIsStmt(const MincExpr* expr);
	void evalCStmt(const char* code, MincBlockExpr* scope);
	void evalPythonStmt(const char* code, MincBlockExpr* scope);

	// >>> MincBlockExpr

	bool ExprIsBlock(const MincExpr* expr);
	MincBlockExpr* createEmptyBlockExpr();
	MincBlockExpr* wrapExpr(MincExpr* expr);
	void defineStmt1(MincBlockExpr* refScope, const std::vector<MincExpr*>& tplt, RunBlock codeBlock, void* stmtArgs=nullptr, MincBlockExpr* scope=nullptr);
	void defineStmt2(MincBlockExpr* refScope, const char* tpltStr, RunBlock codeBlock, void* stmtArgs=nullptr, MincBlockExpr* scope=nullptr);
	void defineStmt3(MincBlockExpr* refScope, const std::vector<MincExpr*>& tplt, MincKernel* stmt, MincBlockExpr* scope=nullptr);
	void defineStmt4(MincBlockExpr* refScope, const char* tpltStr, MincKernel* stmt, MincBlockExpr* scope=nullptr);
	void defineStmt5(MincBlockExpr* refScope, const char* tpltStr, BuildBlock buildBlock, void* stmtArgs=nullptr, MincBlockExpr* scope=nullptr);
	void defineStmt6(MincBlockExpr* refScope, const char* tpltStr, BuildBlock buildBlock, RunBlock runBlock, void* stmtArgs=nullptr, MincBlockExpr* scope=nullptr);
	void lookupStmtCandidates(const MincBlockExpr* scope, const MincStmt* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates);
	size_t countBlockExprStmts(const MincBlockExpr* expr);
	void iterateBlockExprStmts(const MincBlockExpr* expr, std::function<void(const MincListExpr* tplt, MincKernel* stmt)> cbk);
	void defineDefaultStmt2(MincBlockExpr* scope, RunBlock codeBlock, void* stmtArgs=nullptr);
	void defineDefaultStmt3(MincBlockExpr* scope, MincKernel* stmt);
	void defineDefaultStmt5(MincBlockExpr* scope, BuildBlock buildBlock, void* stmtArgs=nullptr);
	void defineDefaultStmt6(MincBlockExpr* scope, BuildBlock buildBlock, RunBlock runBlock, void* stmtArgs=nullptr);
	void defineExpr2(MincBlockExpr* scope, const char* tpltStr, RunBlock codeBlock, MincObject* type, void* exprArgs=nullptr);
	void defineExpr3(MincBlockExpr* scope, const char* tpltStr, RunBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs=nullptr);
	void defineExpr5(MincBlockExpr* scope, MincExpr* tplt, MincKernel* expr);
	void defineExpr6(MincBlockExpr* scope, const char* tpltStr, MincKernel* expr);
	void defineExpr7(MincBlockExpr* scope, const char* tpltStr, BuildBlock buildBlock, MincObject* type, void* exprArgs=nullptr);
	void defineExpr8(MincBlockExpr* scope, const char* tpltStr, BuildBlock buildBlock, ExprTypeBlock typeBlock, void* exprArgs=nullptr);
	void defineExpr9(MincBlockExpr* scope, const char* tpltStr, BuildBlock buildBlock, RunBlock runBlock, MincObject* type, void* exprArgs=nullptr);
	void defineExpr10(MincBlockExpr* scope, const char* tpltStr, BuildBlock buildBlock, RunBlock runBlock, ExprTypeBlock typeBlock, void* exprArgs=nullptr);
	void lookupExprCandidates(const MincBlockExpr* scope, const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates);
	size_t countBlockExprExprs(const MincBlockExpr* expr);
	void iterateBlockExprExprs(const MincBlockExpr* expr, std::function<void(const MincExpr* tplt, MincKernel* expr)> cbk);
	void defineDefaultExpr2(MincBlockExpr* scope, RunBlock codeBlock, MincObject* type, void* exprArgs=nullptr);
	void defineDefaultExpr3(MincBlockExpr* scope, RunBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs=nullptr);
	void defineDefaultExpr5(MincBlockExpr* scope, MincKernel* expr);
	void defineTypeCast2(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, RunBlock codeBlock, void* castArgs=nullptr);
	void defineTypeCast3(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, MincKernel* cast);
	void defineTypeCast9(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, BuildBlock buildBlock, RunBlock runBlock, void* castArgs=nullptr);
	void defineInheritanceCast2(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, RunBlock codeBlock, void* castArgs=nullptr);
	void defineInheritanceCast3(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, MincKernel* cast);
	void defineInheritanceCast9(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, BuildBlock buildBlock, RunBlock runBlock, void* castArgs=nullptr);
	void defineOpaqueTypeCast(MincBlockExpr* scope, MincObject* fromType, MincObject* toType);
	void defineOpaqueInheritanceCast(MincBlockExpr* scope, MincObject* fromType, MincObject* toType);
	MincExpr* lookupCast(const MincBlockExpr* scope, MincExpr* expr, MincObject* toType);
	bool isInstance(const MincBlockExpr* scope, MincObject* fromType, MincObject* toType);
	std::string reportCasts(const MincBlockExpr* scope);
	size_t countBlockExprCasts(const MincBlockExpr* expr);
	void iterateBlockExprCasts(const MincBlockExpr* expr, std::function<void(const MincCast* cast)> cbk);
	void iterateBases(const MincBlockExpr* expr, MincObject* derivedType, std::function<void(MincObject* baseType)> cbk);
	void importBlock(MincBlockExpr* scope, MincBlockExpr* block);
	void defineSymbol(MincBlockExpr* scope, const char* name, MincObject* type, MincObject* value);
	const MincSymbol* lookupSymbol(const MincBlockExpr* scope, const char* name);
	const std::string* lookupSymbolName1(const MincBlockExpr* scope, const MincObject* value);
	const std::string& lookupSymbolName2(const MincBlockExpr* scope, const MincObject* value, const std::string& defaultName);
	size_t countBlockExprSymbols(const MincBlockExpr* expr);
	size_t countBlockExprBuildtimeSymbols(const MincBlockExpr* expr);
	size_t countBlockExprStackSymbols(const MincBlockExpr* expr);
	MincSymbol* importSymbol(MincBlockExpr* scope, const char* name);
	void iterateBlockExprSymbols(const MincBlockExpr* expr, std::function<void(const std::string& name, const MincSymbol& symbol)> cbk);
	const MincStackSymbol* allocStackSymbol(MincBlockExpr* scope, const char* name, MincObject* type, size_t size);
	const MincStackSymbol* allocAnonymousStackSymbol(MincBlockExpr* scope, MincObject* type, size_t size);
	const MincStackSymbol* lookupStackSymbol(const MincBlockExpr* scope, const char* name);
	MincObject* getStackSymbol(MincRuntime& runtime, const MincStackSymbol* stackSymbol);
	MincObject* getStackSymbolOfNextStackFrame(MincRuntime& runtime, const MincStackSymbol* stackSymbol);
	MincBlockExpr* cloneBlockExpr(MincBlockExpr* expr);
	void resetBlockExpr(MincBlockExpr* expr);
	void resetBlockExprCache(MincBlockExpr* block, size_t targetState);
	MincBlockExpr* getBlockExprParent(const MincBlockExpr* expr);
	void setBlockExprParent(MincBlockExpr* expr, MincBlockExpr* parent);
	const std::vector<MincBlockExpr*>& getBlockExprReferences(const MincBlockExpr* expr);
	void addBlockExprReference(MincBlockExpr* expr, MincBlockExpr* reference);
	void clearBlockExprReferences(MincBlockExpr* expr);
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
	bool isResumable(MincBlockExpr* block);
	void setResumable(MincBlockExpr* block, bool resumable);
	bool isBlockExprBusy(MincBlockExpr* block);
	void removeBlockExpr(MincBlockExpr* expr);
	const MincSymbol& getVoid();
	MincBlockExpr* getRootScope();
	MincBlockExpr* getFileScope();
	void defineImportRule(MincScopeType* fromScope, MincScopeType* toScope, MincObject* symbolType, ImptBlock imptBlock);
	void registerStepEventListener(StepEvent listener, void* eventArgs=nullptr);
	void deregisterStepEventListener(StepEvent listener);
	void evalCBlock(const char* code, MincBlockExpr* scope);
	void evalPythonBlock(const char* code, MincBlockExpr* scope);
	void evalGoBlock(const char* code, MincBlockExpr* scope);
	MincEnteredBlockExpr* enterBlockExpr(MincRuntime& runtime, const MincBlockExpr* block);
	void exitBlockExpr(MincEnteredBlockExpr* enteredBlock);

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

#ifdef __cplusplus
inline MincObject* getType(const MincExpr* expr, const MincBlockExpr* scope) { return getType1(expr, scope); }
inline MincObject* getType(MincExpr* expr, MincBlockExpr* scope) { return getType2(expr, scope); }
#else
#define getType(expr, scope) _Generic((expr), const MincExpr*: getType1, MincExpr*: getType2)(expr, scope)
#endif

#endif