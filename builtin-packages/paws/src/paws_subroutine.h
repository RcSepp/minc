#ifndef __PAWS_SUBROUTINE_H
#define __PAWS_SUBROUTINE_H

struct PawsFunc
{
	std::string name;
	PawsType* returnType;
	std::vector<PawsType*> argTypes;
	std::vector<std::string> argNames;
	virtual bool call(MincRuntime& runtime, const std::vector<MincExpr*>& args, const MincSymbol* self=nullptr) const = 0;

	PawsFunc() = default;
	PawsFunc(const std::string& name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames)
		: name(name), returnType(returnType), argTypes(argTypes), argNames(argNames) {}
};
typedef PawsValue<PawsFunc*> PawsFunction;

struct PawsFunctionType : public PawsType
{
private:
	static std::recursive_mutex mutex;
	static std::set<PawsFunctionType> functionTypes;

	PawsFunctionType(PawsType* returnType, const std::vector<PawsType*>& argTypes);

public:
	PawsType* const returnType;
	const std::vector<PawsType*> argTypes;

	static PawsFunctionType* get(const MincBlockExpr* scope, PawsType* returnType, const std::vector<PawsType*>& argTypes);
	MincObject* copy(MincObject* value);
	void copyTo(MincObject* src, MincObject* dest);
	void copyToNew(MincObject* src, MincObject* dest);
	MincObject* alloc();
	MincObject* allocTo(MincObject* memory);
	std::string toString(MincObject* value) const;
};

struct PawsRegularFunc : public PawsFunc
{
	MincBlockExpr* body;
	mutable std::vector<const MincStackSymbol*> args;
	bool call(MincRuntime& runtime, const std::vector<MincExpr*>& args, const MincSymbol* self=nullptr) const;

	PawsRegularFunc() = default;
	PawsRegularFunc(const std::string& name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, MincBlockExpr* body)
		: PawsFunc(name, returnType, argTypes, argNames), body(body) {}
};

typedef bool (*FuncBlock)(MincRuntime& runtime, const std::vector<MincExpr*>& argExprs, void* funcArgs);
struct PawsConstFunc : public PawsFunc
{
	FuncBlock body;
	void* funcArgs;
	bool call(MincRuntime& runtime, const std::vector<MincExpr*>& argExprs, const MincSymbol* self=nullptr) const
	{
		return body(runtime, argExprs, funcArgs);
	}

	PawsConstFunc() = default;
	PawsConstFunc(const std::string& name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, FuncBlock body, void* funcArgs = nullptr)
		: PawsFunc(name, returnType, argTypes, argNames), body(body), funcArgs(funcArgs) {}
};

template<std::size_t...Is, class L>
constexpr auto for_each(std::index_sequence<Is...>, L&& cbk) noexcept(true)
{
	auto foreacher = [&cbk](auto&&...args) {
		((void)(cbk(args)),...);
	};

	return decltype(foreacher)(foreacher)( std::integral_constant<std::size_t, Is>{}... );
}

template<class F, std::size_t...Is, class L>
constexpr auto call_with_args(F func, std::index_sequence<Is...>, L&& cbk) noexcept(true)
{
	auto caller = [&cbk, &func](auto&&...args) {
		return func(cbk(args)...);
	};

	return decltype(caller)(caller)( std::integral_constant<std::size_t, Is>{}... );
}

template<class C, class F, std::size_t...Is, class L>
constexpr auto call_with_args(C* self, F func, std::index_sequence<Is...>, L&& cbk) noexcept(true)
{
	auto caller = [self, &cbk, &func](auto&&...args) {
		return (self->*func)(cbk(args)...);
	};

	return decltype(caller)(caller)( std::integral_constant<std::size_t, Is>{}... );
}

template <typename F>
struct PawsExternFunc : public PawsFunc
{
	PawsExternFunc(F func, const std::string& name="");
};

template <typename R, typename... A>
struct PawsExternFunc<R (*)(A...)> : public PawsFunc
{
	typedef R F(A...);

	F* func;
	PawsExternFunc(F func, const std::string& name="") : func(func)
	{
		this->name = name;
		returnType = PawsValue<R>::TYPE;

		for_each(std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
			typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
			argTypes.push_back(PawsValue<P>::TYPE);
			argNames.push_back("a" + std::to_string(i));
		});
	}

#pragma GCC diagnostic ignored "-Wunused-but-set-variable" // Workaround for gcc bug, where constexpr blocks can cause false positives for
														   // unused variable warnings
	bool call(MincRuntime& runtime, const std::vector<MincExpr*>& args, const MincSymbol* self=nullptr) const
	{
		static MincObject* argValues[sizeof...(A)];

		bool cancel = false;
		for_each(std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
			if (cancel || (cancel = runExpr(args[i], runtime)))
				return;
			argValues[i] = runtime.result.value;
		});
		if (cancel)
			return true;

		if constexpr (std::is_void<R>::value)
		{
			call_with_args(func, std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
				typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
				return ((PawsValue<P>*)argValues[i])->get();
			});
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(
				PawsValue<R>::TYPE,
				new PawsValue<R>(
					call_with_args(func, std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
						typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
						return ((PawsValue<P>*)argValues[i])->get();
					})
				)
			);
		return false;
	}
#pragma GCC diagnostic pop // Restore -Wunused-but-set-variable
};

template <typename R, typename C, typename... A>
struct PawsExternFunc<R (C::*)(A...)> : public PawsFunc
{
	typedef R (C::*F)(A...);

	F func;
	PawsExternFunc(F func, const std::string& name="") : func(func)
	{
		this->name = name;
		returnType = PawsValue<R>::TYPE;

		for_each(std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
			typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
			argTypes.push_back(PawsValue<P>::TYPE);
			argNames.push_back("a" + std::to_string(i));
		});
	}

#pragma GCC diagnostic ignored "-Wunused-but-set-variable" // Workaround for gcc bug, where constexpr blocks can cause false positives for
														   // unused variable warnings
	bool call(MincRuntime& runtime, const std::vector<MincExpr*>& args, const MincSymbol* self=nullptr) const
	{
		static MincObject* argValues[sizeof...(A)];
		PawsValue<C*>* s = (PawsValue<C*>*)self->value;

		bool cancel = false;
		for_each(std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
			if (cancel || (cancel = runExpr(args[i], runtime)))
				return;
			argValues[i] = runtime.result.value;
		});
		if (cancel)
			return true;

		if constexpr (std::is_void<R>::value)
		{
			call_with_args(s->get(), func, std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
				typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
				return ((PawsValue<P>*)argValues[i])->get();
			});
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(
				PawsValue<R>::TYPE,
				new PawsValue<R>(
					call_with_args(s->get(), func, std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
						typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
						return ((PawsValue<P>*)argValues[i])->get();
					})
				)
			);
		return false;
	}
#pragma GCC diagnostic pop // Restore -Wunused-but-set-variable
};

void defineFunction(MincBlockExpr* scope, const char* name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, MincBlockExpr* body);
void defineConstantFunction(MincBlockExpr* scope, const char* name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, FuncBlock body, void* funcArgs = nullptr);
template<class F> void defineExternFunction(MincBlockExpr* scope, const char* name, F func)
{
	PawsFunc* pawsFunc = new PawsExternFunc(func, name);
	defineSymbol(scope, name, PawsFunctionType::get(scope, pawsFunc->returnType, pawsFunc->argTypes), new PawsFunction(pawsFunc));
}

#endif