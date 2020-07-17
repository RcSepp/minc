all: helloworld-C++.so

clean:
	rm helloworld-C++.so

helloworld-C++.so: helloworld.cpp
	gcc -g -Wall -fPIC -shared -I${MINC_INCLUDE} -o helloworld-C++.so helloworld.cpp