#ifndef __PAWS_SUBROUTINE_H
#define __PAWS_SUBROUTINE_H

struct PawsFunc
{
	PawsType* returnType;
	std::vector<PawsType*> argTypes;
	std::vector<std::string> argNames;
	virtual MincSymbol call(MincBlockExpr* callerScope, const std::vector<MincExpr*>& args, const MincSymbol* self=nullptr) const = 0;

	PawsFunc() = default;
	PawsFunc(PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames)
		: returnType(returnType), argTypes(argTypes), argNames(argNames) {}
};
typedef PawsValue<PawsFunc*> PawsFunction;

struct PawsRegularFunc : public PawsFunc
{
	MincBlockExpr* body;
	mutable std::vector<MincSymbolId> args;
	MincSymbol call(MincBlockExpr* callerScope, const std::vector<MincExpr*>& args, const MincSymbol* self=nullptr) const;

	PawsRegularFunc() = default;
	PawsRegularFunc(PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, MincBlockExpr* body)
		: PawsFunc(returnType, argTypes, argNames), body(body) {}
};

typedef MincSymbol (*FuncBlock)(MincBlockExpr* callerScope, const std::vector<MincExpr*>& argExprs, void* funcArgs);
struct PawsConstFunc : public PawsFunc
{
	FuncBlock body;
	void* funcArgs;
	MincSymbol call(MincBlockExpr* callerScope, const std::vector<MincExpr*>& argExprs, const MincSymbol* self=nullptr) const
	{
		return body(callerScope, argExprs, funcArgs);
	}

	PawsConstFunc() = default;
	PawsConstFunc(PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, FuncBlock body, void* funcArgs = nullptr)
		: PawsFunc(returnType, argTypes, argNames), body(body), funcArgs(funcArgs) {}
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
	PawsExternFunc(F func);
};

template <typename R, typename... A>
struct PawsExternFunc<R (*)(A...)> : public PawsFunc
{
	typedef R F(A...);

	F* func;
	PawsExternFunc(F func) : func(func)
	{
		returnType = PawsValue<R>::TYPE;

		for_each(std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
			typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
			argTypes.push_back(PawsValue<P>::TYPE);
			argNames.push_back("a" + std::to_string(i));
		});
	}

	MincSymbol call(MincBlockExpr* callerScope, const std::vector<MincExpr*>& args, const MincSymbol* self=nullptr) const
	{
		if constexpr (std::is_void<R>::value)
		{
			call_with_args(func, std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
				typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
				PawsValue<P>* p = (PawsValue<P>*)codegenExpr(args[i], callerScope).value;
				return p->get();
			});
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(
				PawsValue<R>::TYPE,
				new PawsValue<R>(
					call_with_args(func, std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
						typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
						PawsValue<P>* p = (PawsValue<P>*)codegenExpr(args[i], callerScope).value;
						return p->get();
					})
				)
			);
	}
};

template <typename R, typename C, typename... A>
struct PawsExternFunc<R (C::*)(A...)> : public PawsFunc
{
	typedef R (C::*F)(A...);

	F func;
	PawsExternFunc(F func) : func(func)
	{
		returnType = PawsValue<R>::TYPE;

		for_each(std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
			typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
			argTypes.push_back(PawsValue<P>::TYPE);
			argNames.push_back("a" + std::to_string(i));
		});
	}

	MincSymbol call(MincBlockExpr* callerScope, const std::vector<MincExpr*>& args, const MincSymbol* self=nullptr) const
	{
		PawsValue<C*>* s = (PawsValue<C*>*)self->value;
		if constexpr (std::is_void<R>::value)
		{
			call_with_args(s->get(), func, std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
				typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
				PawsValue<P>* p = (PawsValue<P>*)codegenExpr(args[i], callerScope).value;
				return p->get();
			});
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(
				PawsValue<R>::TYPE,
				new PawsValue<R>(
					call_with_args(s->get(), func, std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
						typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
						PawsValue<P>* p = (PawsValue<P>*)codegenExpr(args[i], callerScope).value;
						return p->get();
					})
				)
			);
	}
};

void defineFunction(MincBlockExpr* scope, const char* name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, MincBlockExpr* body);
void defineConstantFunction(MincBlockExpr* scope, const char* name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, FuncBlock body, void* funcArgs = nullptr);
template<class F> void defineExternFunction(MincBlockExpr* scope, const char* name, F func)
{
	PawsFunc* pawsFunc = new PawsExternFunc(func);
	defineSymbol(scope, name, PawsTpltType::get(scope, PawsFunction::TYPE, pawsFunc->returnType), new PawsFunction(pawsFunc));
}

#endif