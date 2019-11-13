all: Core.ll CloneModule.ll DebugInfo.ll ExecutionEngineBindings.ll

clean:
	-rm *.ll

%.ll: %.cpp
	clang -O1 -S -emit-llvm -o $@ $<