struct PawsFunc
{
	PawsType* returnType;
	std::vector<PawsType*> argTypes;
	std::vector<std::string> argNames;
	BlockExprAST* body;
	virtual Variable call(BlockExprAST* callerScope, const std::vector<ExprAST*>& args) const;

	PawsFunc() = default;
	PawsFunc(PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, BlockExprAST* body)
		: returnType(returnType), argTypes(argTypes), argNames(argNames), body(body) {}
};
typedef PawsValue<PawsFunc*> PawsFunction;


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

template <typename F>
struct PawsExternFunc : public PawsFunc
{
	PawsExternFunc(F func);
};

template <typename R, typename... A>
struct PawsExternFunc<R (*)(A...)> : public PawsFunc {
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

		body = nullptr;
	}

	Variable call(BlockExprAST* callerScope, const std::vector<ExprAST*>& args) const
	{
		if constexpr (std::is_void<R>::value)
		{
			call_with_args(func, std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
				typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
				PawsValue<P>* p = (PawsValue<P>*)codegenExpr(args[i], callerScope).value;
				return p->get();
			});
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(
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

template<class F> void defineExternFunction(BlockExprAST* scope, const char* name, F func)
{
	PawsFunc* pawsFunc = new PawsExternFunc(func);
	defineSymbol(scope, name, PawsTpltType::get(PawsFunction::TYPE, pawsFunc->returnType), new PawsFunction(pawsFunc));
}