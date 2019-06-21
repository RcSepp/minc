### The problem

With the binary llvm-8-dev release for Ubuntu 18.04 from the LLVM downloads page, JITed functions throw seg faults when calling any LLVM-c functions.
The problem seems to be that this release statically links LLVM (not verified).

JITed code only runs without any linking, because LLVM links all shared libraries used by the host program to the JITed code. Statically linked libraries, however, have to be linked manually.
Manual linking is not supported by the Interpreter JIT.
On MCJIT it is implemented according to https://stackoverflow.com/a/48116001.

When libLLVMCore.a is manually linked, `jitEngine->finalizeObject();` throws "LLVM ERROR: Symbol not found: __dso_handle".

### Code snippet

```C++
    DynamicStmtContext(Function* func)
	{
        // The following lines didn't change anything. They manually add shared objects to LLVM, but it seems these libraries have already been loaded by the host program.
std::string err;
//llvm::sys::DynamicLibrary::LoadLibraryPermanently("/usr/lib/x86_64-linux-gnu/libpthread.so");
//llvm::sys::DynamicLibrary::LoadLibraryPermanently("/usr/lib/x86_64-linux-gnu/libtinfo.so");
bool a = llvm::sys::DynamicLibrary::LoadLibraryPermanently("/usr/lib/gcc/x86_64-linux-gnu/7/libstdc++.so", &err);
bool b = llvm::sys::DynamicLibrary::LoadLibraryPermanently("/lib/x86_64-linux-gnu/libm.so.6", &err);
bool c = llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr, &err);

		EngineBuilder jitBuilder(std::unique_ptr<Module>(func->getParent()));
jitBuilder.setOptLevel(CodeGenOpt::None);
//jitBuilder.setUseOrcMCJITReplacement(true);
		TargetOptions options;
		options.DebuggerTuning = DebuggerKind::GDB;
		jitBuilder.setTargetOptions(options);
		ExecutionEngine* jitEngine = jitBuilder.create();
const std::string libs[] = {
	"/usr/lib/libLLVMCore.a", // This line causes "LLVM ERROR: Symbol not found: __dso_handle"
	"/usr/lib/libLLVMBinaryFormat.a",
	"/usr/lib/libLLVMSupport.a",
	"/usr/lib/libLLVMDemangle.a",
	"/usr/lib/x86_64-linux-gnu/libpthread.a",
	"/usr/lib/x86_64-linux-gnu/libtinfo.a",
};
for (auto& lib: libs)
{
ErrorOr<std::unique_ptr<MemoryBuffer>> buffer = MemoryBuffer::getFile(lib);
if (!buffer) {
	int abc = 0;
}
Expected<std::unique_ptr<object::Archive>> objectOrError = object::Archive::create(buffer.get()->getMemBufferRef());
if (!objectOrError) {
	int abc = 0;
}
std::unique_ptr<object::Archive> Archive(std::move(objectOrError.get()));
auto owningObject = object::OwningBinary<object::Archive>(std::move(Archive), std::move(buffer.get()));
jitEngine->addArchive(std::move(owningObject));
}
		uint64_t funcAddr = jitEngine->getFunctionAddress(func->getName());
		assert(funcAddr);
		this->func = reinterpret_cast<funcPtr>(funcAddr);
		jitEngine->finalizeObject();
ExprAST** params = new ExprAST*[16];
for (int i = 0; i < 16; ++i)
	params[i] = new LiteralExprAST({0}, "TEST");
this->func(wrap(builder), params);


/*llvm::orc::KaleidoscopeJIT jit;
jit.addModule(std::unique_ptr<Module>(func->getParent()));
JITSymbol sym = jit.findSymbol("jitFunction1");
int abc = 0;*/




		/*Expected<std::unique_ptr<llvm::orc::LLJIT>> jit = llvm::orc::LLJIT::Create(orc::JITTargetMachineBuilder(
			Triple(func->getParent()->getTargetTriple())),
			func->getParent()->getDataLayout()
		);
		jit->get()->addIRModule(func->getParent());
		jit->reset();*/
	}
```

### Possible solution

To enable linking llvm statically, create a shared compiler runtime library ("runtime.so") and link that one statically to LLVM.
Link the host program ("./minc") only to the runtime. This should include all required LLVM symbols.
If the JITed code loads symbols not loaded by the host program, compile with `-fdynamic` to force loading all symbols (including unused symbols).