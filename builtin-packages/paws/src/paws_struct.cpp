#include <vector>
#include <map>
#include <cassert>
#include "minc_api.h"
#include "paws_types.h"
#include "paws_struct.h"
#include "minc_pkgmgr.h"

static struct {} STRUCT_ID;

Struct* getStruct(const MincBlockExpr* scope)
{
	assert(getBlockExprUserType(scope) == &STRUCT_ID);
	return (Struct*)getBlockExprUser(scope);
}

MincObject* Struct::copy(MincObject* value)
{
	return value; //TODO: This passes structs by reference. Think of how to handle struct assignment (by value, by reference, via reference counting, ...)
}

std::string Struct::toString(MincObject* value) const
{
	StructInstance* instance = ((PawsStructInstance*)value)->get();
	
	if (variables.empty())
		return name + " {}";
	else
	{
		std::string str = name + " { ";
		for (const std::pair<std::string, MincSymbol>& var: variables)
			str += var.first + '=' + var.second.type->toString(lookupSymbol(instance->body, var.first.c_str())->value) + ", ";
		str[str.size() - 2] = ' ';
		str[str.size() - 1] = '}';
		return str;
	}
}

void Struct::inherit(const Struct* base)
{
	methods.insert(base->methods.begin(), base->methods.end());
	variables.insert(base->variables.begin(), base->variables.end());
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
	class StructDefinitionKernel : public MincKernel
	{
		const MincSymbolId varId;
		Struct* const strct;
		PawsType* const structType;
	public:
		StructDefinitionKernel() : varId(MincSymbolId::NONE), strct(nullptr), structType(nullptr) {}
		StructDefinitionKernel(MincSymbolId varId, PawsType* structType, Struct* strct) : varId(varId), strct(strct), structType(structType) {}

		MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			const char* structName = getIdExprName((MincIdExpr*)params[0]);
			MincBlockExpr* block = (MincBlockExpr*)params[1];
			setBlockExprParent(block, parentBlock); // Overwrite parent, because block parent could be an old, deleted function instance
			setBlockExprParent((MincBlockExpr*)params[1], parentBlock);
			//TODO: Think of a safer way to implement this
			//		Failure scenario:
			//		PawsVoid f() { struct s {}; s(); }
			//		f(); // Struct body is created in this instance
			//		f(); // Struct body is still old instance

			Struct* strct = new Struct();
			defineStruct(parentBlock, structName, strct);
			setBlockExprUser(block, strct);
			setBlockExprUserType(block, &STRUCT_ID);
			strct->body = (MincBlockExpr*)params[1];

			// Define "this" variable in struct scope
			defineSymbol(strct->body, "this", strct, nullptr); //TODO: Store self id

			// Define member variable definition
			defineStmt5(block, "$I = $E<PawsBase>",
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					Struct* strct = getStruct(parentBlock);
					MincExpr *varAST = params[0], *exprAST = params[1];
					if (ExprIsCast(exprAST))
						exprAST = getCastExprSource((MincCastExpr*)exprAST);

					PawsType* type = (PawsType*)::getType(exprAST, parentBlock);
					strct->variables[getIdExprName((MincIdExpr*)varAST)] = Struct::MincSymbol{type, exprAST};
					strct->size += type->size;

					// Define member variable in struct scope
					defineSymbol(strct->body, getIdExprName((MincIdExpr*)varAST), type, nullptr);
				}
			);

			// Define method definition
			defineStmt5(block, "$E<PawsType> $I($E<PawsType> $I, ...) $B",
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					Struct* strct = getStruct(parentBlock);
					buildExpr(params[0], parentBlock);
					PawsType* returnType = (PawsType*)codegenExpr(params[0], parentBlock).value;
					const char* name = getIdExprName((MincIdExpr*)params[1]);
					const std::vector<MincExpr*>& argTypeExprs = getListExprExprs((MincListExpr*)params[2]);
					const std::vector<MincExpr*>& argNameExprs = getListExprExprs((MincListExpr*)params[3]);
					MincBlockExpr* block = (MincBlockExpr*)params[4];

					// Set method parent to method definition scope
					setBlockExprParent(block, strct->body);

					// Define return statement in method scope
					definePawsReturnStmt(block, returnType);

					PawsRegularFunc* method = new PawsRegularFunc();
					strct->methods.insert(std::make_pair(name, method));
					method->returnType = returnType;
					method->argTypes.reserve(argTypeExprs.size());
					for (MincExpr* argTypeExpr: argTypeExprs)
					{
						buildExpr(argTypeExpr, parentBlock);
						method->argTypes.push_back((PawsType*)codegenExpr(argTypeExpr, parentBlock).value);
					}
					method->argNames.reserve(argNameExprs.size());
					for (MincExpr* argNameExpr: argNameExprs)
						method->argNames.push_back(getIdExprName((MincIdExpr*)argNameExpr));
					method->body = block;

					// Define arguments in method scope
					method->args.reserve(method->argTypes.size());
					for (size_t i = 0; i < method->argTypes.size(); ++i)
					{
						defineSymbol(block, method->argNames[i].c_str(), method->argTypes[i], nullptr);
						method->args.push_back(lookupSymbolId(block, method->argNames[i].c_str()));
					}

					// Name method block
					std::string signature(name);
					signature += '(';
					if (method->argTypes.size())
					{
						signature += method->argTypes[0]->name;
						for (size_t i = 1; i != method->argTypes.size(); ++i)
							signature += ", " + method->argTypes[i]->name;
					}
					signature += ')';
					setBlockExprName(block, signature.c_str());

					// Define method in struct scope
					PawsType* methodType = PawsTpltType::get(strct->body, PawsFunction::TYPE, returnType);
					defineSymbol(strct->body, name, methodType, new PawsFunction(method));
				}
			);

			// Define constructor definition
			defineStmt5(block, "$I($E<PawsType> $I, ...) $B",
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					Struct* strct = getStruct(parentBlock);
					const char* name = getIdExprName((MincIdExpr*)params[0]);
					const std::vector<MincExpr*>& argTypeExprs = getListExprExprs((MincListExpr*)params[1]);
					if (name != strct->name)
					{
						std::string argTypeStr;
						if (argTypeExprs.size())
						{
							buildExpr(argTypeExprs[0], parentBlock);
							argTypeStr = lookupSymbolName2(parentBlock, codegenExpr(argTypeExprs[0], parentBlock).value, "UNKNOWN_TYPE");
							for (size_t i = 1; i != argTypeExprs.size(); ++i)
							{
								buildExpr(argTypeExprs[i], parentBlock);
								argTypeStr += ", " + lookupSymbolName2(parentBlock, codegenExpr(argTypeExprs[i], parentBlock).value, "UNKNOWN_TYPE");
							}
						}
						throw CompileError(parentBlock, getLocation(params[0]), "cannot declare non-constructor method %s(%S) without a return type", name, argTypeStr);
					}
					const std::vector<MincExpr*>& argNameExprs = getListExprExprs((MincListExpr*)params[2]);
					MincBlockExpr* block = (MincBlockExpr*)params[3];

					// Set function parent to function definition scope
					setBlockExprParent(block, strct->body);

					// Define return statement in constructor scope
					definePawsReturnStmt(block, PawsVoid::TYPE);

					PawsRegularFunc* constructor = new PawsRegularFunc();
					strct->constructors.push_back(constructor);
					constructor->returnType = PawsVoid::TYPE;
					constructor->argTypes.reserve(argTypeExprs.size());
					for (MincExpr* argTypeExpr: argTypeExprs)
					{
						buildExpr(argTypeExpr, parentBlock);
						constructor->argTypes.push_back((PawsType*)codegenExpr(argTypeExpr, parentBlock).value);
					}
					constructor->argNames.reserve(argNameExprs.size());
					for (MincExpr* argNameExpr: argNameExprs)
						constructor->argNames.push_back(getIdExprName((MincIdExpr*)argNameExpr));
					constructor->body = block;

					// Define arguments in constructor scope
					constructor->args.reserve(constructor->argTypes.size());
					for (size_t i = 0; i < constructor->argTypes.size(); ++i)
					{
						defineSymbol(block, constructor->argNames[i].c_str(), constructor->argTypes[i], nullptr);
						constructor->args.push_back(lookupSymbolId(block, constructor->argNames[i].c_str()));
					}

					// Name constructor block
					std::string signature(name);
					signature += '(';
					if (constructor->argTypes.size())
					{
						signature += constructor->argTypes[0]->name;
						for (size_t i = 1; i != constructor->argTypes.size(); ++i)
							signature += ", " + constructor->argTypes[i]->name;
					}
					signature += ')';
					setBlockExprName(block, signature.c_str());
				}
			);

			// Disallow any other statements in struct body
			defineDefaultStmt5(block,
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					raiseCompileError("Invalid command in struct context", (MincExpr*)parentBlock);
				} // LCOV_EXCL_LINE
			);

			// Define struct members (variables, methods and constructors)
			buildExpr((MincExpr*)block, parentBlock);

			defineDefaultStmt5(block, nullptr);

			// Build constructors
			for (PawsFunc* constructor: strct->constructors)
				buildExpr((MincExpr*)((PawsRegularFunc*)constructor)->body, strct->body);

			// Build methods
			for (const std::pair<std::string, PawsFunc*>& method: strct->methods)
				buildExpr((MincExpr*)((PawsRegularFunc*)method.second)->body, strct->body);

			return new StructDefinitionKernel(lookupSymbolId(parentBlock, structName), PawsTpltType::get(parentBlock, Struct::TYPE, strct), strct);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			// Set struct parent to struct definition scope (the parent may have changed during function cloning)
			MincBlockExpr* block = (MincBlockExpr*)params[1];
			setBlockExprParent(block, parentBlock);

			MincSymbol* varFromId = getSymbol(parentBlock, varId);
			varFromId->value = strct;
			varFromId->type = structType;
			return getVoid();
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	defineStmt4(pkgScope, "struct $I $B", new StructDefinitionKernel());

	// Define struct constructor
	class StructConstructorKernel : public MincKernel
	{
		PawsFunc* constructor;
	public:
		StructConstructorKernel(PawsFunc* constructor=nullptr) : constructor(constructor) {}

		MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			Struct* strct = (Struct*)((PawsTpltType*)::getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
			for (const std::pair<const std::string, Struct::MincSymbol>& pair: strct->variables)
				buildExpr(pair.second.initExpr, strct->body);

			size_t numArgs = getListExprExprs((MincListExpr*)params[1]).size();
			for (PawsFunc* constructor: strct->constructors)
			{
				// Check number of arguments
				if (constructor->argTypes.size() != numArgs)
					continue;

				std::vector<MincExpr*> argExprs = getListExprExprs((MincListExpr*)params[1]);

				// Check argument types and perform inherent type casts
				bool valid = true;
				for (size_t i = 0; i < numArgs; ++i)
				{
					MincExpr* argExpr = argExprs[i];
					MincObject *expectedType = constructor->argTypes[i], *gotType = ::getType(argExpr, parentBlock);

					if (expectedType != gotType)
					{
						MincExpr* castExpr = lookupCast(parentBlock, argExpr, expectedType);
						if (castExpr == nullptr)
						{
							valid = false;
							break;
						}
						buildExpr(castExpr, parentBlock);
						argExprs[i] = castExpr;
					}
				}
				if (valid)
				{
					std::vector<MincExpr*>& actualArgExprs = getListExprExprs((MincListExpr*)params[1]);
					for (size_t i = 0; i < numArgs; ++i)
						buildExpr(actualArgExprs[i] = argExprs[i], parentBlock);
					return new StructConstructorKernel(constructor);
				}
			}
			if (numArgs || !strct->constructors.empty())
			{
				std::string argTypeStr;
				if (numArgs)
				{
					std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);
					argTypeStr = lookupSymbolName2(parentBlock, ::getType(argExprs[0], parentBlock), "UNKNOWN_TYPE");
					for (size_t i = 1; i != argExprs.size(); ++i)
						argTypeStr += ", " + lookupSymbolName2(parentBlock, ::getType(argExprs[i], parentBlock), "UNKNOWN_TYPE");
				}
				throw CompileError(parentBlock, getLocation(params[0]), "no matching constructor for call %S(%S)", strct->name, argTypeStr);
			}

			return new StructConstructorKernel(nullptr);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			Struct* strct = (Struct*)((PawsTpltType*)::getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
			MincSymbol self(strct, nullptr);
			StructInstance* instance = new StructInstance();
			if (strct->body != nullptr)
			{
				instance->body = cloneBlockExpr(strct->body);
				for (const std::pair<const std::string, Struct::MincSymbol>& pair: strct->variables)
				{
					MincSymbol sym = codegenExpr(pair.second.initExpr, instance->body);
					assert(sym.type == pair.second.type);
					defineSymbol(instance->body, pair.first.c_str(), pair.second.type, sym.value);
				}
				for (const std::pair<std::string, PawsFunc*>& pair: strct->methods)
				{
					// Set method parent to struct instance
					//TODO: Temporary solution. This overwrites method parents of previous struct instances!
					const PawsRegularFunc* pawsMethod = dynamic_cast<const PawsRegularFunc*>(pair.second);
					if (pawsMethod != nullptr)
						setBlockExprParent(pawsMethod->body, instance->body);
				}
			}
			else
				instance->body = nullptr;

			if (constructor != nullptr)
			{
				// Set constructor parent to struct instance
				const PawsRegularFunc* pawsConstructor = dynamic_cast<const PawsRegularFunc*>(constructor);
				if (pawsConstructor != nullptr)
					setBlockExprParent(pawsConstructor->body, instance->body);

				// Call constructor
				std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);
				if (constructor->returnType == PawsVoid::TYPE)
				{
					self.value = new PawsStructInstance(instance);
					if (instance->body != nullptr)
						defineSymbol(instance->body, "this", strct, self.value);
					constructor->call(parentBlock, argExprs, &self);
				}
				else
				{
					self.value = constructor->call(parentBlock, argExprs, nullptr).value;
					if (instance->body != nullptr)
						defineSymbol(instance->body, "this", strct, self.value);
				}
			}
			else // constructor == nullptr
			{
				self.value = new PawsStructInstance(instance);
				if (instance->body != nullptr)
					defineSymbol(instance->body, "this", strct, self.value);
			}

			return self;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			assert(ExprIsCast(params[0]));
			return ((PawsTpltType*)::getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
		}
	};
	defineExpr6(pkgScope, "$E<PawsStruct>($E, ...)", new StructConstructorKernel());

	// Define struct member getter
	defineExpr10(pkgScope, "$E<PawsStructInstance>.$I",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			if (!ExprIsCast(params[0]))
				throw CompileError(parentBlock, getLocation(params[0]), "cannot access member of non-struct type <%T>", params[0]);
			buildExpr(params[0], parentBlock);
			Struct* strct = (Struct*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto pair = strct->variables.find(memberName);
			if (pair == strct->variables.end())
				raiseCompileError(("no member named '" + memberName + "' in '" + strct->name + "'").c_str(), params[1]);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const MincSymbol& var = codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			StructInstance* instance = ((PawsStructInstance*)var.value)->get();
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			return *lookupSymbol(instance->body, memberName.c_str());
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
	defineExpr10(pkgScope, "$E<PawsStructInstance>.$I = $E<PawsBase>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			if (!ExprIsCast(params[0]))
				throw CompileError(parentBlock, getLocation(params[0]), "cannot access member of non-struct type <%T>", params[0]);
			buildExpr(params[0], parentBlock);
			Struct* strct = (Struct*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
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
				buildExpr(castExpr, parentBlock);
				valueExpr = castExpr;
			}
			buildExpr(params[2] = valueExpr, parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const MincSymbol& var = codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			//Struct* strct = (Struct*)var.type;
			StructInstance* instance = ((PawsStructInstance*)var.value)->get();
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			//auto pair = strct->variables.find(memberName); //TODO: Store variable id during build()
			MincSymbol val = codegenExpr(params[2], parentBlock);
			MincSymbol* sym = importSymbol(instance->body, memberName.c_str());
			assert(val.type == sym->type);
			sym->value = ((PawsType*)val.type)->copy((PawsBase*)val.value);
			return *sym;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return getErrorType();
			Struct* strct = (Struct*)(getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock));
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto pair = strct->variables.find(memberName);
			return pair == strct->variables.end() ? getErrorType() : pair->second.type;
		}
	);

	// Define method call
	defineExpr10(pkgScope, "$E<PawsStructInstance>.$I($E, ...)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			if (!ExprIsCast(params[0]))
				throw CompileError(parentBlock, getLocation(params[0]), "cannot access member of non-struct type <%T>", params[0]);
			buildExpr(params[0], parentBlock);
			Struct* strct = (Struct*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			std::string methodName = getIdExprName((MincIdExpr*)params[1]);

			auto pair = strct->methods.find(methodName);
			if (pair == strct->methods.end())
				raiseCompileError(("no method named '" + methodName + "' in '" + strct->name + "'").c_str(), params[1]);

			const PawsFunc* method = pair->second;
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[2]);

			// Check number of arguments
			if (method->argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of method arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				MincExpr* argExpr = argExprs[i];
				MincObject *expectedType = method->argTypes[i], *gotType = getType(argExpr, parentBlock);

				if (expectedType != gotType)
				{
					MincExpr* castExpr = lookupCast(parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
						throw CompileError(parentBlock, getLocation(argExpr), "invalid method argument type: %E<%t>, expected: <%t>", argExpr, gotType, expectedType);
					buildExpr(castExpr, parentBlock);
					argExprs[i] = castExpr;
				}

				buildExpr(argExprs[i], parentBlock);
			}
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const MincSymbol& self = codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			Struct* strct = (Struct*)self.type;
			StructInstance* instance = ((PawsStructInstance*)self.value)->get();
			std::string methodName = getIdExprName((MincIdExpr*)params[1]);

			auto pair = strct->methods.find(methodName);
			const PawsFunc* method = pair->second;
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[2]);

			// Set method parent to struct instance
			const PawsRegularFunc* pawsMethod = dynamic_cast<const PawsRegularFunc*>(method);
			if (pawsMethod != nullptr)
				setBlockExprParent(pawsMethod->body, instance->body);

			// Call method
			return method->call(parentBlock, argExprs, &self);
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return getErrorType();
			Struct* strct = (Struct*)(getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock));
			std::string methodName = getIdExprName((MincIdExpr*)params[1]);

			auto pair = strct->methods.find(methodName);
			return pair == strct->methods.end() ? getErrorType() : pair->second->returnType;
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

	// Define integer relations
	defineExpr9(pkgScope, "$E<PawsStructInstance> == $E<PawsStructInstance>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsInt::TYPE, new PawsInt(
					((PawsStructInstance*)codegenExpr(params[0], parentBlock).value)->get() ==
					((PawsStructInstance*)codegenExpr(params[1], parentBlock).value)->get()
			));
		},
		PawsInt::TYPE
	);
	defineExpr9(pkgScope, "$E<PawsStructInstance> != $E<PawsStructInstance>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsInt::TYPE, new PawsInt(
					((PawsStructInstance*)codegenExpr(params[0], parentBlock).value)->get() !=
					((PawsStructInstance*)codegenExpr(params[1], parentBlock).value)->get()
			));
		},
		PawsInt::TYPE
	);
});