#include <string>
#include <map>
#include <cstring>
#include <iostream>
#include "api.h"
#include "codegen.h"
#include "builtin.h"

template<typename T> struct PawsType : BaseValue
{
	T val;
	static inline BaseType* TYPE = new BaseType();
	PawsType() {}
	PawsType(const T& val) : val(val) {}
	uint64_t getConstantValue() { return 0; }
};
template<> uint64_t PawsType<BaseType*>::getConstantValue() { return (uint64_t)val; }
namespace std
{
	template<typename T> struct less<PawsType<T>*>
	{
		bool operator()(const PawsType<T>* lhs, const PawsType<T>* rhs) const { return lhs->val < rhs->val; }
	};
}

template<int T> struct PawsOpaqueType
{
	static inline BaseType* TYPE = new BaseType();
};

typedef PawsOpaqueType<0> PawsBase;
typedef PawsOpaqueType<1> PawsVoid;
typedef PawsType<BaseType*> PawsMetaType;
typedef PawsType<int> PawsInt;
typedef PawsType<std::string> PawsString;
typedef PawsType<ExprAST*> PawsExprAST;
typedef PawsType<BlockExprAST*> PawsBlockExprAST;
typedef PawsType<IModule*> PawsModule;
typedef PawsType<std::map<PawsString*, PawsString*>> PawsStringMap;
typedef PawsType<std::map<PawsExprAST*, PawsExprAST*>> PawsExprASTMap;


struct ReturnException
{
	const int result;
	ReturnException(int result) : result(result) {}
};

