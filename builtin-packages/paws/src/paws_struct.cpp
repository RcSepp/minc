#include <vector>
#include <map>
#include <cassert>
#include "minc_api.hpp"
#include "paws_types.h"
#include "paws_struct.h"
#include "minc_pkgmgr.h"

static struct {} STRUCT_ID;

extern MincBlockExpr* pawsSubroutineScope;

Struct* getStruct(const MincBlockExpr* scope)
{
	assert(scope->userType == &STRUCT_ID);
	return (Struct*)scope->user;
}

MincObject* Struct::copy(MincObject* value)
{
	return value; //TODO: This passes structs by reference. Think of how to handle struct assignment (by value, by reference, via reference counting, ...)
}

void Struct::copyTo(MincObject* src, MincObject* dest)
{
	PawsStructInstance::TYPE->copyTo(src, dest);
}

void Struct::copyToNew(MincObject* src, MincObject* dest)
{
	PawsStructInstance::TYPE->copyToNew(src, dest);
}

MincObject* Struct::alloc()
{
	return new PawsStructInstance(nullptr);
}
MincObject* Struct::allocTo(MincObject* memory)
{
	return new(memory) PawsStructInstance(nullptr);
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
		{
			MincObject* value = (MincObject*)(instance->heapFrame.heapPointer + var.second.symbol->location);
			str += var.first + '=' + ((PawsType*)var.second.symbol->type)->toString(value) + ", ";
		}
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

void defineStruct(MincBlockExpr* scope, const std::string& name, Struct* strct)
{
	strct->name = name;
	scope->defineSymbol(name, PawsTpltType::get(scope, Struct::TYPE, strct), strct);
	scope->defineCast(new InheritanceCast(strct, PawsStructInstance::TYPE, new MincOpaqueCastKernel(PawsStructInstance::TYPE)));
	scope->defineCast(new InheritanceCast(PawsTpltType::get(scope, Struct::TYPE, strct), PawsType::TYPE, new MincOpaqueCastKernel(PawsType::TYPE)));
	class CastFromNullKernel : public MincKernel
	{
		Struct* const strct;
	public:
		CastFromNullKernel(Struct* strct) : strct(strct) {}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			runtime.result = new PawsStructInstance(nullptr);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return strct;
		}
	};
	scope->defineCast(new InheritanceCast(PawsNull::TYPE, strct, new CastFromNullKernel(strct)));
}

void defineStructInstance(MincBlockExpr* scope, const std::string& name, Struct* strct, StructInstance* instance)
{
	scope->defineSymbol(name, strct, new PawsStructInstance(instance));
}

