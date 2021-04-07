SHELL := /bin/bash
SRC_DIR = src/
TMP_DIR = tmp/
OUT_DIR = ./
COV_DIR = coverage/
TST_DIR = test/

OBJS = \
	paws.o \
	paws_int.o \
	paws_double.o \
	paws_string.o \
	paws_assert.o \
	paws_exception.o \
	paws_extend.o \
	paws_fileio.o \
	paws_time.o \
	paws_castreg.o \
	paws_stmtreg.o \
	paws_struct.o \
	paws_subroutine.o \
	paws_frame.o \
	paws_frame_eventloop.o \
	paws_array.o \

CPPFLAGS = --coverage -g -Wall -std=c++1z -I${MINC_INCLUDE}

OBJPATHS = $(addprefix ${TMP_DIR}, ${OBJS})

all: ${OUT_DIR}paws.so

clean:
	-rm -r ${TMP_DIR}* ${OUT_DIR}paws.so

# Dependency management

depend: $(OBJPATHS:.o=.d)

${TMP_DIR}%.d: ${SRC_DIR}%.cpp
	@mkdir -p `dirname $@`
	$(CXX) $(CPPFLAGS) -MM -MT ${TMP_DIR}$*.o $^ > $@;

-include $(OBJPATHS:.o=.d)

# Coverage

coverage: ${TMP_DIR}/minc/minc.gcda
	lcov -c -d ${TMP_DIR} --include "*/src/*" -o ${TMP_DIR}lcov.info
	genhtml ${TMP_DIR}lcov.info --output-directory ${COV_DIR}

${TMP_DIR}/minc/minc.gcda: ${OUT_DIR}paws.so
	${MINC_BIN}/minc ${TST_DIR}test.minc

# paws.so library

${OUT_DIR}paws.so: ${OBJPATHS}
	@mkdir -p ${OUT_DIR}
	${CXX} ${CPPFLAGS} -shared -o $@ ${OBJPATHS} -L${MINC_BIN} -lminc -lminc_pkgmgr

# Compiler code

${TMP_DIR}%.o: ${SRC_DIR}%.cpp
	@mkdir -p `dirname $@`
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<
