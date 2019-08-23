#include <string>
#include <cstring>
#include <iostream>
#include "api.h"

BaseType* PAWS_BASE_TYPE = new BaseType();
BaseType* PAWS_META_TYPE = new BaseType();
BaseType* PAWS_STRING_TYPE = new BaseType();
BaseType* PAWS_INT_TYPE = new BaseType();
BaseType* PAWS_VOID_TYPE = new BaseType();

struct PawsType : BaseValue
{
	BaseType* type;
	PawsType(BaseType* type) : type(type) {}
	uint64_t getConstantValue() { return (uint64_t)type; }
};

struct PawsString : BaseValue
{
	std::string val;
	PawsString(std::string val) : val(val) {}
	PawsString(const char* val) : val(val) {}
	PawsString(const char* val, size_t len) : val(val, len) {}
	uint64_t getConstantValue() { return (uint64_t)val.c_str(); }
};

struct PawsInt : BaseValue
{
	int val;
	PawsInt(int val) : val(val) {}
	uint64_t getConstantValue() { return (uint64_t)val; }
};

void PAWRun(BlockExprAST* block)
{
	defineType("PawsBase", PAWS_BASE_TYPE);
	defineSymbol(block, "PawsBase", getBaseType(), new PawsType(PAWS_BASE_TYPE));

	defineType("PawsType", PAWS_META_TYPE);
	defineSymbol(block, "PawsType", PAWS_META_TYPE, new PawsType(PAWS_META_TYPE));
	defineOpaqueCast(block, PAWS_META_TYPE, PAWS_BASE_TYPE);

	defineType("PawsString", PAWS_STRING_TYPE);
	defineSymbol(block, "PawsString", PAWS_META_TYPE, new PawsType(PAWS_STRING_TYPE));
	defineOpaqueCast(block, PAWS_STRING_TYPE, PAWS_BASE_TYPE);

	defineType("PawsInt", PAWS_INT_TYPE);
	defineSymbol(block, "PawsInt", PAWS_META_TYPE, new PawsType(PAWS_INT_TYPE));
	defineOpaqueCast(block, PAWS_INT_TYPE, PAWS_BASE_TYPE);

	defineType("PawsVoid", PAWS_VOID_TYPE);
	defineSymbol(block, "PawsVoid", PAWS_META_TYPE, new PawsType(PAWS_VOID_TYPE));
	defineOpaqueCast(block, PAWS_VOID_TYPE, PAWS_BASE_TYPE);

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
				return Variable(PAWS_STRING_TYPE, new PawsString(value + 1, strlen(value) - 2));
			
			int intValue;
			if (value[0] == '0' && value[1] == 'x')
				intValue = std::stoi(value, 0, 16);
			else
				intValue = std::stoi(value, 0, 10);
			return Variable(PAWS_INT_TYPE, new PawsInt(intValue));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);
			return value[0] == '"' || value[0] == '\'' ? PAWS_STRING_TYPE : PAWS_INT_TYPE;
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
			return Variable(PAWS_STRING_TYPE, new PawsString(a->val + b->val));
		},
		PAWS_STRING_TYPE
	);

	// Define integer addition
	defineExpr2(block, "$E<PawsInt> + $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* a = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			PawsInt* b = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(PAWS_INT_TYPE, new PawsInt(a->val + b->val));
		},
		PAWS_INT_TYPE
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

	//TODO: Create function call expression instead
	defineExpr2(block, "str($E<PawsType>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return Variable(PAWS_STRING_TYPE, new PawsString(getTypeName(((PawsType*)codegenExpr(exprAST, parentBlock).value)->type)));
		},
		PAWS_STRING_TYPE
	);
	defineExpr2(block, "str($E<PawsInt>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* value = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			return Variable(PAWS_STRING_TYPE, new PawsString(std::to_string(value->val)));
		},
		PAWS_STRING_TYPE
	);
	defineExpr2(block, "str($E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return codegenExpr(params[0], parentBlock);
		},
		PAWS_STRING_TYPE
	);
	defineExpr2(block, "print($E<PawsType>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			std::cout << getTypeName(((PawsType*)codegenExpr(exprAST, parentBlock).value)->type) << '\n';
			return Variable(PAWS_VOID_TYPE, nullptr);
		},
		PAWS_VOID_TYPE
	);
	defineExpr2(block, "print($E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsString* value = (PawsString*)codegenExpr(params[0], parentBlock).value;
			std::cout << value->val << '\n';
			return Variable(PAWS_VOID_TYPE, nullptr);
		},
		PAWS_VOID_TYPE
	);
	defineExpr2(block, "print($E<PawsInt>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsInt* value = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			std::cout << value->val << '\n';
			return Variable(PAWS_VOID_TYPE, nullptr);
		},
		PAWS_VOID_TYPE
	);
	defineExpr2(block, "type($E<PawsBase>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return Variable(PAWS_META_TYPE, new PawsType(getType(exprAST, parentBlock)));
		},
		PAWS_META_TYPE
	);

// defineStmt2(block, "foo $E<PawsType>",
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

/*defineStmt2(block, "$E<PawsType> $I",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
		int abc = 0;
	}
);
defineExpr2(block, "$E<PawsType>*",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) -> Variable {
		ExprAST* exprAST = params[0];
		if (ExprASTIsCast(exprAST))
			exprAST = getCastExprASTSource((CastExprAST*)exprAST);
		return Variable(PAWS_META_TYPE, new PawsType(getType(exprAST, parentBlock)));
	},
	PAWS_META_TYPE
);*/

	codegenExpr((ExprAST*)block, nullptr);
}