MincPackage PAWS_STRUCT("paws.struct", [](MincBlockExpr* pkgScope) {
	registerType<Struct>(pkgScope, "PawsStruct");
	pkgScope->defineCast(new InheritanceCast(Struct::TYPE, PawsType::TYPE, new MincOpaqueCastKernel(PawsType::TYPE)));
	registerType<PawsStructInstance>(pkgScope, "PawsStructInstance");

	// Define struct
	class StructDefinitionKernel : public MincKernel
	{
		const bool hasBase;
	public:
		StructDefinitionKernel(bool hasBase) : hasBase(hasBase) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			// Create struct
			const std::string& structName = ((MincIdExpr*)params[0])->name;

			// A struct with null-body is considered a forward declarated struct
			// If structName refers to a forward declarated struct, define the new struct within the forward declarated struct to preserve its type
			// Otherwise, create a new struct
			Struct* strct;
			const MincSymbol* strctSymbol = buildtime.parentBlock->lookupSymbol(structName);
			if (strctSymbol == nullptr ||
				strctSymbol->value == nullptr ||
				!buildtime.parentBlock->isInstance(strctSymbol->value, PawsStructInstance::TYPE) ||
				(strct = (Struct*)strctSymbol->value)->body != nullptr)
			{
				strct = new Struct();
				defineStruct(buildtime.parentBlock, structName, strct);
			}

			strct->body = (MincBlockExpr*)params[1 + hasBase];
			strct->body->parent = buildtime.parentBlock;
			strct->body->isResumable = true;

			if (hasBase)
			{
				strct->base = (Struct*)((PawsTpltType*)((MincCastExpr*)params[1])->getSourceExpr()->getType(buildtime.parentBlock))->tpltType;
				strct->size += strct->base->size; // Derived struct size includes base struct size
				strct->body->references.push_back(strct->base->body);
				class CastToBaseKernel : public MincKernel
				{
					Struct* const base;
				public:
					CastToBaseKernel(Struct* base) : base(base) {}

					MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
					{
						params[0]->build(buildtime);
						buildtime.result.type = base;
						return this;
					}

					bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
					{
						if (params[0]->run(runtime))
							return true;
						StructInstance* instance = ((PawsStructInstance*)runtime.result)->get();
						StructInstance* baseInstance = instance->base;
						runtime.result = new PawsStructInstance(baseInstance);
						return false;
					}

					MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
					{
						return base;
					}
				};
				buildtime.parentBlock->defineCast(new InheritanceCast(strct, strct->base, new CastToBaseKernel(strct->base)));
			}
			else
				strct->base = nullptr;

			// Create struct definition scope
			MincBlockExpr* structDefScope = (MincBlockExpr*)strct->body->clone();
			structDefScope->parent = buildtime.parentBlock;
			//TODO: Think of a safer way to implement this
			//		Failure scenario:
			//		PawsVoid f() { struct s {}; s(); }
			//		f(); // Struct body is created in this instance
			//		f(); // Struct body is still old instance
			structDefScope->user = strct;
			structDefScope->userType = &STRUCT_ID;

			// Allocate "this" variable in struct scope
			strct->thisSymbol = strct->body->allocStackSymbol("this", strct, PawsStructInstance::TYPE->size);

			// Define member variable definition
			struct VariableDefinitionKernel : public MincKernel
			{
				MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
				{
					Struct* strct = getStruct(buildtime.parentBlock);
					const std::string& name = ((MincIdExpr*)params[0])->name;
					MincExpr* exprAST = params[1];
					if (exprAST->exprtype == MincExpr::ExprType::CAST)
						exprAST = ((MincCastExpr*)exprAST)->getSourceExpr();

					if (strct->getMethod(name) != nullptr)
						throw CompileError(buildtime.parentBlock, params[0]->loc, "redeclaration of %S::%S", strct->name, name);
					Struct* oldVarStrct;
					Struct::MincSymbol* oldVar = strct->getVariable(name, &oldVarStrct);
					if (oldVar != nullptr)
					{
						if (oldVarStrct == strct)
							throw CompileError(buildtime.parentBlock, params[0]->loc, "redeclaration of %S::%S", strct->name, name);
						throw CompileError(buildtime.parentBlock, params[0]->loc, "overwriting inherited variables not implemented");
						// TODO: Implement variable overloading: Store initExpr for inherited variables, but do not redefine symbol
					}

					// Allocate member variable in struct scope
					PawsType* type = (PawsType*)exprAST->getType(buildtime.parentBlock);
					const MincStackSymbol* symbol = strct->body->allocStackSymbol(name, type, type->size);
					strct->variables[name] = Struct::MincSymbol{symbol, exprAST};
					strct->size += type->size;
					return this;
				}

				bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
				{
					return false;
				}

				MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
				{
					return getVoid().type;
				}
			};
			structDefScope->defineStmt(MincBlockExpr::parseCTplt("$I = $E<PawsBase>"), new VariableDefinitionKernel());

			// Define method definition
			struct MethodDefinitionKernel : public MincKernel
			{
				MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
				{
					Struct* strct = getStruct(buildtime.parentBlock);
					PawsType* returnType = (PawsType*)params[0]->build(buildtime).value;
					const std::string& name = ((MincIdExpr*)params[1])->name;
					const std::vector<MincExpr*>& argTypeExprs = ((MincListExpr*)params[2])->exprs;
					const std::vector<MincExpr*>& argNameExprs = ((MincListExpr*)params[3])->exprs;
					MincBlockExpr* block = (MincBlockExpr*)params[4];

					Struct* oldMethodStrct;
					strct->getMethod(name, &oldMethodStrct);
					if (oldMethodStrct == strct || strct->getVariable(name) != nullptr)
						throw CompileError(buildtime.parentBlock, params[0]->loc, "redeclaration of %S::%S", strct->name, name);

					// Set method parent to method definition scope
					block->parent = strct->body;

					// Define return statement in method scope
					definePawsReturnStmt(block, returnType);

					PawsRegularFunc* method = new PawsRegularFunc();
					method->name = name;
					method->returnType = returnType;
					method->argTypes.reserve(argTypeExprs.size());
					for (MincExpr* argTypeExpr: argTypeExprs)
						method->argTypes.push_back((PawsType*)argTypeExpr->build(buildtime).value);
					method->argNames.reserve(argNameExprs.size());
					for (MincExpr* argNameExpr: argNameExprs)
						method->argNames.push_back(((MincIdExpr*)argNameExpr)->name);
					method->body = block;

					// Define arguments in method scope
					method->args.reserve(method->argTypes.size());
					for (size_t i = 0; i < method->argTypes.size(); ++i)
						method->args.push_back(block->allocStackSymbol(method->argNames[i], method->argTypes[i], method->argTypes[i]->size));

					// Define method in struct scope
					strct->methods.insert(std::make_pair(name, method));
					PawsType* methodType = PawsFunctionType::get(pawsSubroutineScope, returnType, method->argTypes);
					PawsFunction* methodValue = new PawsFunction(method);
					strct->body->defineSymbol(name, methodType, methodValue);

					// Name method block
					block->name = methodType->toString(methodValue);
					return this;
				}

				bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
				{
					return false;
				}

				MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
				{
					return getVoid().type;
				}
			};
			structDefScope->defineStmt(MincBlockExpr::parseCTplt("$E<PawsType> $I($E<PawsType> $I, ...) $B"), new MethodDefinitionKernel());

			// Define constructor definition
			struct ConstructorDefinitionKernel : public MincKernel
			{
				MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
				{
					Struct* strct = getStruct(buildtime.parentBlock);
					const std::string& name = ((MincIdExpr*)params[0])->name;
					const std::vector<MincExpr*>& argTypeExprs = ((MincListExpr*)params[1])->exprs;
					if (name != strct->name)
					{
						std::string argTypeStr;
						if (argTypeExprs.size())
						{
							argTypeStr = buildtime.parentBlock->lookupSymbolName(argTypeExprs[0]->build(buildtime).value, "UNKNOWN_TYPE");
							for (size_t i = 1; i != argTypeExprs.size(); ++i)
								argTypeStr += ", " + buildtime.parentBlock->lookupSymbolName(argTypeExprs[i]->build(buildtime).value, "UNKNOWN_TYPE");
						}
						throw CompileError(buildtime.parentBlock, params[0]->loc, "cannot declare non-constructor method %S(%S) without a return type", name, argTypeStr);
					}
					const std::vector<MincExpr*>& argNameExprs = ((MincListExpr*)params[2])->exprs;
					MincBlockExpr* block = (MincBlockExpr*)params[3];

					// Set constructor parent to constructor definition scope
					block->parent = strct->body;

					// Define return statement in constructor scope
					definePawsReturnStmt(block, PawsVoid::TYPE);

					PawsRegularFunc* constructor = new PawsRegularFunc();
					constructor->name = name;
					strct->constructors.push_back(constructor);
					constructor->returnType = PawsVoid::TYPE;
					constructor->argTypes.reserve(argTypeExprs.size());
					for (MincExpr* argTypeExpr: argTypeExprs)
						constructor->argTypes.push_back((PawsType*)argTypeExpr->build(buildtime).value);
					constructor->argNames.reserve(argNameExprs.size());
					for (MincExpr* argNameExpr: argNameExprs)
						constructor->argNames.push_back(((MincIdExpr*)argNameExpr)->name);
					constructor->body = block;

					// Define arguments in constructor scope
					constructor->args.reserve(constructor->argTypes.size());
					for (size_t i = 0; i < constructor->argTypes.size(); ++i)
						constructor->args.push_back(block->allocStackSymbol(constructor->argNames[i], constructor->argTypes[i], constructor->argTypes[i]->size));

					// Name constructor block
					std::string signature(name);
					signature += '(';
					if (constructor->argTypes.size())
					{
						signature += constructor->argTypes[0]->name + ' ' + constructor->argNames[0];
						for (size_t i = 1; i != constructor->argTypes.size(); ++i)
							signature += ", " + constructor->argTypes[i]->name + ' ' + constructor->argNames[i];
					}
					signature += ')';
					block->name = signature;
					return this;
				}

				bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
				{
					return false;
				}

				MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
				{
					return getVoid().type;
				}
			};
			structDefScope->defineStmt(MincBlockExpr::parseCTplt("$I($E<PawsType> $I, ...) $B"), new ConstructorDefinitionKernel());

			// Disallow any other statements in struct body
			struct DefaultKernel : public MincKernel
			{
				MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
				{
					throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "Invalid command in struct context");
				} // LCOV_EXCL_LINE

				bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
				{
					return false;
				}

				MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
				{
					return getVoid().type;
				}
			};
			structDefScope->defineDefaultStmt(new DefaultKernel());

			// Define struct members (variables, methods and constructors)
			structDefScope->build(buildtime);

			// Build constructors
			buildtime.parentBlock = strct->body;
			for (PawsFunc* constructor: strct->constructors)
				((PawsRegularFunc*)constructor)->body->build(buildtime);

			// Build methods
			for (Struct* base = strct; base != nullptr; base = base->base) // Also build overloaded methods
				for (const std::pair<std::string, PawsFunc*>& method: base->methods)
				{
					buildtime.parentBlock = ((PawsRegularFunc*)method.second)->body->parent;
					((PawsRegularFunc*)method.second)->body->build(buildtime);
				}

			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("struct $I $B"), new StructDefinitionKernel(false));
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("struct $I: $E<PawsStruct> $B"), new StructDefinitionKernel(true));

	// Define struct forward declaration
	struct StructForwardDeclarationKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			// Create struct with null-body
			const std::string& structName = ((MincIdExpr*)params[0])->name;
			Struct* strct = new Struct();
			defineStruct(buildtime.parentBlock, structName, strct);
			strct->body = nullptr;
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("struct $I"), new StructForwardDeclarationKernel());

	// Define struct constructor
	class StructConstructorKernel : public MincKernel
	{
		PawsFunc* const constructor;
		Struct* const strct;
	public:
		StructConstructorKernel(PawsFunc* constructor=nullptr, Struct* strct=nullptr) : constructor(constructor), strct(strct) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincBlockExpr* parentBlock = buildtime.parentBlock;
			Struct* strct = (Struct*)((PawsTpltType*)((MincCastExpr*)params[0])->getSourceExpr()->getType(parentBlock))->tpltType;
			for (Struct* base = strct; base != nullptr; base = base->base)
			{
				buildtime.parentBlock = base->body;
				for (const std::pair<const std::string, Struct::MincSymbol>& pair: base->variables)
					pair.second.initExpr->build(buildtime);
			}
			buildtime.parentBlock = parentBlock;

			size_t numArgs = ((MincListExpr*)params[1])->exprs.size();
			for (PawsFunc* constructor: strct->constructors)
			{
				// Check number of arguments
				if (constructor->argTypes.size() != numArgs)
					continue;

				std::vector<MincExpr*> argExprs = ((MincListExpr*)params[1])->exprs;

				// Check argument types and perform inherent type casts
				bool valid = true;
				for (size_t i = 0; i < numArgs; ++i)
				{
					MincExpr* argExpr = argExprs[i];
					MincObject *expectedType = constructor->argTypes[i], *gotType = argExpr->getType(parentBlock);

					if (expectedType != gotType)
					{
						const MincCast* cast = buildtime.parentBlock->lookupCast(gotType, expectedType);
						if (cast == nullptr)
						{
							valid = false;
							break;
						}
						(argExprs[i] = new MincCastExpr(cast, argExpr))->build(buildtime);
					}
				}
				if (valid)
				{
					std::vector<MincExpr*>& actualArgExprs = ((MincListExpr*)params[1])->exprs;
					for (size_t i = 0; i < numArgs; ++i)
						(actualArgExprs[i] = argExprs[i])->build(buildtime);
					return new StructConstructorKernel(constructor, strct);
				}
			}
			if (numArgs || !strct->constructors.empty())
			{
				std::string argTypeStr;
				if (numArgs)
				{
					std::vector<MincExpr*>& argExprs = ((MincListExpr*)params[1])->exprs;
					argTypeStr = parentBlock->lookupSymbolName(argExprs[0]->getType(parentBlock), "UNKNOWN_TYPE");
					for (size_t i = 1; i != argExprs.size(); ++i)
						argTypeStr += ", " + parentBlock->lookupSymbolName(argExprs[i]->getType(parentBlock), "UNKNOWN_TYPE");
				}
				throw CompileError(parentBlock, params[0]->loc, "no matching constructor for call %S(%S)", strct->name, argTypeStr);
			}

			return new StructConstructorKernel(nullptr, strct);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			const MincBlockExpr* const parentBlock = runtime.parentBlock;
			MincSymbol self(strct, nullptr);
			StructInstance* instance = new StructInstance();
			MincEnteredBlockExpr* entered = nullptr; //TODO: Implement this without context-manager pattern
			if (strct->body != nullptr)
			{
				runtime.heapFrame = &instance->heapFrame; // Assign heap frame
				entered = new MincEnteredBlockExpr(runtime, runtime.parentBlock = strct->body);
				for (const std::pair<const std::string, Struct::MincSymbol>& pair: strct->variables)
				{
					// Assign member variable in struct scope
					if (pair.second.initExpr->run(runtime))
						return true;
					MincObject* var = runtime.getStackSymbol(pair.second.symbol);
					((PawsType*)pair.second.symbol->type)->copyToNew(runtime.result, var);
				}
				if (strct->base != nullptr) //TODO: Create baseInstance within StructInstance
				{
					
					instance->base = new StructInstance();
					runtime.heapFrame = &instance->base->heapFrame; // Assign heap frame
					MincEnteredBlockExpr enteredBase(runtime, runtime.parentBlock = strct->base->body);
					for (const std::pair<const std::string, Struct::MincSymbol>& pair: strct->base->variables)
					{
						// Assign member variable in struct base scope
						if (pair.second.initExpr->run(runtime))
							return true;
						MincObject* var = runtime.getStackSymbol(pair.second.symbol);
						((PawsType*)pair.second.symbol->type)->copyToNew(runtime.result, var);
					}
				}
				runtime.heapFrame = &instance->heapFrame; // Assign heap frame
			}
			else
				strct->body = nullptr;

			if (constructor != nullptr)
			{
				// Call constructor
				runtime.parentBlock = parentBlock;
				std::vector<MincExpr*>& argExprs = ((MincListExpr*)params[1])->exprs;
				if (constructor->returnType == PawsVoid::TYPE)
				{
					self.value = new PawsStructInstance(instance);
					if (strct->body != nullptr)
					{
						// Assign "this" variable in struct scope
						MincObject* thisValue = runtime.getStackSymbol(strct->thisSymbol);
						strct->copyToNew(self.value, thisValue);
					}
					if (constructor->call(runtime, argExprs, &self))
						return true;
				}
				else
				{
					if (constructor->call(runtime, argExprs, nullptr))
						return true;
					self.value = runtime.result;
					if (strct->body != nullptr)
					{
						// Assign "this" variable in struct scope
						MincObject* thisValue = runtime.getStackSymbol(strct->thisSymbol);
						strct->copyToNew(self.value, thisValue);
					}
				}
			}
			else // constructor == nullptr
			{
				self.value = new PawsStructInstance(instance);
				if (strct->body != nullptr)
				{
					// Assign "this" variable in struct scope
					MincObject* thisValue = runtime.getStackSymbol(strct->thisSymbol);
					strct->copyToNew(self.value, thisValue);
				}
			}
			delete entered;

			runtime.result = self.value;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			assert(params[0]->exprtype == MincExpr::ExprType::CAST);
			return ((PawsTpltType*)((MincCastExpr*)params[0])->getSourceExpr()->getType(parentBlock))->tpltType;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsStruct>($E, ...)")[0], new StructConstructorKernel());

	// Define struct member getter
	class StructMemberGetterKernel : public MincKernel
	{
		Struct* const strct;
		const MincStackSymbol* const member;
	public:
		StructMemberGetterKernel(Struct* strct=nullptr, const MincStackSymbol* member=nullptr) : strct(strct), member(member) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->exprtype != MincExpr::ExprType::CAST)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "cannot access member of non-struct type <%T>", params[0]);
			params[0]->build(buildtime);
			Struct* strct = (Struct*)((MincCastExpr*)params[0])->getSourceExpr()->getType(buildtime.parentBlock);
			const std::string& memberName = ((MincIdExpr*)params[1])->name;

			Struct::MincSymbol* const memberVar = strct->getVariable(memberName);
			if (memberVar == nullptr)
				throw CompileError(buildtime.parentBlock, params[1]->loc, "no member named '%S' in '%S'", memberName, strct->name);

			return new StructMemberGetterKernel(strct, strct->body->lookupStackSymbol(memberName));
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (((MincCastExpr*)params[0])->getSourceExpr()->run(runtime))
				return true;
			StructInstance* instance = ((PawsStructInstance*)runtime.result)->get();
			const std::string& memberName = ((MincIdExpr*)params[1])->name;

			if (instance == nullptr)
				throw CompileError(runtime.parentBlock, params[0]->loc, "trying to access member %S of NULL", memberName);

			runtime.heapFrame = &instance->heapFrame; // Assign heap frame
			MincEnteredBlockExpr entered(runtime, strct->body);

			if (strct->base != nullptr)
			{
				runtime.heapFrame = &instance->base->heapFrame; // Assign heap frame
				MincEnteredBlockExpr entered(runtime, strct->base->body);
				runtime.result = runtime.getStackSymbol(member);
			}
			else
			{
				runtime.result = runtime.getStackSymbol(member);
			}
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			if (params[0]->exprtype != MincExpr::ExprType::CAST)
				return getErrorType();
			Struct* strct = (Struct*)((MincCastExpr*)params[0])->getSourceExpr()->getType(parentBlock);
			const std::string& memberName = ((MincIdExpr*)params[1])->name;

			Struct::MincSymbol* var = strct->getVariable(memberName);
			return var == nullptr ? getErrorType() : var->symbol->type;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsStructInstance>.$I")[0], new StructMemberGetterKernel());

	// Define struct member setter
	class StructMemberSetterKernel : public MincKernel
	{
		Struct* const strct;
		const MincStackSymbol* const member;
	public:
		StructMemberSetterKernel(Struct* strct=nullptr, const MincStackSymbol* member=nullptr) : strct(strct), member(member) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->exprtype != MincExpr::ExprType::CAST)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "cannot access member of non-struct type <%T>", params[0]);
			params[0]->build(buildtime);
			Struct* strct = (Struct*)((MincCastExpr*)params[0])->getSourceExpr()->getType(buildtime.parentBlock);
			std::string memberName = ((MincIdExpr*)params[1])->name;

			Struct::MincSymbol* var = strct->getVariable(memberName);
			if (var == nullptr)
				throw CompileError(buildtime.parentBlock, params[1]->loc, "no member named '%S' in '%S'", memberName, strct->name);

			assert(params[2]->exprtype == MincExpr::ExprType::CAST);
			MincExpr* valueExpr = ((MincCastExpr*)params[2])->getDerivedExpr();
			MincObject *memberType = var->symbol->type, *valueType = valueExpr->getType(buildtime.parentBlock);
			if (memberType != valueType)
			{
				const MincCast* cast = buildtime.parentBlock->lookupCast(valueType, memberType);
				if (cast == nullptr)
					throw CompileError(buildtime.parentBlock, valueExpr->loc, "cannot assign value of type <%t> to variable of type <%t>", valueType, memberType);
				(valueExpr = new MincCastExpr(cast, valueExpr))->build(buildtime);
			}
			(params[2] = valueExpr)->build(buildtime);

			return new StructMemberSetterKernel(strct, strct->body->lookupStackSymbol(memberName));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (((MincCastExpr*)params[0])->getSourceExpr()->run(runtime))
				return true;
			StructInstance* instance = ((PawsStructInstance*)runtime.result)->get();

			if (instance == nullptr)
				throw CompileError(runtime.parentBlock, params[0]->loc, "trying to access member %S of NULL", ((MincIdExpr*)params[1])->name);

			runtime.heapFrame = &instance->heapFrame; // Assign heap frame
			MincEnteredBlockExpr entered(runtime, strct->body);

			//auto pair = strct->variables.find(memberName);
			if (params[2]->run(runtime))
				return true;
			MincObject* const val = runtime.getStackSymbol(member);
			((PawsType*)member->type)->copyTo(runtime.result, val);
			runtime.result = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			if (params[0]->exprtype != MincExpr::ExprType::CAST)
				return getErrorType();
			Struct* strct = (Struct*)((MincCastExpr*)params[0])->getSourceExpr()->getType(parentBlock);
			const std::string& memberName = ((MincIdExpr*)params[1])->name;

			Struct::MincSymbol* var = strct->getVariable(memberName);
			return var == nullptr ? getErrorType() : var->symbol->type;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsStructInstance>.$I = $E<PawsBase>")[0], new StructMemberSetterKernel());

	// Define method call
	class StructMethodCallKernel : public MincKernel
	{
		Struct* const strct;
	public:
		StructMethodCallKernel(Struct* strct=nullptr) : strct(strct) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->exprtype != MincExpr::ExprType::CAST)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "cannot access member of non-struct type <%T>", params[0]);
			params[0]->build(buildtime);
			Struct* strct = (Struct*)((MincCastExpr*)params[0])->getSourceExpr()->getType(buildtime.parentBlock);
			const std::string& methodName = ((MincIdExpr*)params[1])->name;

			for (Struct* base = strct; base != nullptr; base = base->base)
			{
				auto pair = base->methods.find(methodName);
				if (pair == base->methods.end())
					continue;

				const PawsFunc* method = pair->second;
				std::vector<MincExpr*>& argExprs = ((MincListExpr*)params[2])->exprs;

				// Check number of arguments
				if (method->argTypes.size() != argExprs.size())
					throw CompileError(buildtime.parentBlock, params[0]->loc, "invalid number of method arguments");

				// Check argument types and perform inherent type casts
				for (size_t i = 0; i < argExprs.size(); ++i)
				{
					MincExpr* argExpr = argExprs[i];
					MincObject *expectedType = method->argTypes[i], *gotType = argExpr->getType(buildtime.parentBlock);

					if (expectedType != gotType)
					{
						const MincCast* cast = buildtime.parentBlock->lookupCast(gotType, expectedType);
						if (cast == nullptr)
							throw CompileError(buildtime.parentBlock, argExpr->loc, "invalid method argument type: %E<%t>, expected: <%t>", argExpr, gotType, expectedType);
						(argExprs[i] = new MincCastExpr(cast, argExpr))->build(buildtime);
					}
					else
						argExpr->build(buildtime);
				}
				return new StructMethodCallKernel(strct);
			}
			throw CompileError(buildtime.parentBlock, params[1]->loc, "no method named '%S' in '%S'", methodName, strct->name);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (((MincCastExpr*)params[0])->getSourceExpr()->run(runtime))
				return true;
			const MincSymbol self = MincSymbol(strct, runtime.result);
			StructInstance* instance = ((PawsStructInstance*)self.value)->get();
			const std::string& methodName = ((MincIdExpr*)params[1])->name;

			if (instance == nullptr)
				throw CompileError(runtime.parentBlock, params[0]->loc, "trying to access method %S of NULL", methodName);

			StructInstance* instanceBase = instance;
			std::map<std::string, PawsFunc*>::iterator pair;
			std::vector<MincEnteredBlockExpr*> entered; //TODO: Implement this without context-manager pattern
			for (Struct* base = strct; base != nullptr; base = base->base, instanceBase = instanceBase->base)
			{
				runtime.heapFrame = &instanceBase->heapFrame; // Assign heap frame
				entered.push_back(new MincEnteredBlockExpr(runtime, base->body));
			}
			for (Struct* base = strct; base != nullptr; base = base->base)
			{
				if ((pair = base->methods.find(methodName)) != base->methods.end())
				{
					const PawsFunc* method = pair->second;
					std::vector<MincExpr*>& argExprs = ((MincListExpr*)params[2])->exprs;

					// Call method
					bool result = method->call(runtime, argExprs, &self);
					for (auto e: entered)
						delete e;
					return result;
				}
			}
			for (auto e: entered)
				delete e;
			return false; // LCOV_EXCL_LINE
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			if (params[0]->exprtype != MincExpr::ExprType::CAST)
				return getErrorType();
			Struct* strct = (Struct*)((MincCastExpr*)params[0])->getSourceExpr()->getType(parentBlock);
			const std::string& methodName = ((MincIdExpr*)params[1])->name;

			PawsFunc* method = strct->getMethod(methodName);
			return method == nullptr ? getErrorType() : method->returnType;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsStructInstance>.$I($E, ...)")[0], new StructMethodCallKernel());

	struct NonStructMemberGetterKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			throw CompileError(buildtime.parentBlock, params[0]->loc, "cannot access member of non-struct type <%T>", params[0]);
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getErrorType();
		}
	};
	for (auto tplt: {"$E.$I", "$E.$I = $E", "$E.$I($E, ...)"})
		pkgScope->defineExpr(MincBlockExpr::parseCTplt(tplt)[0], new NonStructMemberGetterKernel());

	// Define struct relations
	struct StructEquivalenceKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			StructInstance* const a = ((PawsStructInstance*)runtime.result)->get();
			if (params[1]->run(runtime))
				return true;
			StructInstance* const b = ((PawsStructInstance*)runtime.result)->get();
			runtime.result = new PawsInt(a == b);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsStructInstance> == $E<PawsStructInstance>")[0], new StructEquivalenceKernel());

	struct StructInequivalenceKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			StructInstance* const a = ((PawsStructInstance*)runtime.result)->get();
			if (params[1]->run(runtime))
				return true;
			StructInstance* const b = ((PawsStructInstance*)runtime.result)->get();
			runtime.result = new PawsInt(a != b);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsStructInstance> != $E<PawsStructInstance>")[0], new StructInequivalenceKernel());
});