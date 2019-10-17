struct PawsFunc
{
	BaseType* returnType;
	std::vector<BaseType*> argTypes;
	std::vector<std::string> argNames;
	BlockExprAST* body;
	virtual Variable call(BlockExprAST* parentBlock, const std::vector<ExprAST*>& args) const;
};
typedef PawsType<PawsFunc*> PawsFunction;


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
		returnType = PawsType<R>::TYPE;

		for_each(std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
			typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
			argTypes.push_back(PawsType<P>::TYPE);
			argNames.push_back("a" + std::to_string(i));
		});

		body = nullptr;
	}

	Variable call(BlockExprAST* parentBlock, const std::vector<ExprAST*>& args) const
	{
		return Variable(
			PawsType<R>::TYPE,
			new PawsType<R>(
				call_with_args(func, std::make_index_sequence<sizeof...(A)>{}, [&](auto i) constexpr {
					typedef typename std::tuple_element<i, std::tuple<A...>>::type P;
					PawsType<P>* p = (PawsType<P>*)codegenExpr(args[i], parentBlock).value;
					return p->val;
				})
			)
		);
	}
};

template<class F> void defineExternFunction(BlockExprAST* scope, const char* name, F func)
{
	PawsFunc* pawsFunc = new PawsExternFunc(func);
	defineSymbol(scope, "test", PawsTpltType::get(PawsFunction::TYPE, pawsFunc->returnType), new PawsFunction(pawsFunc));
}