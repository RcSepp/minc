SHELL := /bin/bash
SRC_DIR = src/
INC_DIR = include/
TMP_DIR = tmp/
BIN_DIR = bin/
COV_DIR = coverage/
PKG_DIR = builtin-packages/

LIBMINC_OBJS = \
	libminc/cparser.o \
	libminc/cparser.yy.o \
	libminc/pyparser.o \
	libminc/pyparser.yy.o \
	libminc/goparser.o \
	libminc/goparser.yy.o \
	libminc/MincException.o \
	libminc/MincArgOpExpr.o \
	libminc/MincBinOpExpr.o \
	libminc/MincBlockExpr.o \
	libminc/MincBuildtime.o \
	libminc/MincCastExpr.o \
	libminc/MincCastRegister.o \
	libminc/MincEllipsisExpr.o \
	libminc/MincEncOpExpr.o \
	libminc/MincExpr.o \
	libminc/MincIdExpr.o \
	libminc/MincInterpreter.o \
	libminc/MincKernel.o \
	libminc/MincListExpr.o \
	libminc/MincLiteralExpr.o \
	libminc/MincParamExpr.o \
	libminc/MincParser.o \
	libminc/MincPlchldExpr.o \
	libminc/MincPostfixExpr.o \
	libminc/MincPrefixExpr.o \
	libminc/MincRunner.o \
	libminc/MincRuntime.o \
	libminc/MincStatementRegister.o \
	libminc/MincStmt.o \
	libminc/MincStopExpr.o \
	libminc/MincTerOpExpr.o \
	libminc/MincVarBinOpExpr.o \

LIBMINC_PKGMGR_OBJS = \
	libminc_pkgmgr/minc_pkgmgr.o \
	libminc_pkgmgr/minc_discover.o \
	libminc_pkgmgr/json.o \

LIBMINC_DBG_OBJS = \
	libminc_dbg/minc_dbg.o \

LIBMINC_SVR_OBJS = \
	libminc_svr/minc_svr.o \

LIBMINC_LLVM_OBJS = \
	libminc_llvm/Module.o \
	libminc_llvm/LlvmRunner.o \

MINC_OBJS = \
	minc/minc.o \

MINC_LIBS = \
	libminc.so \
	libminc_pkgmgr.so \
	libminc_dbg.so \
	libminc_svr.so \

ifneq (${USE_LLVM},)
	MINC_LIBS := ${MINC_LIBS} libminc_llvm.so
endif

YACC = bison
CPPFLAGS =  --coverage -g -Wall -std=c++1z -I${INC_DIR} -Ithird_party/cppdap/include -Ithird_party/LspCpp/include -Ithird_party/rapidjson/include
LDFLAGS = -lutil -pthread -ldl -rdynamic -lboost_thread -lboost_chrono

ifneq (${USE_NODE_PACKAGES},)
	CPPFLAGS := ${CPPFLAGS} -DUSE_NODE_PACKAGES -I/usr/include/nodejs/src -I/usr/include/nodejs/deps/v8/include -Ithird_party/node/include
	LDFLAGS := -lnode ${LDFLAGS}
endif

ifneq (${USE_PYTHON_PACKAGES},)
	CPPFLAGS := ${CPPFLAGS} -DUSE_PYTHON_PACKAGES `pkg-config --cflags python-3.7`
	LDFLAGS := `pkg-config --libs python-3.7` ${LDFLAGS}
endif

LIBMINC_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_OBJS})
LIBMINC_PKGMGR_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_PKGMGR_OBJS})
LIBMINC_DBG_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_DBG_OBJS})
LIBMINC_SVR_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_SVR_OBJS})
LIBMINC_LLVM_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_LLVM_OBJS})
MINC_OBJPATHS = $(addprefix ${TMP_DIR}, ${MINC_OBJS})
MINC_LIBPATHS = $(addprefix ${BIN_DIR}, ${MINC_LIBS})

ALL_OBJPATHS = \
	${LIBMINC_OBJPATHS} \
	${LIBMINC_PKGMGR_OBJPATHS} \
	${LIBMINC_DBG_OBJPATHS} \
	${LIBMINC_SVR_OBJPATHS} \
	${MINC_OBJPATHS} \

ifneq (${USE_LLVM},)
	ALL_OBJPATHS := ${ALL_OBJPATHS} ${LIBMINC_LLVM_OBJPATHS}
endif

ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

all: ${BIN_DIR}minc builtin

.PHONY: clean depend coverage install uninstall builtin

clean:
	-rm -r ${TMP_DIR}* ${BIN_DIR}libminc.so ${BIN_DIR}libminc_pkgmgr.so ${BIN_DIR}libminc_dbg.so ${BIN_DIR}libminc_svr.so ${BIN_DIR}libminc_llvm.so ${BIN_DIR}minc
	$(foreach DIR, $(wildcard ${PKG_DIR}*/), $(MAKE) -C ${DIR} clean;)

# Dependency management

depend: $(ALL_OBJPATHS:.o=.d)

${TMP_DIR}%.d: ${SRC_DIR}%.cpp
	@mkdir -p `dirname $@`
	$(CXX) $(CPPFLAGS) -MM -MT ${TMP_DIR}$*.o $^ > $@;

-include $(ALL_OBJPATHS:.o=.d)

# Coverage

