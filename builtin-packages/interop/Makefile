SHELL := /bin/bash
SRC_DIR = src/
TMP_DIR = tmp/
OUT_DIR = ./
TST_DIR = test/

OBJS = \
	interop.o \

CPPFLAGS = -g -Wall -std=c++1z -I${MINC_INCLUDE}

OBJPATHS = $(addprefix ${TMP_DIR}, ${OBJS})

all: ${OUT_DIR}interop.so

clean:
	-rm -r ${TMP_DIR}* ${OUT_DIR}interop.so

# Dependency management

depend: $(OBJPATHS:.o=.d)

${TMP_DIR}%.d: ${SRC_DIR}%.cpp
	@mkdir -p `dirname $@`
	$(CXX) $(CPPFLAGS) -MM -MT ${TMP_DIR}$*.o $^ > $@;

-include $(OBJPATHS:.o=.d)

# interop.so library

${OUT_DIR}interop.so: ${OBJPATHS}
	@mkdir -p ${OUT_DIR}
	${CXX} ${CPPFLAGS} -shared -o $@ ${OBJPATHS} -L${MINC_BIN} -lminc -lminc_pkgmgr

# Compiler code

${TMP_DIR}%.o: ${SRC_DIR}%.cpp
	@mkdir -p `dirname $@`
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<
