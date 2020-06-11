#include <vector>
#include <map>
#include <cassert>
#include "minc_api.h"
#include "paws_types.h"
#include "paws_struct.h"
#include "minc_pkgmgr.h"

void defineStruct(BlockExprAST* scope, const char* name, Struct* strct)
{
	defineType(name, strct);
	defineOpaqueInheritanceCast(scope, strct, PawsStructInstance::TYPE);
	defineOpaqueInheritanceCast(scope, PawsTpltType::get(PawsStruct::TYPE, strct), PawsMetaType::TYPE);
	defineSymbol(scope, name, PawsTpltType::get(PawsStruct::TYPE, strct), new PawsStruct(strct));
}

void defineStructInstance(BlockExprAST* scope, const char* name, Struct* strct, StructInstance* instance)
{
	defineSymbol(scope, name, strct, new PawsStructInstance(instance));
}

MincPackage PAWS_STRUCT("paws.struct", [](BlockExprAST* pkgScope) {
	registerType<PawsStruct>(pkgScope, "PawsStruct");
	defineOpaqueInheritanceCast(pkgScope, PawsStruct::TYPE, PawsMetaType::TYPE);
	registerType<PawsStructInstance>(pkgScope, "PawsStructInstance");

	// Define struct
	defineStmt2(pkgScope, "struct $I $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			const char* structName = getIdExprASTName((IdExprAST*)params[0]);
			BlockExprAST* block = (BlockExprAST*)params[1];

			Struct* strct = new Struct();

			// Define member variable definition
			defineStmt2(block, "$I = $E<PawsBase>",
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
					ExprAST* exprAST = params[1];
					if (ExprASTIsCast(exprAST))
						exprAST = getCastExprASTSource((CastExprAST*)exprAST);

					ExprAST* varAST = params[0];
					if (ExprASTIsCast(varAST))
						varAST = getCastExprASTSource((CastExprAST*)varAST);

					((Struct*)stmtArgs)->variables[getIdExprASTName((IdExprAST*)varAST)] = Struct::Variable{(PawsType*)getType(exprAST, parentBlock), exprAST};
				}, strct
			);

			// Define method definition
			defineStmt2(block, "$E<PawsMetaType> $I($E<PawsMetaType> $I, ...) $B",
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
					PawsType* returnType = ((PawsMetaType*)codegenExpr(params[0], parentBlock).value)->get();
					const char* funcName = getIdExprASTName((IdExprAST*)params[1]);
					const std::vector<ExprAST*>& argTypeExprs = getExprListASTExprs((ExprListAST*)params[2]);
					const std::vector<ExprAST*>& argNameExprs = getExprListASTExprs((ExprListAST*)params[3]);
					BlockExprAST* block = (BlockExprAST*)params[4];

					// Set function parent to function definition scope
					setBlockExprASTParent(block, parentBlock);

					// Define return statement in function scope
					definePawsReturnStmt(block, returnType);

					Struct::Method& method = ((Struct*)stmtArgs)->methods.insert(std::make_pair(funcName, Struct::Method()))->second;
					method.returnType = returnType;
					method.argTypes.reserve(argTypeExprs.size());
					for (ExprAST* argTypeExpr: argTypeExprs)
						method.argTypes.push_back(((PawsMetaType*)codegenExpr(argTypeExpr, parentBlock).value)->get());
					method.argNames.reserve(argNameExprs.size());
					for (ExprAST* argNameExpr: argNameExprs)
						method.argNames.push_back(getIdExprASTName((IdExprAST*)argNameExpr));
					method.body = block;

					// Name function block
					std::string funcFullName(funcName);
					funcFullName += '(';
					if (method.argTypes.size())
					{
						funcFullName += getTypeName(method.argTypes[0]);
						for (size_t i = 1; i != method.argTypes.size(); ++i)
							funcFullName += getTypeName(method.argTypes[i]) + ", ";
					}
					funcFullName += ')';
					setBlockExprASTName(block, funcFullName);
				}, strct
			);

			// Disallow any other statements in struct body
			defineAntiStmt2(block,
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
					raiseCompileError("Invalid command in struct context", (ExprAST*)parentBlock);
				}
			);

			codegenExpr((ExprAST*)block, parentBlock);

			defineType(structName, strct);
			defineOpaqueInheritanceCast(parentBlock, strct, PawsStructInstance::TYPE);
			defineOpaqueInheritanceCast(parentBlock, PawsTpltType::get(PawsStruct::TYPE, strct), PawsMetaType::TYPE);
			defineSymbol(parentBlock, structName, PawsTpltType::get(PawsStruct::TYPE, strct), new PawsStruct(strct));
		}
	);

	// Define struct constructor
	defineExpr3(pkgScope, "$E<PawsStruct>($E, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Struct* strct = (Struct*)((PawsTpltType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock))->tpltType;
			StructInstance* instance = new StructInstance();
			for (const std::pair<const std::string, Struct::Variable>& pair: strct->variables)
				instance->variables[pair.first] = codegenExpr(pair.second.initExpr, parentBlock).value; //TODO: Replace parentBlock with strct->body->parent
			return Variable(strct, new PawsStructInstance(instance));
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			return ((PawsTpltType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock))->tpltType;
		}
	);

	// Define struct member getter
	defineExpr3(pkgScope, "$E<PawsStructInstance>.$I",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const Variable& var = codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			Struct* strct = (Struct*)var.type;
			StructInstance* instance = ((PawsStructInstance*)var.value)->get();
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto pair = strct->variables.find(memberName);
			if (pair == strct->variables.end())
				raiseCompileError(("no member named '" + memberName + "' in '" + getTypeName(strct) + "'").c_str(), params[1]);

			return Variable(pair->second.type, instance->variables[memberName]);
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			Struct* strct = (Struct*)(getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock));
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto pair = strct->variables.find(memberName);
			return pair == strct->variables.end() ? nullptr : pair->second.type;
		}
	);

	// Define method call
	defineExpr3(pkgScope, "$E<PawsStructInstance>.$I($E, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const Variable& var = codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			Struct* strct = (Struct*)var.type;
			StructInstance* instance = ((PawsStructInstance*)var.value)->get();
			std::string methodName = getIdExprASTName((IdExprAST*)params[1]);

			auto pair = strct->methods.find(methodName);
			if (pair == strct->methods.end())
				raiseCompileError(("no method named '" + methodName + "' in '" + getTypeName(strct) + "'").c_str(), params[1]);

			const Struct::Method& method = pair->second;
			std::vector<ExprAST*>& argExprs = getExprListASTExprs((ExprListAST*)params[2]);

			// Check number of arguments
			if (method.argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of method arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				ExprAST* argExpr = argExprs[i];
				BaseType *expectedType = method.argTypes[i], *gotType = getType(argExpr, parentBlock);

				if (expectedType != gotType)
				{
					ExprAST* castExpr = lookupCast(parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
					{
						std::string candidateReport = reportExprCandidates(parentBlock, argExpr);
						raiseCompileError(
							("invalid method argument type: " + ExprASTToString(argExpr) + "<" + getTypeName(gotType) + ">, expected: <" + getTypeName(expectedType) + ">\n" + candidateReport).c_str(),
							argExpr
						);
					}
					argExprs[i] = castExpr;
				}
			}

			// Call function
			return method.call(parentBlock, argExprs);
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			Struct* strct = (Struct*)(getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock));
			std::string methodName = getIdExprASTName((IdExprAST*)params[1]);

			auto pair = strct->methods.find(methodName);
			return pair == strct->methods.end() ? nullptr : pair->second.returnType;
		}
	);
});