coverage: ${TMP_DIR}/minc/minc.gcda
	lcov -c -d ${TMP_DIR} --include "*/src/*" -o ${TMP_DIR}lcov.info
	genhtml ${TMP_DIR}lcov.info --output-directory ${COV_DIR}
	$(MAKE) -C ${PKG_DIR}*/ $(MAKECMDGOALS)

${TMP_DIR}/minc/minc.gcda: ${BIN_DIR}minc
	${BIN_DIR}minc ${PKG_DIR}paws/test/test.minc #TODO: Create separate coverage test for minc

# Installation

install:
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 ${BIN_DIR}minc $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/lib/
	install -m 644 ${BIN_DIR}libminc.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 ${BIN_DIR}libminc_pkgmgr.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 ${BIN_DIR}libminc_dbg.so $(DESTDIR)$(PREFIX)/lib/
	install -m 644 ${BIN_DIR}libminc_svr.so $(DESTDIR)$(PREFIX)/lib/
ifneq ("$(wildcard ${BIN_DIR}libminc_llvm.so)","")
	install -m 644 ${BIN_DIR}libminc_llvm.so $(DESTDIR)$(PREFIX)/lib/
endif
	install -d $(DESTDIR)$(PREFIX)/include/
	install -m 644 ${INC_DIR}minc_types.h $(DESTDIR)$(PREFIX)/include/
	install -m 644 ${INC_DIR}minc_api.hpp $(DESTDIR)$(PREFIX)/include/
	install -m 644 ${INC_DIR}minc_api.h $(DESTDIR)$(PREFIX)/include/
	install -m 644 ${INC_DIR}minc_cli.h $(DESTDIR)$(PREFIX)/include/
	install -m 644 ${INC_DIR}minc_pkgmgr.h $(DESTDIR)$(PREFIX)/include/
	install -m 644 ${INC_DIR}minc_dbg.h $(DESTDIR)$(PREFIX)/include/
	install -m 644 ${INC_DIR}minc_svr.h $(DESTDIR)$(PREFIX)/include/
	install -m 644 ${INC_DIR}minc_llvm.h $(DESTDIR)$(PREFIX)/include/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/minc
	rm -f $(DESTDIR)$(PREFIX)/lib/libminc.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libminc_pkgmgr.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libminc_dbg.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libminc_svr.so
	rm -f $(DESTDIR)$(PREFIX)/lib/libminc_llvm.so
	rm -f $(DESTDIR)$(PREFIX)/include/minc_types.h
	rm -f $(DESTDIR)$(PREFIX)/include/minc_api.hpp
	rm -f $(DESTDIR)$(PREFIX)/include/minc_api.h
	rm -f $(DESTDIR)$(PREFIX)/include/minc_cli.h
	rm -f $(DESTDIR)$(PREFIX)/include/minc_pkgmgr.h
	rm -f $(DESTDIR)$(PREFIX)/include/minc_dbg.h
	rm -f $(DESTDIR)$(PREFIX)/include/minc_svr.h
	rm -f $(DESTDIR)$(PREFIX)/include/minc_llvm.h

# Builtin packages

builtin:
	$(foreach DIR, $(wildcard ${PKG_DIR}*/), $(MAKE) -C ${DIR} all MINC_BIN=$(realpath ${BIN_DIR}) MINC_INCLUDE=$(realpath ${INC_DIR});)

# minc binary

${BIN_DIR}minc: ${MINC_OBJPATHS} ${MINC_LIBPATHS}
	@mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} -o $@ ${MINC_OBJPATHS} -L${BIN_DIR} -lminc -lminc_pkgmgr -lminc_dbg -lminc_svr ${LDFLAGS}

# libminc.so library

${BIN_DIR}libminc.so: ${LIBMINC_OBJPATHS}
	@mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} -shared -o $@ ${LIBMINC_OBJPATHS}

# libminc_pkgmgr.so library

${BIN_DIR}libminc_pkgmgr.so: ${LIBMINC_PKGMGR_OBJPATHS}
	@mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} -shared -o $@ ${LIBMINC_PKGMGR_OBJPATHS}

# libminc_dbg.so library

${BIN_DIR}libminc_dbg.so: ${LIBMINC_DBG_OBJPATHS}
	@mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} -shared -o $@ ${LIBMINC_DBG_OBJPATHS} third_party/cppdap/lib/libcppdap.a

# libminc_svr.so library

${BIN_DIR}libminc_svr.so: ${LIBMINC_SVR_OBJPATHS}
	@mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} -shared -o $@ ${LIBMINC_SVR_OBJPATHS} third_party/LspCpp/lib/liblsp.a

# libminc_llvm.so library

${BIN_DIR}libminc_llvm.so: ${LIBMINC_LLVM_OBJPATHS}
	@mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} -shared -o $@ ${LIBMINC_LLVM_OBJPATHS}

# Parser code

${TMP_DIR}%.yy.cc: ${SRC_DIR}%.l ${TMP_DIR}%.o
	@mkdir -p `dirname $@`
	$(LEX) -o $@ $<

${TMP_DIR}%.cc: ${SRC_DIR}%.y
	@mkdir -p `dirname $@`
	$(YACC) -o $@ $<

# Generated parser code

${TMP_DIR}%.o: ${TMP_DIR}%.cc
	@mkdir -p `dirname $@`
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<

# Compiler code

${TMP_DIR}%.o: ${SRC_DIR}%.cpp
	@mkdir -p `dirname $@`
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<
