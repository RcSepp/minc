#include <vector>
#include <map>
#include <cassert>
#include "minc_api.h"
#include "paws_types.h"
#include "paws_struct.h"
#include "minc_pkgmgr.h"

void defineStruct(BlockExprAST* scope, const char* name, Struct* strct)
{
	strct->name = name;
	defineSymbol(scope, name, PawsTpltType::get(scope, Struct::TYPE, strct), strct);
	defineOpaqueInheritanceCast(scope, strct, PawsStructInstance::TYPE);
	defineOpaqueInheritanceCast(scope, PawsTpltType::get(scope, Struct::TYPE, strct), PawsType::TYPE);
}

void defineStructInstance(BlockExprAST* scope, const char* name, Struct* strct, StructInstance* instance)
{
	defineSymbol(scope, name, strct, new PawsStructInstance(instance));
}

MincPackage PAWS_STRUCT("paws.struct", [](BlockExprAST* pkgScope) {
	registerType<Struct>(pkgScope, "PawsStruct");
	defineOpaqueInheritanceCast(pkgScope, Struct::TYPE, PawsType::TYPE);
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
			defineStmt2(block, "$E<PawsType> $I($E<PawsType> $I, ...) $B",
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
					PawsType* returnType = (PawsType*)codegenExpr(params[0], parentBlock).value;
					const char* funcName = getIdExprASTName((IdExprAST*)params[1]);
					const std::vector<ExprAST*>& argTypeExprs = getListExprASTExprs((ListExprAST*)params[2]);
					const std::vector<ExprAST*>& argNameExprs = getListExprASTExprs((ListExprAST*)params[3]);
					BlockExprAST* block = (BlockExprAST*)params[4];

					// Set function parent to function definition scope
					setBlockExprASTParent(block, parentBlock);

					// Define return statement in function scope
					definePawsReturnStmt(block, returnType);

					Struct::Method& method = ((Struct*)stmtArgs)->methods.insert(std::make_pair(funcName, Struct::Method()))->second;
					method.returnType = returnType;
					method.argTypes.reserve(argTypeExprs.size());
					for (ExprAST* argTypeExpr: argTypeExprs)
						method.argTypes.push_back((PawsType*)codegenExpr(argTypeExpr, parentBlock).value);
					method.argNames.reserve(argNameExprs.size());
					for (ExprAST* argNameExpr: argNameExprs)
						method.argNames.push_back(getIdExprASTName((IdExprAST*)argNameExpr));
					method.body = block;

					// Name function block
					std::string funcFullName(funcName);
					funcFullName += '(';
					if (method.argTypes.size())
					{
						funcFullName += method.argTypes[0]->name;
						for (size_t i = 1; i != method.argTypes.size(); ++i)
							funcFullName += ", " + method.argTypes[i]->name;
					}
					funcFullName += ')';
					setBlockExprASTName(block, funcFullName.c_str());
				}, strct
			);

			// Disallow any other statements in struct body
			defineAntiStmt2(block,
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
					raiseCompileError("Invalid command in struct context", (ExprAST*)parentBlock);
				}
			);

			codegenExpr((ExprAST*)block, parentBlock);

			defineStruct(parentBlock, structName, strct);
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
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
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
				raiseCompileError(("no member named '" + memberName + "' in '" + strct->name + "'").c_str(), params[1]);

			return Variable(pair->second.type, instance->variables[memberName]);
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			assert(ExprASTIsCast(params[0]));
			Struct* strct = (Struct*)(getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock));
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto pair = strct->variables.find(memberName);
			return pair == strct->variables.end() ? nullptr : pair->second.type;
		}
	);

	// Define struct member setter
	defineExpr3(pkgScope, "$E<PawsStructInstance>.$I = $E<PawsBase>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const Variable& var = codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			Struct* strct = (Struct*)var.type;
			StructInstance* instance = ((PawsStructInstance*)var.value)->get();
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto pair = strct->variables.find(memberName);
			if (pair == strct->variables.end())
				raiseCompileError(("no member named '" + memberName + "' in '" + strct->name + "'").c_str(), params[1]);

			assert(ExprASTIsCast(params[2]));
			ExprAST* valueExpr = getDerivedExprAST(params[2]);
			MincObject *memberType = pair->second.type, *valueType = getType(valueExpr, parentBlock);
			if (memberType != valueType)
			{
				ExprAST* castExpr = lookupCast(parentBlock, valueExpr, memberType);
				if (castExpr == nullptr)
				{
					std::string candidateReport = reportExprCandidates(parentBlock, valueExpr);
					throw CompileError(
						parentBlock, getLocation(valueExpr), "cannot assign value of type <%t> to variable of type <%t>\n%S",
						valueType, memberType, candidateReport
					);
				}
				valueExpr = castExpr;
			}
			PawsBase* value = (PawsBase*)codegenExpr(valueExpr, parentBlock).value;

			return Variable(pair->second.type, instance->variables[memberName] = value->copy());
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
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
			//StructInstance* instance = ((PawsStructInstance*)var.value)->get(); //TODO: Instance will be required when implementing the `this` parameter
			std::string methodName = getIdExprASTName((IdExprAST*)params[1]);

			auto pair = strct->methods.find(methodName);
			if (pair == strct->methods.end())
				raiseCompileError(("no method named '" + methodName + "' in '" + strct->name + "'").c_str(), params[1]);

			const Struct::Method& method = pair->second;
			std::vector<ExprAST*>& argExprs = getListExprASTExprs((ListExprAST*)params[2]);

			// Check number of arguments
			if (method.argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of method arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				ExprAST* argExpr = argExprs[i];
				MincObject *expectedType = method.argTypes[i], *gotType = getType(argExpr, parentBlock);

				if (expectedType != gotType)
				{
					ExprAST* castExpr = lookupCast(parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
					{
						std::string candidateReport = reportExprCandidates(parentBlock, argExpr);
						throw CompileError(
							parentBlock, getLocation(argExpr), "invalid method argument type: %E<%t>, expected: <%t>\n%S",
							argExpr, gotType, expectedType, candidateReport
						);
					}
					argExprs[i] = castExpr;
				}
			}

			// Call method
			return method.call(parentBlock, argExprs);
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			assert(ExprASTIsCast(params[0]));
			Struct* strct = (Struct*)(getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock));
			std::string methodName = getIdExprASTName((IdExprAST*)params[1]);

			auto pair = strct->methods.find(methodName);
			return pair == strct->methods.end() ? nullptr : pair->second.returnType;
		}
	);
});