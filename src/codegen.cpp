// STD
#include <string>
#include <vector>
#include <set>
#include <map>

// Local includes
#include "api.h"
#include "cparser.h"

class KaleidoscopeJIT;
class FileModule;

const Variable VOID = Variable(new BaseType(), nullptr);

// Misc
BlockExprAST* rootBlock = nullptr;
BlockExprAST* fileBlock = nullptr;
std::map<const BaseType*, TypeDescription> typereg;
std::set<StepEvent> stepEventListeners;
const std::string NULL_TYPE = "NULL";
const std::string UNKNOWN_TYPE = "UNKNOWN_TYPE";

struct StaticStmtContext : public CodegenContext
{
private:
	StmtBlock cbk;
	void* stmtArgs;
public:
	StaticStmtContext(StmtBlock cbk, void* stmtArgs = nullptr) : cbk(cbk), stmtArgs(stmtArgs) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		cbk(parentBlock, params, stmtArgs);
		return VOID;
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return nullptr;
	}
};
struct StaticExprContext : public CodegenContext
{
private:
	ExprBlock cbk;
	BaseType* const type;
	void* exprArgs;
public:
	StaticExprContext(ExprBlock cbk, BaseType* type, void* exprArgs = nullptr) : cbk(cbk), type(type), exprArgs(exprArgs) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		return cbk(parentBlock, params, exprArgs);
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return type;
	}
};
struct StaticExprContext2 : public CodegenContext
{
private:
	ExprBlock cbk;
	ExprTypeBlock typecbk;
	void* exprArgs;
public:
	StaticExprContext2(ExprBlock cbk, ExprTypeBlock typecbk, void* exprArgs = nullptr) : cbk(cbk), typecbk(typecbk), exprArgs(exprArgs) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		return cbk(parentBlock, params, exprArgs);
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return typecbk(parentBlock, params, exprArgs);
	}
};
struct OpaqueExprContext : public CodegenContext
{
private:
	BaseType* const type;
public:
	OpaqueExprContext(BaseType* type) : type(type) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		return Variable(type, params[0]->codegen(parentBlock).value);
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return type;
	}
};

