#include <vector>
#include <map>
#include <cassert>
#include "minc_api.h"
#include "paws_types.h"
#include "paws_struct.h"
#include "minc_pkgmgr.h"

static struct {} STRUCT_ID;

PawsType* const Struct::TYPE = new PawsType();

Struct* getStruct(const MincBlockExpr* scope)
{
	while (getBlockExprUserType(scope) != &STRUCT_ID)
		scope = getBlockExprParent(scope);
	assert(scope);
	return (Struct*)getBlockExprUser(scope);
}

void defineStruct(MincBlockExpr* scope, const char* name, Struct* strct)
{
	strct->name = name;
	defineSymbol(scope, name, PawsTpltType::get(scope, Struct::TYPE, strct), strct);
	defineOpaqueInheritanceCast(scope, strct, PawsStructInstance::TYPE);
	defineOpaqueInheritanceCast(scope, PawsTpltType::get(scope, Struct::TYPE, strct), PawsType::TYPE);
}

void defineStructInstance(MincBlockExpr* scope, const char* name, Struct* strct, StructInstance* instance)
{
	defineSymbol(scope, name, strct, new PawsStructInstance(instance));
}

MincPackage PAWS_STRUCT("paws.struct", [](MincBlockExpr* pkgScope) {
	registerType<Struct>(pkgScope, "PawsStruct");
	defineOpaqueInheritanceCast(pkgScope, Struct::TYPE, PawsType::TYPE);
	registerType<PawsStructInstance>(pkgScope, "PawsStructInstance");

	// Define struct
	defineStmt2(pkgScope, "struct $I $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			const char* structName = getIdExprName((MincIdExpr*)params[0]);
			MincBlockExpr* block = (MincBlockExpr*)params[1];
			setBlockExprParent(block, parentBlock); // Overwrite parent, because block parent could be an old, deleted function instance
			//TODO: Think of a safer way to implement this
			//		Failure scenario:
			//		PawsVoid f() { struct s {}; s(); }
			//		f(); // Struct body is created in this instance
			//		f(); // Struct body is still old instance

			Struct* strct = new Struct();
			setBlockExprUser(block, strct);
			setBlockExprUserType(block, &STRUCT_ID);

			// Define member variable definition
			defineStmt2(block, "$I = $E<PawsBase>",
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					MincExpr *varAST = params[0], *exprAST = params[1];
					if (ExprIsCast(exprAST))
						exprAST = getCastExprSource((MincCastExpr*)exprAST);

					getStruct(parentBlock)->variables[getIdExprName((MincIdExpr*)varAST)] = Struct::MincSymbol{(PawsType*)getType(exprAST, parentBlock), exprAST};
				}
			);

			// Define method definition
			defineStmt2(block, "$E<PawsType> $I($E<PawsType> $I, ...) $B",
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					PawsType* returnType = (PawsType*)codegenExpr(params[0], parentBlock).value;
					const char* funcName = getIdExprName((MincIdExpr*)params[1]);
					const std::vector<MincExpr*>& argTypeExprs = getListExprExprs((MincListExpr*)params[2]);
					const std::vector<MincExpr*>& argNameExprs = getListExprExprs((MincListExpr*)params[3]);
					MincBlockExpr* block = (MincBlockExpr*)params[4];

					// Set function parent to function definition scope
					setBlockExprParent(block, parentBlock);

					// Define return statement in function scope
					definePawsReturnStmt(block, returnType);

					Struct::Method& method = getStruct(parentBlock)->methods.insert(std::make_pair(funcName, Struct::Method()))->second;
					method.returnType = returnType;
					method.argTypes.reserve(argTypeExprs.size());
					for (MincExpr* argTypeExpr: argTypeExprs)
						method.argTypes.push_back((PawsType*)codegenExpr(argTypeExpr, parentBlock).value);
					method.argNames.reserve(argNameExprs.size());
					for (MincExpr* argNameExpr: argNameExprs)
						method.argNames.push_back(getIdExprName((MincIdExpr*)argNameExpr));
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
					setBlockExprName(block, funcFullName.c_str());
				}
			);

			// Disallow any other statements in struct body
			defineDefaultStmt2(block,
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					raiseCompileError("Invalid command in struct context", (MincExpr*)parentBlock);
				} // LCOV_EXCL_LINE
			);

			codegenExpr((MincExpr*)block, parentBlock);

			defineDefaultStmt2(block, nullptr);

			defineStruct(parentBlock, structName, strct);
		}
	);

	// Define struct constructor
	defineExpr3(pkgScope, "$E<PawsStruct>($E, ...)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			Struct* strct = (Struct*)((PawsTpltType*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
			StructInstance* instance = new StructInstance();
			for (const std::pair<const std::string, Struct::MincSymbol>& pair: strct->variables)
				instance->variables[pair.first] = codegenExpr(pair.second.initExpr, parentBlock).value; //TODO: Replace parentBlock with strct->body->parent
			return MincSymbol(strct, new PawsStructInstance(instance));
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			assert(ExprIsCast(params[0]));
			return ((PawsTpltType*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
		}
	);

	// Define struct member getter
	defineExpr3(pkgScope, "$E<PawsStructInstance>.$I",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			if (!ExprIsCast(params[0]))
				throw CompileError(parentBlock, getLocation(params[0]), "cannot access member of non-struct type <%T>", params[0]);
			const MincSymbol& var = codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			Struct* strct = (Struct*)var.type;
			StructInstance* instance = ((PawsStructInstance*)var.value)->get();
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto pair = strct->variables.find(memberName);
			if (pair == strct->variables.end())
				raiseCompileError(("no member named '" + memberName + "' in '" + strct->name + "'").c_str(), params[1]);

			return MincSymbol(pair->second.type, instance->variables[memberName]);
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return getErrorType();
			Struct* strct = (Struct*)(getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock));
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto pair = strct->variables.find(memberName);
			return pair == strct->variables.end() ? getErrorType() : pair->second.type;
		}
	);

	// Define struct member setter
	defineExpr3(pkgScope, "$E<PawsStructInstance>.$I = $E<PawsBase>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			if (!ExprIsCast(params[0]))
				throw CompileError(parentBlock, getLocation(params[0]), "cannot access member of non-struct type <%T>", params[0]);
			const MincSymbol& var = codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			Struct* strct = (Struct*)var.type;
			StructInstance* instance = ((PawsStructInstance*)var.value)->get();
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto pair = strct->variables.find(memberName);
			if (pair == strct->variables.end())
				raiseCompileError(("no member named '" + memberName + "' in '" + strct->name + "'").c_str(), params[1]);

			assert(ExprIsCast(params[2]));
			MincExpr* valueExpr = getDerivedExpr(params[2]);
			MincObject *memberType = pair->second.type, *valueType = getType(valueExpr, parentBlock);
			if (memberType != valueType)
			{
				MincExpr* castExpr = lookupCast(parentBlock, valueExpr, memberType);
				if (castExpr == nullptr)
					throw CompileError(parentBlock, getLocation(valueExpr), "cannot assign value of type <%t> to variable of type <%t>", valueType, memberType);
				valueExpr = castExpr;
			}
			PawsBase* value = (PawsBase*)codegenExpr(valueExpr, parentBlock).value;

			return MincSymbol(pair->second.type, instance->variables[memberName] = value->copy());
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return getErrorType();
			Struct* strct = (Struct*)(getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock));
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto pair = strct->variables.find(memberName);
			return pair == strct->variables.end() ? nullptr : pair->second.type;
		}
	);

	// Define method call
	defineExpr3(pkgScope, "$E<PawsStructInstance>.$I($E, ...)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			if (!ExprIsCast(params[0]))
				throw CompileError(parentBlock, getLocation(params[0]), "cannot access member of non-struct type <%T>", params[0]);
			const MincSymbol& var = codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			Struct* strct = (Struct*)var.type;
			std::string methodName = getIdExprName((MincIdExpr*)params[1]);

			auto pair = strct->methods.find(methodName);
			if (pair == strct->methods.end())
				raiseCompileError(("no method named '" + methodName + "' in '" + strct->name + "'").c_str(), params[1]);

			const Struct::Method& method = pair->second;
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[2]);

			// Check number of arguments
			if (method.argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of method arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				MincExpr* argExpr = argExprs[i];
				MincObject *expectedType = method.argTypes[i], *gotType = getType(argExpr, parentBlock);

				if (expectedType != gotType)
				{
					MincExpr* castExpr = lookupCast(parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
						throw CompileError(parentBlock, getLocation(argExpr), "invalid method argument type: %E<%t>, expected: <%t>", argExpr, gotType, expectedType);
					argExprs[i] = castExpr;
				}
			}

			// Call method
			return method.call(parentBlock, argExprs, &var);
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return getErrorType();
			Struct* strct = (Struct*)(getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock));
			std::string methodName = getIdExprName((MincIdExpr*)params[1]);

			auto pair = strct->methods.find(methodName);
			return pair == strct->methods.end() ? nullptr : pair->second.returnType;
		}
	);

	for (auto tplt: {"$E.$I", "$E.$I = $E", "$E.$I($E, ...)"})
		defineExpr3(pkgScope, tplt,
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
				throw CompileError(parentBlock, getLocation(params[0]), "cannot access member of non-struct type <%T>", params[0]);
			}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
				return getErrorType();
			}
		);
});