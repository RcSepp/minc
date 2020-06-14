# Minc - The Minimal Compiler

Language compilers and interpreters are complex programs that can take years to develop. With Minc you can create one in a few hours ...

## Just how easy can it be?

A programming language in Minc consists of 5 components: **packages**, **types**, **symbols**, **statements** and **expressions**.
A programming language that can run hello world programs can be defined in less than 100 lines of code:

1) Define a package, so that your language can be imported in `minc`:

```C++
MincPackage HELLOWORLD_PKG("helloworld", [](BlockExprAST* pkgScope) {
	...
});
```

2) Define some types (Every language needs to have at least one):

```C++
defineType("string", &STRING_TYPE);
```

3) Define a few symbols (Here we define the meta-type of our string type, so that Minc knows how to interpret the statement template "print($E<string>)" below):

```C++
defineSymbol(pkgScope, "string", &META_TYPE, &STRING_META_TYPE);
```

4) Define the language's expressions:

```C++
defineExpr3(pkgScope, "$L",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) {
		const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);
		if (value[0] != '"' && value[0] != '\'')
			raiseCompileError("Non-string literals not implemented", params[0]);
		return Variable(&STRING_TYPE, new String(std::string(value + 1, strlen(value) - 2)));
	},
	[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
		return &STRING_TYPE;
	}
);
```

5) Define the language's statements:

```C++
defineStmt2(pkgScope, "print($E<string>)",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
		String* const message = (String*)codegenExpr(params[0], parentBlock).value;
		std::cout << *message << '\n';
	}
);
```

Et voilà!

```C++
> import helloworld;
> print("Hello World!");
"Hello World!"
```

You just wrote a programming language!

Note: You may have noticed we didn't declare the "import" statement. Import and export are the only predefined statements in Minc (hence the term: *minimal* compiler).

## Where to go from here

Arbitrarily powerful programming languages can be designed using the same 5 components as our `helloworld` language. Arithmetic expressions, "if" statements, "class" types, ... Just add as many as you like.

The Paws programming language is an excellent reference on how to implement many of the classic programming language constructs, but feel free to be creative!

## High Level Goals

Minc brings significant contributions in the field of modern software architecture. The following high level goals have directed the design of this library.

* Proliferation of programming languages

An ever growing amount of outstanding programming languages have made writing quality code both easier and more complex at the same time. The question about the best language for a specific domain often has no right answer, while wrong answers can incur significant cost to development.

Minc's goal is to eventually support all major programming languages, with transparent wrappers between any two of them built in. This eliminates the need for countless wrapper libraries and answers the unanswerable question: The best programming language is a super-set of all languages. By choosing Minc as the platform for an enterprise scale software system, the architect allows engineers of different teams and backgrounds to collaborate on a shared codebase using each individual's language of choice.

* Abstraction of higher level responsibilities from the core language

Many higher level tasks are traditionally performed by the programming languages itself or by language specific tools. Examples of these tasks are software configuration and version management. Each traditional language introduces separate tool chains and best practices, contributing significantly to the time it takes a programmer to master a new language.

Minc enables the creation of software-agnostic tools that consistently manage the entire software stack.

* Decoupling of software dependencies

Before the advent of the internet, software development was a linear process. Each application started at the design phase, went through some iterations of implementation and testing and eventually ended up being frozen into a final release in the form of a set of floppy disks or a CD. The possibility to update software post-release gave rise to the vicious circle of software maintenance. Today a software that is not under constant development is considered stale and outdated. Even if one were to create a perfect piece of code without any bugs, eventually one of its dependencies will introduce breaking changes with the implementation of a critical security fix or an important new feature. This will force the previously perfect code to be adapted, potentially introducing new bugs and forcing dependent code to propagate the update. With each layer of dependencies, the problem grows exponentially. A program that cannot keep up or whose developers have moved on to other projects will be marked obsolete and replaced by newer tools engineered to relive the same challenges until it will too fall out of the never ending dependency cycle.

The dependency cycle cannot be fully broken, but it's exponential blast radius can. By introducing the ability to transparently mix different programming languages and even different versions of the same language, smaller dependency cycles can be isolated on a per-import basis. It allows using old-and-proven software libraries side-by-side with cutting-edge new packages, preserving the validity of software beyond its retirement date. By shifting development efforts from maintenance. to improvement, Minc has the capability to reshape the present circular redevelopment industry into the goal oriented innovation machinery of the future.

## Features

**TODO**

## Limitations

### Compiling source code

You say compiler, but all Minc does is interpret source code...
Correct, at it's current stage Minc consists only of lexer, parser and code generator. To upgrade your language to a compiled language, you need to emit intermediary byte-code or a program binary (e.g. using LLVM) in your statements and expressions. In the future this will be (optionally) handled by a separate part of Minc

**TODO: Write heLLVMoworld using LLVM**

### Selecting a parser

One of the many bold goals of Minc is to be able to compile any programming language in existence, but even an infinite improbability drive requires coordinates [Quote](https://www.imdb.com/title/tt0371724/quotes/qt0351150).

These coordinates are the language grammar. Currently statements and expressions aren't interpreted from raw source code, but from a static abstract syntax tree (AST), generated by a parser ([GNU Bison](https://www.gnu.org/software/bison/)). The difference between Minc's parser and any regular parser for a static language is that Minc's parsers are designed to be as flexible as possible within the boundaries of the underlying language. They can be seen as laying some ground rules for the language. For example: C-flavored languages delimit lines with ";" and surround blocks with "{ ... }", while Python-flavored languages specify blocks with ":" and increased indentation.

At the moment Minc supports two parsers:

* C-Parser
* Python-Parser

The language flavor can only be switched between files. A parser-free interpreter that directly matches statements and expressions from source code is one of the stretch goals of Minc. It will allow switching flavors anywhere in code and can truly compile any language (even textual data file formats, like Markdown or XML).

Until that time being restricted by the lean boundaries of the Minc parsers should be seen as more of an advantage than a hindrance. (The more unrestricted your language, the more your users (the programmers) have to scratch their heads before they can efficiently code with it.)

**TODO: Rename parsers "styles" or "flavors"**
