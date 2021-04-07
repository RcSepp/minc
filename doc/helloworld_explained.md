# More about the Helloworld language

This chapter will take a closer look at the helloworld language defined [here](../README.md#Add-or-extend-languages-with-few-lines-of-code).

1. Defining the helloworld package

	```C++
	// Create `helloworld` package
	MincPackage HELLOWORLD_PKG("helloworld", [](MincBlockExpr* pkgScope) {
		...
	});
	```

	In Minc, languages are imported like any other software packages. A language can be defined in either a single package or multiple packages. For example, the builtin Paws language is defined in multiple small packages, such as `paws.array` and `paws.exception`.
	Packages register themselves once they are defined. We define the global variable `HELLOWORLD_PKG`, which is constructed when Minc loads our language's library (`helloworld.so`).

	In order for Minc to find the `*.so` file, it must be located in a directory of the same name, which itself must be located in the Minc package search path. The package search path is defined in the environment variable `MINC_PATH`.

	For example, given MINC_PATH contains the following paths:
	```
	echo $MINC_PATH
	~/minc/builtin-packages:~/minc-packages
	```
	... then helloworld should be located in any of these locations:
	```
	~/minc/builtin-packages/helloworld/helloworld.so
	~/minc-packages/helloworld/helloworld.so
	```

	When the helloworld package is imported, all symbols, statements, expressions and casts defined in `pkgScope` will be imported into the target scope.

2. Define the language's symbols:

	```C++
	// Create `string` data type
	MincObject STRING_TYPE, META_TYPE;
	pkgScope->defineSymbol("string", &META_TYPE, &STRING_TYPE);
	```

	Types, variables and constants are all symbols. Minc does not differentiate between them. Symbols can be accessed at buildtime using the `MincBlockExpr::lookupSymbol()` and `MincBlockExpr::lookupStackSymbol()` methods. Symbols that appear within the type specifier of an expression template (i.e. `$E<string>`) are looked up internally when the template is parsed.

	A symbol consists of type and value. The data type of type and value is the empty class `MincObject`. Any object can be made into a Minc object by deriving from this class. Here we use the `MincObject` directly, because `META_TYPE` is unused and `STRING_TYPE` is only used internally for matching objects returned from the `$L` expression with the parameter of the `print($E<string>)` statement.

3. Define the language's expressions:

	```C++
	// Create expression kernel for literal expressions
	// Examples of literals are "foo", 128 or 3.14159
	// For this example we only allow string expressions, like "foo" or 'bar'
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$L")[0],
		[](MincRuntime& runtime, const std::vector<MincExpr*>& params) -> bool {
			const std::string& value = ((MincLiteralExpr*)params[0])->value;

			if (value.back() == '"' || value.back() == '\'')
				runtime.result = new std::string(value.substr(1, value.size() - 2));
			else
				raiseCompileError("Non-string literals not implemented", params[0]);
			return false;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) -> MincObject* {
			const std::string& value = ((MincLiteralExpr*)params[0])->value;
			if (value.back() == '"' || value.back() == '\'')
				return &STRING_TYPE;
			else
				return getErrorType();
		}
	);
	```

	Both statements and expressions are defined with a template (here `$L`) and a `MincKernel`. Alternatively, we can pass the `build()`, `run()` and `getType()` functions directly, which will create a kernel internally. In the above case, we only pass `run()` and `getType()` functions.

	* The `getType()` function is used during the resolver phase, which matches program code against statement- and expression kernels.
	* The `build()` function is used at buildtime to prepare the kernel for runtime execution. For example, variables are usually looked up at buildtime using `MincBlockExpr::lookupStackSymbol()` and the returned `MincStackSymbol` is used at runtime to access the symbol.
	* The `run()` function is used ar runtime to perform the actual operation. To create a performant programming language, all `run()` methods should be carefully optimized. The literal kernel defined above can be improved by constructing the string value at buildtime and only returning the stored value at runtime.

	The following rules should be followed when designing statement- and expression kernels:
	1. `CompileError`s should be thrown from `build()` (build errors) or from `run()` (runtime errors).
	1. `getType()` should never throw exceptions. Instead inputs that would raise a compile error when passed to build or run (if known at buildtime) should return `getErrorType()`.
	1. Results returned from `build()` (buildtime values) or from `run()` (runtime values) should always be of the type returned by `getType()` with identical input.

	Minc does not explicitly enforce these rules, but breaking them will lead to unexpected bahavior.

4. Define the language's statements:

	```C++
	// Create statement kernel for interpreting the `print(...)` statement
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("print($E<string>)"),
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params) {
			params[0]->build(buildtime);
		},
		[](MincRuntime& runtime, const std::vector<MincExpr*>& params) -> bool {
			if (params[0]->run(runtime))
				return true;
			std::string* const message = (std::string*)runtime.result;
			std::cout << *message << '\n';
			return false;
		}
	);
	```

	Statements are top-level expressions. Executing the code `import helloworld; "foo";` would raise an exception, because the `$L` expression can only be executed embedded in a statement such as `print($E<string>)`. 

	Statements differ from expressions in the following ways:
	1. Statements are top-level expressions.
	1. Statements can consist of multiple consecutive expressions (i.e. `do $S while $E<bool>` is a valid statement template, but would be invalid as an expression template). *Note: The template parameter of `MincBlockExpr::defineExpr()` is a `MincExpr*`, while the template parameter of `MincBlockExpr::defineStmt()` is a `std::vector<MincExpr*>`.*
	1. Statements don't return values.

## Where to go from here

Arbitrarily powerful programming languages can be designed using the same components used in the `helloworld` language. Arithmetic expressions, "if" statements, "class" types, ... Add as many as you like.

The [Paws programming language](builtin-packages/paws/src/) is an excellent reference on how to implement many of the classic programming language constructs, but feel free to be creative!