int PAWRun(BlockExprAST* block, int argc, char **argv)
{
	defineType("PawsBase", PawsBase::TYPE);
	defineSymbol(block, "PawsBase", getBaseType(), new PawsMetaType(PawsBase::TYPE));

	defineType("PawsVoid", PawsVoid::TYPE);
	defineSymbol(block, "PawsVoid", PawsMetaType::TYPE, new PawsMetaType(PawsVoid::TYPE));
	defineOpaqueCast(block, PawsVoid::TYPE, PawsBase::TYPE);

	defineType("PawsMetaType", PawsMetaType::TYPE);
	defineSymbol(block, "PawsMetaType", PawsMetaType::TYPE, new PawsMetaType(PawsMetaType::TYPE));
	defineOpaqueCast(block, PawsMetaType::TYPE, PawsBase::TYPE);

	defineType("PawsInt", PawsInt::TYPE);
	defineSymbol(block, "PawsInt", PawsMetaType::TYPE, new PawsMetaType(PawsInt::TYPE));
	defineOpaqueCast(block, PawsInt::TYPE, PawsBase::TYPE);

	defineType("PawsString", PawsString::TYPE);
	defineSymbol(block, "PawsString", PawsMetaType::TYPE, new PawsMetaType(PawsString::TYPE));
	defineOpaqueCast(block, PawsString::TYPE, PawsBase::TYPE);

	defineType("PawsBlockExprAST", PawsBlockExprAST::TYPE);
	defineSymbol(block, "PawsBlockExprAST", PawsMetaType::TYPE, new PawsMetaType(PawsBlockExprAST::TYPE));
	defineOpaqueCast(block, PawsBlockExprAST::TYPE, PawsBase::TYPE);

	defineType("PawsModule", PawsModule::TYPE);
	defineSymbol(block, "PawsModule", PawsMetaType::TYPE, new PawsMetaType(PawsModule::TYPE));
	defineOpaqueCast(block, PawsModule::TYPE, PawsBase::TYPE);

	defineType("PawsStringMap", PawsStringMap::TYPE);
	defineSymbol(block, "PawsStringMap", PawsMetaType::TYPE, new PawsMetaType(PawsStringMap::TYPE));
	defineOpaqueCast(block, PawsStringMap::TYPE, PawsBase::TYPE);

	std::vector<Variable> blockParams;
	blockParams.reserve(argc);
	for (int i = 0; i < argc; ++i)
		blockParams.push_back(Variable(PawsString::TYPE, new PawsString(std::string(argv[i]))));
	setBlockExprASTParams(block, blockParams);

	defineSymbol(block, "FILE_SCOPE", PawsBlockExprAST::TYPE, new PawsBlockExprAST(block));

	// Define single-expr statement
	defineStmt2(block, "$E<PawsBase>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define context-free block statement
	defineStmt2(block, "$B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define general bracketed expression
	defineExpr3(block, "($E)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return codegenExpr(params[0], parentBlock);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			return getType(params[0], parentBlock);
		}
	);

	// Define return statement
	defineStmt2(block, "return $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			throw ReturnException(((PawsInt*)codegenExpr(params[0], parentBlock).value)->val);
		}
	);

	// Define variable lookup
	defineExpr3(block, "$I",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			bool isCaptured;
			const Variable* var = lookupSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]), isCaptured);
			if (var == nullptr)
				raiseCompileError(("`" + std::string(getIdExprASTName((IdExprAST*)params[0])) + "` was not declared in this scope").c_str(), params[0]);
			return *var;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			bool isCaptured;
			const Variable* var = lookupSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]), isCaptured);
			return var != nullptr ? var->type : nullptr;
		}
	);

	// Define literal definition
	defineExpr3(block, "$L",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);

			if (value[0] == '"' || value[0] == '\'')
				return Variable(PawsString::TYPE, new PawsString(std::string(value + 1, strlen(value) - 2)));
			
			int intValue;
			if (value[0] == '0' && value[1] == 'x')
				intValue = std::stoi(value, 0, 16);
			else
				intValue = std::stoi(value, 0, 10);
			return Variable(PawsInt::TYPE, new PawsInt(intValue));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);
			return value[0] == '"' || value[0] == '\'' ? PawsString::TYPE : PawsInt::TYPE;
		}
	);

	// Define variable assignment
	defineExpr3(block, "$I = $E<PawsBase>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[1];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			Variable expr = codegenExpr(exprAST, parentBlock);

			defineSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]), expr.type, expr.value);
			return expr;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			ExprAST* exprAST = params[1];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return getType(exprAST, parentBlock);
		}
	);

	// Define string concatenation
	defineExpr2(block, "$E<PawsString> + $E<PawsString>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* a = (PawsString*)codegenExpr(params[0], parentBlock).value;
			PawsString* b = (PawsString*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsString::TYPE, new PawsString(a->val + b->val));
		},
		PawsString::TYPE
	);

	// Define string length getter
	defineExpr2(block, "$E<PawsString>.length()",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* a = (PawsString*)codegenExpr(params[0], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val.length()));
		},
		PawsInt::TYPE
	);

	// Define substring
	defineExpr2(block, "$E<PawsString>.substr($E<PawsInt>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* a = (PawsString*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsString::TYPE, new PawsString(a->val.substr(b->val)));
		},
		PawsString::TYPE
	);
	defineExpr2(block, "$E<PawsString>.substr($E<PawsInt>, $E<PawsInt>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* a = (PawsString*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			PawsInt* c = (PawsInt*)codegenExpr(params[2], parentBlock).value;
			return Variable(PawsString::TYPE, new PawsString(a->val.substr(b->val, c->val)));
		},
		PawsString::TYPE
	);

	// Define substring finder
	defineExpr2(block, "$E<PawsString>.find($E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* a = (PawsString*)codegenExpr(params[0], parentBlock).value;
			PawsString* b = (PawsString*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val.find(b->val)));
		},
		PawsInt::TYPE
	);
	defineExpr2(block, "$E<PawsString>.rfind($E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* a = (PawsString*)codegenExpr(params[0], parentBlock).value;
			PawsString* b = (PawsString*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val.rfind(b->val)));
		},
		PawsInt::TYPE
	);

	// Define string concatenation
	defineExpr2(block, "$E<PawsInt> * $E<PawsString>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsString* b = (PawsString*)codegenExpr(params[1], parentBlock).value;
			PawsString* result = new PawsString("");
			for (int i = 0; i < a->val; ++i)
				result->val += b->val;
			return Variable(PawsString::TYPE, result);
		},
		PawsString::TYPE
	);
	defineExpr2(block, "$E<PawsString> * $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* a = (PawsString*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			PawsString* result = new PawsString("");
			for (int i = 0; i < b->val; ++i)
				result->val += a->val;
			return Variable(PawsString::TYPE, result);
		},
		PawsString::TYPE
	);

	// Define string relations
	defineExpr2(block, "$E<PawsString> == $E<PawsString>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* a = (PawsString*)codegenExpr(params[0], parentBlock).value;
			PawsString* b = (PawsString*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val == b->val));
		},
		PawsInt::TYPE
	);
	defineExpr2(block, "$E<PawsString> != $E<PawsString>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* a = (PawsString*)codegenExpr(params[0], parentBlock).value;
			PawsString* b = (PawsString*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val != b->val));
		},
		PawsInt::TYPE
	);

	// Define integer addition
	defineExpr2(block, "$E<PawsInt> + $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val + b->val));
		},
		PawsInt::TYPE
	);

	// Define integer subtraction
	defineExpr2(block, "$E<PawsInt> - $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val - b->val));
		},
		PawsInt::TYPE
	);

	// Define integer minimum
	defineExpr2(block, "min($E<PawsInt>, $E<PawsInt>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val < b->val ? a->val : b->val));
		},
		PawsInt::TYPE
	);

	// Define integer maximum
	defineExpr2(block, "max($E<PawsInt>, $E<PawsInt>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val > b->val ? a->val : b->val));
		},
		PawsInt::TYPE
	);

	// Define integer relations
	defineExpr2(block, "$E<PawsInt> == $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val == b->val));
		},
		PawsInt::TYPE
	);
	defineExpr2(block, "$E<PawsInt> != $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val != b->val));
		},
		PawsInt::TYPE
	);
	defineExpr2(block, "$E<PawsInt> <= $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val <= b->val));
		},
		PawsInt::TYPE
	);
	defineExpr2(block, "$E<PawsInt> >= $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val >= b->val));
		},
		PawsInt::TYPE
	);

	// Define logical operators
	defineExpr2(block, "$E<PawsInt> && $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val && b->val));
		},
		PawsInt::TYPE
	);
	defineExpr2(block, "$E<PawsInt> || $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(a->val || b->val));
		},
		PawsInt::TYPE
	);

	// Define boolean negation
	defineExpr2(block, "!$E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(!a->val));
		},
		PawsInt::TYPE
	);

	// Define is-NULL
	defineExpr2(block, "$E == NULL",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsInt::TYPE, new PawsInt(getType(params[0], parentBlock) == nullptr));
		},
		PawsInt::TYPE
	);
	defineExpr2(block, "$E != NULL",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsInt::TYPE, new PawsInt(getType(params[0], parentBlock) != nullptr));
		},
		PawsInt::TYPE
	);

	// Define if statement
	defineStmt2(block, "if($E<PawsInt>) $S",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsInt* condition = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			if (condition->val)
				codegenExpr(params[1], parentBlock);
		}
	);

	// Define if/else statement
	defineStmt2(block, "if($E<PawsInt>) $S else $S",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsInt* condition = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			if (condition->val)
				codegenExpr(params[1], parentBlock);
			else
				codegenExpr(params[2], parentBlock);
		}
	);

	// Define inline if expression
	defineExpr3(block, "$E<PawsInt> ? $E : $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return codegenExpr(params[((PawsInt*)codegenExpr(params[0], parentBlock).value)->val ? 1 : 2], parentBlock);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			BaseType* ta = getType(params[1], parentBlock);
			BaseType* tb = getType(params[2], parentBlock);
			if (ta != tb)
				raiseCompileError("TODO", params[0]);
			return ta;
		}
	);

	// Define map iterating for statement
	defineStmt2(block, "for ($I, $I: $E<PawsStringMap>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* keyExpr = (IdExprAST*)params[0];
			IdExprAST* valueExpr = (IdExprAST*)params[1];
			PawsStringMap* map = (PawsStringMap*)codegenExpr(params[2], parentBlock).value;
			BlockExprAST* body = (BlockExprAST*)params[3];
			PawsString key, value;
			defineSymbol(body, getIdExprASTName(keyExpr), PawsString::TYPE, &key);
			defineSymbol(body, getIdExprASTName(valueExpr), PawsString::TYPE, &value);
			for (const std::pair<const PawsString*, PawsString*>& pair: map->val)
			{
				key.val = pair.first->val;
				value.val = pair.second->val;
				codegenExpr((ExprAST*)body, parentBlock);
			}
		}
	);

	//TODO: Create function call expression instead
	defineExpr2(block, "str($E<PawsMetaType>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return Variable(PawsString::TYPE, new PawsString(getTypeName(((PawsMetaType*)codegenExpr(exprAST, parentBlock).value)->val)));
		},
		PawsString::TYPE
	);
	defineExpr2(block, "str($E<PawsInt>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* value = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			return Variable(PawsString::TYPE, new PawsString(std::to_string(value->val)));
		},
		PawsString::TYPE
	);
	defineExpr2(block, "str($E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return codegenExpr(params[0], parentBlock);
		},
		PawsString::TYPE
	);
	defineExpr2(block, "str($E<PawsBlockExprAST>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsBlockExprAST* value = (PawsBlockExprAST*)codegenExpr(params[0], parentBlock).value;
			return Variable(PawsString::TYPE, new PawsString(ExprASTToString((ExprAST*)value->val)));
		},
		PawsString::TYPE
	);
	defineExpr2(block, "str($E<PawsStringMap>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsStringMap* value = (PawsStringMap*)codegenExpr(params[0], parentBlock).value;
			//TODO: Use stringstream instead
			std::string str = "{";
			for (const std::pair<const PawsString*, PawsString*>& pair: value->val)
				str += "TODO ";
			str += "}";
			return Variable(PawsString::TYPE, new PawsString(str));
		},
		PawsString::TYPE
	);
	defineExpr2(block, "print()",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			std::cout << '\n';
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);
	defineExpr2(block, "printerr()",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			std::cerr << '\n';
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);
	defineExpr2(block, "print($E<PawsMetaType>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			std::cout << getTypeName(((PawsMetaType*)codegenExpr(exprAST, parentBlock).value)->val) << '\n';
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);
	defineExpr2(block, "printerr($E<PawsMetaType>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			std::cerr << getTypeName(((PawsMetaType*)codegenExpr(exprAST, parentBlock).value)->val) << '\n';
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);
	defineExpr2(block, "print($E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* value = (PawsString*)codegenExpr(params[0], parentBlock).value;
			std::cout << value->val << '\n';
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);
	defineExpr2(block, "printerr($E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* value = (PawsString*)codegenExpr(params[0], parentBlock).value;
			std::cerr << value->val << '\n';
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);
	defineExpr2(block, "print($E<PawsInt>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* value = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			std::cout << value->val << '\n';
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);
	defineExpr2(block, "printerr($E<PawsInt>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* value = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			std::cerr << value->val << '\n';
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);
	defineExpr2(block, "type($E<PawsBase>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return Variable(PawsMetaType::TYPE, new PawsMetaType(getType(exprAST, parentBlock)));
		},
		PawsMetaType::TYPE
	);

	// defineExpr2(block, "map($E<PawsBase>, ...)",
	// 	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
	// 		std::vector<ExprAST*>& exprs = getExprListASTExpressions((ExprListAST*)params[0]);
	// 		//TODO: Throw CompileError if exprs.size() & 0x1 != 0x0
	// 		std::map<Variable, Variable> map;
	// 		for (size_t i = 0; i < exprs.size(); i += 2)
	// 			map[codegenExpr(exprs[i + 0], parentBlock)] = codegenExpr(exprs[i + 1], parentBlock);
	// 		return Variable(PawsMap::TYPE, new PawsMap(map));
	// 	},
	// 	PawsMap::TYPE
	// );
	defineExpr2(block, "map($E<PawsString>: $E<PawsString>, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			std::vector<ExprAST*>& keys = getExprListASTExpressions((ExprListAST*)params[0]);
			std::vector<ExprAST*>& values = getExprListASTExpressions((ExprListAST*)params[1]);
			std::map<PawsString*, PawsString*> map;
			for (size_t i = 0; i < keys.size(); ++i)
				map[(PawsString*)codegenExpr(keys[i], parentBlock).value] = (PawsString*)codegenExpr(values[i], parentBlock).value;
			return Variable(PawsStringMap::TYPE, new PawsStringMap(map));
		},
		PawsStringMap::TYPE
	);

	defineExpr2(block, "$E<PawsStringMap>.contains($E<PawsBase>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsStringMap* map = (PawsStringMap*)codegenExpr(params[0], parentBlock).value;
			PawsString* key = (PawsString*)codegenExpr(params[1], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(map->val.find(key) != map->val.end()));
		},
		PawsInt::TYPE
	);
	defineExpr2(block, "$E<PawsStringMap>[$E<PawsBase>]",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsStringMap* map = (PawsStringMap*)codegenExpr(params[0], parentBlock).value;
			PawsString* key = (PawsString*)codegenExpr(params[1], parentBlock).value;
			auto pair = map->val.find(key);
			return Variable(PawsString::TYPE, pair == map->val.end() ? nullptr : pair->second);
		},
		PawsString::TYPE
	);

	defineExpr2(block, "parseCFile($E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* a = (PawsString*)codegenExpr(params[0], parentBlock).value;
			BlockExprAST* fileBlock = parseCFile(a->val.c_str());
			return Variable(PawsBlockExprAST::TYPE, new PawsBlockExprAST(fileBlock));
		},
		PawsBlockExprAST::TYPE
	);

	defineExpr2(block, "$E<PawsBlockExprAST>.exprs",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsBlockExprAST* a = (PawsBlockExprAST*)codegenExpr(params[0], parentBlock).value;
			//TODO
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(block, "$E<PawsBlockExprAST>.codegen($E<PawsBlockExprAST>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsBlockExprAST* expr = (PawsBlockExprAST*)codegenExpr(params[0], parentBlock).value;
			PawsBlockExprAST* scope = (PawsBlockExprAST*)codegenExpr(params[1], parentBlock).value;
			codegenExpr((ExprAST*)expr->val, scope->val);
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(block, "initCodeGenerator()",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			init();
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(block, "initBuiltinSymbols()",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			initBuiltinSymbols();
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(block, "defineBuiltinSymbols($E<PawsBlockExprAST>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsBlockExprAST* block = (PawsBlockExprAST*)codegenExpr(params[0], parentBlock).value;
			defineBuiltinSymbols(block->val);
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(block, "createModule($E<PawsString>, $E<PawsBlockExprAST>, $E<PawsInt>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* sourcePath = (PawsString*)codegenExpr(params[0], parentBlock).value;
			PawsBlockExprAST* moduleBlock = (PawsBlockExprAST*)codegenExpr(params[1], parentBlock).value;
			PawsInt* outputDebugSymbols = (PawsInt*)codegenExpr(params[2], parentBlock).value;
			IModule* module = createModule(sourcePath->val, moduleBlock->val, outputDebugSymbols->val);
			return Variable(PawsModule::TYPE, new PawsModule(module));
		},
		PawsModule::TYPE
	);

	defineExpr2(block, "$E<PawsModule>.print($E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsModule* module = (PawsModule*)codegenExpr(params[0], parentBlock).value;
			PawsString* outputPath = (PawsString*)codegenExpr(params[1], parentBlock).value;
			module->val->print(outputPath->val);
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(block, "$E<PawsModule>.print()",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsModule* module = (PawsModule*)codegenExpr(params[0], parentBlock).value;
			module->val->print();
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(block, "$E<PawsModule>.compile($E<PawsString>, $E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsModule* module = (PawsModule*)codegenExpr(params[0], parentBlock).value;
			PawsString* outputPath = (PawsString*)codegenExpr(params[1], parentBlock).value;
			PawsString* errStr = (PawsString*)codegenExpr(params[2], parentBlock).value;
			return Variable(PawsInt::TYPE, new PawsInt(module->val->compile(outputPath->val, errStr->val)));
		},
		PawsInt::TYPE
	);

	defineExpr2(block, "$E<PawsModule>.run()",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsModule* module = (PawsModule*)codegenExpr(params[0], parentBlock).value;
			module->val->run();
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(block, "$E<PawsModule>.finalize()",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsModule* module = (PawsModule*)codegenExpr(params[0], parentBlock).value;
			module->val->finalize();
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(block, "realpath($E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* path = (PawsString*)codegenExpr(params[0], parentBlock).value;
			char realPath[1024];
			realpath(path->val.c_str(), realPath);
			return Variable(PawsString::TYPE, new PawsString(realPath));
		},
		PawsString::TYPE
	);

// defineStmt2(block, "foo $E<PawsMetaType>",
// 	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
// 		int abc = 0;
// 	}
// );
// defineStmt2(block, "foo $E<PawsBase>",
// 	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
// 		int abc = 0;
// 	}
// );
// defineStmt2(block, "foo $E<PawsInt>",
// 	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
// 		int abc = 0;
// 	}
// );

/*defineStmt2(block, "$E<PawsMetaType> $I",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
		int abc = 0;
	}
);
defineExpr2(block, "$E<PawsMetaType>*",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) -> Variable {
		ExprAST* exprAST = params[0];
		if (ExprASTIsCast(exprAST))
			exprAST = getCastExprASTSource((CastExprAST*)exprAST);
		return Variable(PawsMetaType::TYPE, new PawsMetaType(getType(exprAST, parentBlock)));
	},
	PawsMetaType::TYPE
);*/

// std::map<Variable, Variable> foo;
// Variable a = Variable(PawsInt::TYPE, new PawsInt(1));
// Variable b = Variable(PawsInt::TYPE, new PawsInt(2));
// Variable c = Variable(PawsInt::TYPE, new PawsInt(1));
// Variable d = Variable(PawsString::TYPE, new PawsString("1"));
// foo[a] = b;
// foo[d] = c;
// Variable bar1 = foo.at(c);
// Variable bar2 = foo.at(Variable(PawsString::TYPE, new PawsString("1")));


	try
	{
		codegenExpr((ExprAST*)block, nullptr);
	}
	catch (ReturnException err)
	{
		return err.result;
	}
	return 0;
}