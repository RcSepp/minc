#include <vector>
#include <map>
#include <cassert>
#include "minc_api.h"
#include "paws_types.h"
#include "paws_struct.h"
#include "minc_pkgmgr.h"

static struct {} STRUCT_ID;

extern MincBlockExpr* pawsSubroutineScope;

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

	if (instance == nullptr)
		return "NULL";
	else if (variables.empty())
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

Struct::MincSymbol* Struct::getVariable(const std::string& name, Struct** subStruct)
{
	std::map<std::string, Struct::MincSymbol>::iterator pair;
	for (Struct* base = this; base != nullptr; base = base->base)
		if ((pair = base->variables.find(name)) != base->variables.end())
		{
			if (subStruct != nullptr)
				*subStruct = base;
			return &pair->second;
		}
	return nullptr;
}
PawsFunc* Struct::getMethod(const std::string& name, Struct** subStruct)
{
	std::map<std::string, PawsFunc*>::iterator pair;
	for (Struct* base = this; base != nullptr; base = base->base)
		if ((pair = base->methods.find(name)) != base->methods.end())
		{
			if (subStruct != nullptr)
				*subStruct = base;
			return pair->second;
		}
	return nullptr;
}

void defineStruct(MincBlockExpr* scope, const char* name, Struct* strct)
{
	strct->name = name;
	defineSymbol(scope, name, PawsTpltType::get(scope, Struct::TYPE, strct), strct);
	defineOpaqueInheritanceCast(scope, strct, PawsStructInstance::TYPE);
	defineOpaqueInheritanceCast(scope, PawsTpltType::get(scope, Struct::TYPE, strct), PawsType::TYPE);
	defineInheritanceCast2_2(scope, PawsNull::TYPE, strct,
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* castArgs) -> bool {
			runtime.result = MincSymbol((Struct*)castArgs, new PawsStructInstance(nullptr));
			return false;
		}, strct
	);
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
		const bool hasBase;
		const MincSymbolId varId;
		Struct* const strct;
		PawsType* const structType;
	public:
		StructDefinitionKernel(bool hasBase)
			: hasBase(hasBase), varId(MincSymbolId::NONE), strct(nullptr), structType(nullptr) {}
		StructDefinitionKernel(bool hasBase, MincSymbolId varId, PawsType* structType, Struct* strct)
			: hasBase(hasBase), varId(varId), strct(strct), structType(structType) {}

		MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			// Create struct
			const char* structName = getIdExprName((MincIdExpr*)params[0]);

			// A struct with null-body is considered a forward declarated struct
			// If structName refers to a forward declarated struct, define the new struct within the forward declarated struct to preserve its type
			// Otherwise, create a new struct
			Struct* strct;
			const MincSymbol* strctSymbol = lookupSymbol(parentBlock, structName);
			if (strctSymbol == nullptr ||
				strctSymbol->value == nullptr ||
				!isInstance(parentBlock, strctSymbol->value, PawsStructInstance::TYPE) ||
				(strct = (Struct*)strctSymbol->value)->body != nullptr)
			{
				strct = new Struct();
				defineStruct(parentBlock, structName, strct);
			}

			strct->body = (MincBlockExpr*)params[1 + hasBase];
			setBlockExprParent(strct->body, parentBlock);

			if (hasBase)
			{
				strct->base = (Struct*)((PawsTpltType*)::getType(getCastExprSource((MincCastExpr*)params[1]), parentBlock))->tpltType;
				addBlockExprReference(strct->body, strct->base->body);
				defineInheritanceCast2_2(parentBlock, strct, strct->base,
					[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* castArgs) -> bool {
						if (runExpr2(params[0], runtime))
							return true;
						StructInstance* instance = ((PawsStructInstance*)runtime.result.value)->get();
						StructInstance* baseInstance = new StructInstance(); //TODO: Avoid creating new instance
						baseInstance->body = getBlockExprReferences(instance->body)[0];
						runtime.result = MincSymbol((Struct*)castArgs, new PawsStructInstance(baseInstance));
						return false;
					}, strct->base
				);
			}
			else
				strct->base = nullptr;

			// Create struct definition scope
			MincBlockExpr* structDefScope = cloneBlockExpr(strct->body);
			setBlockExprParent(structDefScope, parentBlock);
			//TODO: Think of a safer way to implement this
			//		Failure scenario:
			//		PawsVoid f() { struct s {}; s(); }
			//		f(); // Struct body is created in this instance
			//		f(); // Struct body is still old instance
			setBlockExprUser(structDefScope, strct);
			setBlockExprUserType(structDefScope, &STRUCT_ID);

			// Define "this" variable in struct scope
			defineSymbol(strct->body, "this", strct, nullptr); //TODO: Store self id

			// Define member variable definition
			defineStmt5(structDefScope, "$I = $E<PawsBase>",
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					Struct* strct = getStruct(parentBlock);
					const std::string name = getIdExprName((MincIdExpr*)params[0]);
					MincExpr* exprAST = params[1];
					if (ExprIsCast(exprAST))
						exprAST = getCastExprSource((MincCastExpr*)exprAST);

					if (strct->getMethod(name) != nullptr)
						throw CompileError(parentBlock, getLocation(params[0]), "redeclaration of %S::%S", strct->name, name);
					Struct* oldVarStrct;
					Struct::MincSymbol* oldVar = strct->getVariable(name, &oldVarStrct);
					if (oldVar != nullptr)
					{
						if (oldVarStrct == strct)
							throw CompileError(parentBlock, getLocation(params[0]), "redeclaration of %S::%S", strct->name, name);
						throw CompileError(parentBlock, getLocation(params[0]), "overwriting inherited variables not implemented");
						// TODO: Implement variable overloading: Store initExpr for inherited variables, but do not redefine symbol
					}

					PawsType* type = (PawsType*)::getType(exprAST, parentBlock);
					strct->variables[name] = Struct::MincSymbol{type, exprAST};
					strct->size += type->size;

					// Define member variable in struct scope
					defineSymbol(strct->body, name.c_str(), type, nullptr);
				}
			);

			// Define method definition
			defineStmt5(structDefScope, "$E<PawsType> $I($E<PawsType> $I, ...) $B",
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					Struct* strct = getStruct(parentBlock);
					buildExpr(params[0], parentBlock);
					PawsType* returnType = (PawsType*)runExpr(params[0], parentBlock).value;
					const char* name = getIdExprName((MincIdExpr*)params[1]);
					const std::vector<MincExpr*>& argTypeExprs = getListExprExprs((MincListExpr*)params[2]);
					const std::vector<MincExpr*>& argNameExprs = getListExprExprs((MincListExpr*)params[3]);
					MincBlockExpr* block = (MincBlockExpr*)params[4];

					Struct* oldMethodStrct;
					strct->getMethod(name, &oldMethodStrct);
					if (oldMethodStrct == strct || strct->getVariable(name) != nullptr)
						throw CompileError(parentBlock, getLocation(params[0]), "redeclaration of %S::%s", strct->name, name);

					// Set method parent to method definition scope
					setBlockExprParent(block, strct->body);

					// Define return statement in method scope
					definePawsReturnStmt(block, returnType);

					PawsRegularFunc* method = new PawsRegularFunc();
					method->returnType = returnType;
					method->argTypes.reserve(argTypeExprs.size());
					for (MincExpr* argTypeExpr: argTypeExprs)
					{
						buildExpr(argTypeExpr, parentBlock);
						method->argTypes.push_back((PawsType*)runExpr(argTypeExpr, parentBlock).value);
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
					strct->methods.insert(std::make_pair(name, method));
					PawsType* methodType = PawsTpltType::get(pawsSubroutineScope, PawsFunction::TYPE, returnType);
					defineSymbol(strct->body, name, methodType, new PawsFunction(method));
				}
			);

			// Define constructor definition
			defineStmt5(structDefScope, "$I($E<PawsType> $I, ...) $B",
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
							argTypeStr = lookupSymbolName2(parentBlock, runExpr(argTypeExprs[0], parentBlock).value, "UNKNOWN_TYPE");
							for (size_t i = 1; i != argTypeExprs.size(); ++i)
							{
								buildExpr(argTypeExprs[i], parentBlock);
								argTypeStr += ", " + lookupSymbolName2(parentBlock, runExpr(argTypeExprs[i], parentBlock).value, "UNKNOWN_TYPE");
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
						constructor->argTypes.push_back((PawsType*)runExpr(argTypeExpr, parentBlock).value);
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
			defineDefaultStmt5(structDefScope,
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					raiseCompileError("Invalid command in struct context", (MincExpr*)parentBlock);
				} // LCOV_EXCL_LINE
			);

			// Define struct members (variables, methods and constructors)
			buildExpr((MincExpr*)structDefScope, parentBlock);

			// Build constructors
			for (PawsFunc* constructor: strct->constructors)
				buildExpr((MincExpr*)((PawsRegularFunc*)constructor)->body, strct->body);

			// Build methods
			for (Struct* base = strct; base != nullptr; base = base->base) // Also build overloaded methods
				for (const std::pair<std::string, PawsFunc*>& method: base->methods)
					buildExpr((MincExpr*)((PawsRegularFunc*)method.second)->body, getBlockExprParent(((PawsRegularFunc*)method.second)->body));

			return new StructDefinitionKernel(hasBase, lookupSymbolId(parentBlock, structName), PawsTpltType::get(parentBlock, Struct::TYPE, strct), strct);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			// Set struct parent to struct definition scope (the parent may have changed during function cloning)
			MincBlockExpr* block = (MincBlockExpr*)params[1 + hasBase];
			setBlockExprParent(block, runtime.parentBlock);

			MincSymbol* varFromId = getSymbol(runtime.parentBlock, varId);
			varFromId->value = strct;
			varFromId->type = structType;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	defineStmt4(pkgScope, "struct $I $B", new StructDefinitionKernel(false));
	defineStmt4(pkgScope, "struct $I: $E<PawsStruct> $B", new StructDefinitionKernel(true));

	// Define struct forward declaration
	defineStmt5(pkgScope, "struct $I",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			// Create struct with null-body
			const char* structName = getIdExprName((MincIdExpr*)params[0]);
			Struct* strct = new Struct();
			defineStruct(parentBlock, structName, strct);
			strct->body = nullptr;
		}
	);

	// Define struct constructor
	class StructConstructorKernel : public MincKernel
	{
		PawsFunc* constructor;
	public:
		StructConstructorKernel(PawsFunc* constructor=nullptr) : constructor(constructor) {}

		MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			Struct* strct = (Struct*)((PawsTpltType*)::getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
			for (Struct* base = strct; base != nullptr; base = base->base)
				for (const std::pair<const std::string, Struct::MincSymbol>& pair: base->variables)
					buildExpr(pair.second.initExpr, base->body);

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

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			MincBlockExpr* const parentBlock = runtime.parentBlock;
			Struct* strct = (Struct*)((PawsTpltType*)::getType(getCastExprSource((MincCastExpr*)params[0]), runtime.parentBlock))->tpltType;
			MincSymbol self(strct, nullptr);
			StructInstance* instance = new StructInstance();
			if (strct->body != nullptr)
			{
				instance->body = cloneBlockExpr(strct->body);
				for (const std::pair<const std::string, Struct::MincSymbol>& pair: strct->variables)
				{
					runtime.parentBlock = instance->body;
					if (runExpr2(pair.second.initExpr, runtime))
						return true;
					assert(runtime.result.type == pair.second.type);
					defineSymbol(instance->body, pair.first.c_str(), pair.second.type, runtime.result.value);
				}
				for (const std::pair<std::string, PawsFunc*>& pair: strct->methods)
				{
					// Set method parent to struct instance
					//TODO: Temporary solution. This overwrites method parents of previous struct instances!
					const PawsRegularFunc* pawsMethod = dynamic_cast<const PawsRegularFunc*>(pair.second);
					if (pawsMethod != nullptr)
						setBlockExprParent(pawsMethod->body, instance->body);
				}
				if (strct->base != nullptr)
				{
					MincBlockExpr* instanceBody = cloneBlockExpr(strct->base->body);
					clearBlockExprReferences(instance->body);
					addBlockExprReference(instance->body, instanceBody);
					for (const std::pair<const std::string, Struct::MincSymbol>& pair: strct->base->variables)
					{
						runtime.parentBlock = instanceBody;
						if (runExpr2(pair.second.initExpr, runtime))
							return true;
						assert(runtime.result.type == pair.second.type);
						defineSymbol(instanceBody, pair.first.c_str(), pair.second.type, runtime.result.value);
					}
					for (const std::pair<std::string, PawsFunc*>& pair: strct->base->methods)
					{
						// Set method parent to struct instance
						//TODO: Temporary solution. This overwrites method parents of previous struct instances!
						const PawsRegularFunc* pawsMethod = dynamic_cast<const PawsRegularFunc*>(pair.second);
						if (pawsMethod != nullptr)
							setBlockExprParent(pawsMethod->body, instanceBody);
					}
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
				runtime.parentBlock = parentBlock;
				std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);
				if (constructor->returnType == PawsVoid::TYPE)
				{
					self.value = new PawsStructInstance(instance);
					if (instance->body != nullptr)
						defineSymbol(instance->body, "this", strct, self.value);
					if (constructor->call(runtime, argExprs, &self))
						return true;
				}
				else
				{
					if (constructor->call(runtime, argExprs, nullptr))
						return true;
					self.value = runtime.result.value;
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

			runtime.result = self;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			assert(ExprIsCast(params[0]));
			return ((PawsTpltType*)::getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
		}
	};
	defineExpr6(pkgScope, "$E<PawsStruct>($E, ...)", new StructConstructorKernel());

	// Define struct member getter
	defineExpr10_2(pkgScope, "$E<PawsStructInstance>.$I",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			if (!ExprIsCast(params[0]))
				throw CompileError(parentBlock, getLocation(params[0]), "cannot access member of non-struct type <%T>", params[0]);
			buildExpr(params[0], parentBlock);
			Struct* strct = (Struct*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			if (strct->getVariable(memberName) == nullptr)
				raiseCompileError(("no member named '" + memberName + "' in '" + strct->name + "'").c_str(), params[1]);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr2(getCastExprSource((MincCastExpr*)params[0]), runtime))
				return true;
			StructInstance* instance = ((PawsStructInstance*)runtime.result.value)->get();
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			if (instance == nullptr)
				throw CompileError(runtime.parentBlock, getLocation(params[0]), "trying to access member %S of NULL", memberName);

			runtime.result = *lookupSymbol(instance->body, memberName.c_str());
			return false;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return getErrorType();
			Struct* strct = (Struct*)(getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock));
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			Struct::MincSymbol* var = strct->getVariable(memberName);
			return var == nullptr ? getErrorType() : var->type;
		}
	);

	// Define struct member setter
	defineExpr10_2(pkgScope, "$E<PawsStructInstance>.$I = $E<PawsBase>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			if (!ExprIsCast(params[0]))
				throw CompileError(parentBlock, getLocation(params[0]), "cannot access member of non-struct type <%T>", params[0]);
			buildExpr(params[0], parentBlock);
			Struct* strct = (Struct*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			Struct::MincSymbol* var = strct->getVariable(memberName);
			if (var == nullptr)
				raiseCompileError(("no member named '" + memberName + "' in '" + strct->name + "'").c_str(), params[1]);

			assert(ExprIsCast(params[2]));
			MincExpr* valueExpr = getDerivedExpr(params[2]);
			MincObject *memberType = var->type, *valueType = getType(valueExpr, parentBlock);
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
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr2(getCastExprSource((MincCastExpr*)params[0]), runtime))
				return true;
			//Struct* strct = (Struct*)runtime.result.type;
			StructInstance* instance = ((PawsStructInstance*)runtime.result.value)->get();
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			if (instance == nullptr)
				throw CompileError(runtime.parentBlock, getLocation(params[0]), "trying to access member %S of NULL", memberName);

			//auto pair = strct->variables.find(memberName); //TODO: Store variable id during build()
			if (runExpr2(params[2], runtime))
				return true;
			MincSymbol* sym = importSymbol(instance->body, memberName.c_str());
			assert(runtime.result.type == sym->type);
			sym->value = runtime.result.value = ((PawsType*)runtime.result.type)->copy((PawsBase*)runtime.result.value);
			return false;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return getErrorType();
			Struct* strct = (Struct*)(getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock));
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			Struct::MincSymbol* var = strct->getVariable(memberName);
			return var == nullptr ? getErrorType() : var->type;
		}
	);

	// Define method call
	defineExpr10_2(pkgScope, "$E<PawsStructInstance>.$I($E, ...)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) {
			if (!ExprIsCast(params[0]))
				throw CompileError(parentBlock, getLocation(params[0]), "cannot access member of non-struct type <%T>", params[0]);
			buildExpr(params[0], parentBlock);
			Struct* strct = (Struct*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			std::string methodName = getIdExprName((MincIdExpr*)params[1]);

			for (Struct* base = strct; base != nullptr; base = base->base)
			{
				auto pair = base->methods.find(methodName);
				if (pair == base->methods.end())
					continue;

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
				return;
			}
			raiseCompileError(("no method named '" + methodName + "' in '" + strct->name + "'").c_str(), params[1]);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr2(getCastExprSource((MincCastExpr*)params[0]), runtime))
				return true;
			const MincSymbol& self = runtime.result;
			Struct* strct = (Struct*)self.type;
			StructInstance* instance = ((PawsStructInstance*)self.value)->get();
			std::string methodName = getIdExprName((MincIdExpr*)params[1]);

			if (instance == nullptr)
				throw CompileError(runtime.parentBlock, getLocation(params[0]), "trying to access method %S of NULL", methodName);

			MincBlockExpr* instanceBody = instance->body;
			std::map<std::string, PawsFunc*>::iterator pair;
			for (Struct* base = strct; base != nullptr; base = base->base, instanceBody = getBlockExprReferences(instanceBody)[0])
			{
				if ((pair = base->methods.find(methodName)) != base->methods.end())
				{
					const PawsFunc* method = pair->second;
					std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[2]);

					// Set method parent to struct instance
					const PawsRegularFunc* pawsMethod = dynamic_cast<const PawsRegularFunc*>(method);
					if (pawsMethod != nullptr)
						setBlockExprParent(pawsMethod->body, instanceBody);

					// Call method
					return method->call(runtime, argExprs, &self);
				}
			}
			return false; // LCOV_EXCL_LINE
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return getErrorType();
			Struct* strct = (Struct*)(getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock));
			std::string methodName = getIdExprName((MincIdExpr*)params[1]);

			PawsFunc* method = strct->getMethod(methodName);
			return method == nullptr ? getErrorType() : method->returnType;
		}
	);

	for (auto tplt: {"$E.$I", "$E.$I = $E", "$E.$I($E, ...)"})
		defineExpr7(pkgScope, tplt,
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
				throw CompileError(parentBlock, getLocation(params[0]), "cannot access member of non-struct type <%T>", params[0]);
			},
			getErrorType()
		);

	// Define integer relations
	defineExpr9_2(pkgScope, "$E<PawsStructInstance> == $E<PawsStructInstance>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr2(params[0], runtime))
				return true;
			StructInstance* const a = ((PawsStructInstance*)runtime.result.value)->get();
			if (runExpr2(params[1], runtime))
				return true;
			StructInstance* const b = ((PawsStructInstance*)runtime.result.value)->get();
			runtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(a == b));
			return false;
		},
		PawsInt::TYPE
	);
	defineExpr9_2(pkgScope, "$E<PawsStructInstance> != $E<PawsStructInstance>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr2(params[0], runtime))
				return true;
			StructInstance* const a = ((PawsStructInstance*)runtime.result.value)->get();
			if (runExpr2(params[1], runtime))
				return true;
			StructInstance* const b = ((PawsStructInstance*)runtime.result.value)->get();
			runtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(a != b));
			return false;
		},
		PawsInt::TYPE
	);
});