extern "C"
{
	Variable codegenExpr(ExprAST* expr, BlockExprAST* scope)
	{
		return expr->codegen(scope);
	}

	uint64_t codegenExprConstant(ExprAST* expr, BlockExprAST* scope)
	{
		return expr->codegen(scope).value->getConstantValue();
	}

	void codegenStmt(StmtAST* stmt, BlockExprAST* scope)
	{
		stmt->codegen(scope);
	}

	BaseType* getType(ExprAST* expr, const BlockExprAST* scope)
	{
		return expr->getType(scope);
	}

	void collectParams(const BlockExprAST* scope, const ExprAST* tplt, ExprAST* expr, std::vector<ExprAST*>& params)
	{
		size_t paramIdx = params.size();
		tplt->collectParams(scope, expr, params, paramIdx);
	}

	std::string ExprASTToString(const ExprAST* expr)
	{
		return expr->str();
	}

	std::string StmtASTToString(const StmtAST* stmt)
	{
		return stmt->str();
	}

	bool ExprASTIsId(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::ID;
	}
	bool ExprASTIsCast(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::CAST;
	}
	bool ExprASTIsParam(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::PARAM;
	}
	bool ExprASTIsBlock(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::BLOCK;
	}

	void resolveExprAST(BlockExprAST* scope, ExprAST* expr)
	{
		expr->resolveTypes(scope);
	}

	BlockExprAST* wrapExprAST(ExprAST* expr)
	{
		return new BlockExprAST(expr->loc, new std::vector<ExprAST*>(1, expr));
	}

	std::vector<ExprAST*>& getExprListASTExpressions(ExprListAST* expr)
	{
		return expr->exprs;
	}
	ExprAST* getExprListASTExpression(ExprListAST* expr, size_t index)
	{
		return expr->exprs[index];
	}
	size_t getExprListASTSize(ExprListAST* expr)
	{
		return expr->exprs.size();
	}
	const char* getIdExprASTName(const IdExprAST* expr)
	{
		return expr->name;
	}
	const char* getLiteralExprASTValue(const LiteralExprAST* expr)
	{
		return expr->value;
	}
	BlockExprAST* getBlockExprASTParent(const BlockExprAST* expr)
	{
		return expr->parent;
	}
	void setBlockExprASTParent(BlockExprAST* expr, BlockExprAST* parent)
	{
		expr->parent = parent;
	}
	ExprAST* getCastExprASTSource(const CastExprAST* expr)
	{
		return expr->resolvedParams[0];
	}

	const Location* getExprLoc(const ExprAST* expr) { return &expr->loc; }
	const char* getExprFilename(const ExprAST* expr) { return expr->loc.filename; }
	unsigned getExprLine(const ExprAST* expr) { return expr->loc.begin_line; }
	unsigned getExprColumn(const ExprAST* expr) { return expr->loc.begin_col; }
	unsigned getExprEndLine(const ExprAST* expr) { return expr->loc.end_line; }
	unsigned getExprEndColumn(const ExprAST* expr) { return expr->loc.end_col; }

	BlockExprAST* getRootScope()
	{
		return rootBlock;
	}
	BlockExprAST* getFileScope()
	{
		return fileBlock;
	}

	const std::string& getTypeName(const BaseType* type)
	{
		if (type == nullptr)
			return NULL_TYPE;
		const auto typeDesc = typereg.find(type);
		if (typeDesc == typereg.cend())
			return UNKNOWN_TYPE;
		else
			return typeDesc->second.name;
	}
	const char* getTypeName2(const BaseType* type)
	{
		if (type == nullptr)
			return NULL_TYPE.c_str();
		const auto typeDesc = typereg.find(type);
		if (typeDesc == typereg.cend())
			return UNKNOWN_TYPE.c_str();
		else
			return typeDesc->second.name.c_str();
	}

	void defineSymbol(BlockExprAST* scope, const char* name, BaseType* type, BaseValue* value)
	{
		scope->addToScope(name, type, value);
	}

	void defineType(const char* name, BaseType* type)
	{
		typereg[type] = TypeDescription{name};
	}

	void defineStmt2(BlockExprAST* scope, const char* tpltStr, StmtBlock codeBlock, void* stmtArgs)
	{
		// Append STOP expr to make tpltStr a valid statement
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';

		// Parse tpltStr into tpltBlock
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse() || tpltBlock->exprs->size() < 2)
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);

		// Remove appended STOP expr if last expr is $B
		assert(tpltBlock->exprs->back()->exprtype == ExprAST::ExprType::STOP);
		const PlchldExprAST* lastExpr = (const PlchldExprAST*)tpltBlock->exprs->at(tpltBlock->exprs->size() - 2);
		if (lastExpr->exprtype == ExprAST::ExprType::PLCHLD && lastExpr->p1 == 'B')
			tpltBlock->exprs->pop_back();
	
		scope->defineStatement(*tpltBlock->exprs, new StaticStmtContext(codeBlock, stmtArgs));
	}

	void defineExpr2(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, BaseType* type, void* exprArgs)
	{
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);
		ExprAST* tplt = tpltBlock->exprs->at(0);
		scope->defineExpr(tplt, new StaticExprContext(codeBlock, type, exprArgs));
	}

	void defineExpr3(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs)
	{
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);
		ExprAST* tplt = tpltBlock->exprs->at(0);
		scope->defineExpr(tplt, new StaticExprContext2(codeBlock, typeBlock, exprArgs));
	}

	void defineCast2(BlockExprAST* scope, BaseType* fromType, BaseType* toType, ExprBlock codeBlock, void* castArgs)
	{
		scope->defineCast(fromType, toType, new StaticExprContext(codeBlock, toType, castArgs));
	}

	void defineOpaqueCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType)
	{
		scope->defineCast(fromType, toType, new OpaqueExprContext(toType));
	}

	const Variable* lookupSymbol(const BlockExprAST* scope, const char* name, bool& isCaptured)
	{
		return scope->lookupScope(name, isCaptured);
	}

	ExprAST* lookupCast(const BlockExprAST* scope, ExprAST* expr, BaseType* toType)
	{
		BaseType* fromType = (expr->exprtype == ExprAST::ExprType::CAST ? expr->resolvedParams[0] : expr)->getType(scope);
		if (fromType == toType)
			return expr;

		CodegenContext* castContext = scope->lookupCast(fromType, toType);
		if (castContext == nullptr)
			return nullptr;

		ExprAST* castExpr = new CastExprAST(expr->loc);
		castExpr->resolvedContext = castContext;
		castExpr->resolvedParams.push_back(expr);
		return castExpr;
	}

	std::string reportExprCandidates(const BlockExprAST* scope, const ExprAST* expr)
	{
		std::string report = "";
		std::multimap<MatchScore, const std::pair<const ExprAST*const, CodegenContext*>&> candidates;
		std::vector<ExprAST*> resolvedParams;
		scope->lookupExprCandidates(expr, candidates);
		for (auto& candidate: candidates)
		{
			const MatchScore score = candidate.first;
			const std::pair<const ExprAST*const, CodegenContext*>& context = candidate.second;
			size_t paramIdx = 0;
			resolvedParams.clear();
			context.first->collectParams(scope, const_cast<ExprAST*>(expr), resolvedParams, paramIdx);
			const std::string& typeName = getTypeName(context.second->getType(scope, resolvedParams));
			report += "\tcandidate(score=" + std::to_string(score) + "): " +  context.first->str() + "<" + typeName + ">\n";
		}
		return report;
	}

	std::string reportCasts(const BlockExprAST* scope)
	{
		std::string report = "";
		std::list<std::pair<BaseType*, BaseType*>> casts;
		scope->listAllCasts(casts);
		for (auto& cast: casts)
			report += "\t" +  getTypeName(cast.first) + " -> " + getTypeName(cast.second) + "\n";
		return report;
	}

	const Variable& getVoid()
	{
		return VOID;
	}

	void raiseCompileError(const char* msg, const ExprAST* loc)
	{
		throw CompileError(msg, loc->loc);
	}

	void registerStepEventListener(StepEvent listener)
	{
		stepEventListeners.insert(listener);
	}

	void deregisterStepEventListener(StepEvent listener)
	{
		stepEventListeners.erase(listener);
	}
}

