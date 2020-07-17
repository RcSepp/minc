all: helloworld-llvm.so

clean:
	rm helloworld-llvm.so

helloworld-llvm.so: helloworld-llvm.cpp
	gcc -g -Wall -fPIC -shared `llvm-config --cxxflags --ldflags --system-libs --libs all` -fexceptions -I${MINC_INCLUDE} -o helloworld-llvm.so helloworld-llvm.cpp