void raiseStepEvent(const ExprAST* loc)
{
	for (StepEvent listener: stepEventListeners)
		listener(loc);
}

StmtAST::StmtAST(ExprASTIter exprBegin, ExprASTIter exprEnd, CodegenContext* context)
	: ExprAST(Location{ exprBegin[0]->loc.filename, exprBegin[0]->loc.begin_line, exprBegin[0]->loc.begin_col, exprEnd[-1]->loc.end_line, exprEnd[-1]->loc.end_col }, ExprAST::ExprType::STMT),
	begin(exprBegin), end(exprEnd)
{
	resolvedContext = context;
}

Variable BlockExprAST::codegen(BlockExprAST* parentBlock)
{
	parent = parentBlock;

	if (fileBlock == nullptr)
	{
		rootBlock = this;
		fileBlock = this;
	}

	for (ExprASTIter iter = exprs->cbegin(); iter != exprs->cend();)
	{
		const ExprASTIter beginExpr = iter;
		const std::pair<const std::vector<ExprAST*>, CodegenContext*>* stmtContext = lookupStatement(iter, exprs->cend());
		const ExprASTIter endExpr = iter;

		StmtAST stmt(beginExpr, endExpr, stmtContext ? stmtContext->second : nullptr);

		if (stmtContext)
		{
			stmt.collectParams(this, stmtContext->first);
			stmt.codegen(this);
		}
		else
			throw UndefinedStmtException(&stmt);
	}

	raiseStepEvent(nullptr);

	if (fileBlock == this)
	{
		rootBlock = nullptr;
		fileBlock = nullptr;
	}

	//parent = nullptr;
	return VOID;
}

Variable ExprAST::codegen(BlockExprAST* parentBlock)
{
	if (!resolvedContext)
		parentBlock->lookupExpr(this);

	if (resolvedContext)
	{
		raiseStepEvent(this);
		const Variable var = resolvedContext->codegen(parentBlock, resolvedParams);
		const BaseType *expectedType = resolvedContext->getType(parentBlock, resolvedParams), *gotType = var.type;
		if (expectedType != gotType)
		{
			throw CompileError(
				("invalid expression return type: " + ExprASTToString(this) + "<" + getTypeName(gotType) + ">, expected: <" + getTypeName(expectedType) + ">").c_str(),
				this->loc
			);
		}
		return var;
	}
	else
		throw UndefinedExprException{this};
}

BaseType* ExprAST::getType(const BlockExprAST* parentBlock) const
{
	return resolvedContext ? resolvedContext->getType(parentBlock, resolvedParams) : nullptr;
}

Variable StmtAST::codegen(BlockExprAST* parentBlock)
{
	raiseStepEvent(this);
	resolvedContext->codegen(parentBlock, resolvedParams);
	return VOID;
}

void ExprAST::resolveTypes(BlockExprAST* block)
{
	block->lookupExpr(this);
}

BaseType* PlchldExprAST::getType(const BlockExprAST* parentBlock) const
{
	if (p2 == nullptr)
		return nullptr;
	const Variable* var = parentBlock->lookupScope(p2);
	if (var == nullptr)
		throw UndefinedIdentifierException(new IdExprAST(loc, p2));
	return (BaseType*)var->value->getConstantValue();
}

Variable ParamExprAST::codegen(BlockExprAST* parentBlock)
{
	const std::vector<Variable>* blockParams = parentBlock->getBlockParams();
	if (blockParams == nullptr)
		throw CompileError("invalid use of parameter expression in parameterless scope", loc);
	if (staticIdx >= blockParams->size())
		throw CompileError("parameter index out of bounds", loc);
	return blockParams->at(staticIdx);
}

BaseType* ParamExprAST::getType(const BlockExprAST* parentBlock) const
{
	const std::vector<Variable>* blockParams = parentBlock->getBlockParams();
	if (blockParams == nullptr || staticIdx >= blockParams->size())
		return nullptr;
	return blockParams->at(staticIdx).type;
}