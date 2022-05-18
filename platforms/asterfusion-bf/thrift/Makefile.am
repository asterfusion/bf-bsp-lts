lib_LTLIBRARIES = libplatform_thrift.la
AM_CPPFLAGS += -I$(srcdir)/../include
libplatform_thrift_la_CXXFLAGS = $(AM_CXXFLAGS)

libplatform_thrift_la_SOURCES = \
src/bf_platform_rpc_server.cpp \
src/pltfm_mgr_rpc_server.ipp \
src/pltfm_pm_rpc_server.ipp \
$(bf_platform_thrift_cxx_files)

AM_CPPFLAGS += -I$(builddir)/gen-cpp/
 
pltfm_mgr_rpc_thrift_cxx_files = \
gen-cpp/pltfm_mgr_rpc.cpp \
gen-cpp/pltfm_mgr_rpc.h \
gen-cpp/pltfm_mgr_rpc_types.cpp \
gen-cpp/pltfm_mgr_rpc_types.h
pltfm_mgr_rpc_thrift_py_files = \
@builddir@/gen-py/pltfm_mgr_rpc/constants.py \
@builddir@/gen-py/pltfm_mgr_rpc/__init__.py \
@builddir@/gen-py/pltfm_mgr_rpc/ttypes.py \
@builddir@/gen-py/pltfm_mgr_rpc/pltfm_mgr_rpc.py \
@builddir@/gen-py/pltfm_mgr_rpc/pltfm_mgr_rpc-remote
pltfm_mgr_rpcdir = $(pythondir)/pltfm_mgr_rpc
pltfm_mgr_rpc_PYTHON = $(pltfm_mgr_rpc_thrift_py_files)

pltfm_pm_rpc_thrift_cxx_files = \
gen-cpp/pltfm_pm_rpc.cpp \
gen-cpp/pltfm_pm_rpc.h \
gen-cpp/pltfm_pm_rpc_types.cpp \
gen-cpp/pltfm_pm_rpc_types.h
pltfm_pm_rpc_thrift_py_files = \
@builddir@/gen-py/pltfm_pm_rpc/constants.py \
@builddir@/gen-py/pltfm_pm_rpc/__init__.py \
@builddir@/gen-py/pltfm_pm_rpc/ttypes.py \
@builddir@/gen-py/pltfm_pm_rpc/pltfm_pm_rpc.py \
@builddir@/gen-py/pltfm_pm_rpc/pltfm_pm_rpc-remote
pltfm_pm_rpcdir = $(pythondir)/pltfm_pm_rpc
pltfm_pm_rpc_PYTHON = $(pltfm_pm_rpc_thrift_py_files)

bf_platform_thrift_cxx_files = \
$(pltfm_mgr_rpc_thrift_cxx_files) \
$(pltfm_pm_rpc_thrift_cxx_files)

bf_platform_thrift_files = \
$(bf_platform_thrift_cxx_files) \
$(pltfm_mgr_rpc_thrift_py_files) \
$(pltfm_pm_rpc_thrift_py_files)

BUILT_SOURCES = \
$(bf_platform_thrift_files)

# The recipe to invoke the thrift compiler is copied from:
# http://www.gnu.org/software/automake/manual/html_node/Multiple-Outputs.html
bfplatform.ts : @srcdir@/pltfm_mgr_rpc.thrift @srcdir@/pltfm_pm_rpc.thrift
	@rm -f bfplatform_fixed.tmp
	@touch bfplatform_fixed.tmp
	$(THRIFT) -o $(builddir) --gen cpp -r $(srcdir)/pltfm_mgr_rpc.thrift
	$(THRIFT) -o $(builddir) --gen py -r $(srcdir)/pltfm_mgr_rpc.thrift
	$(THRIFT) -o $(builddir) --gen cpp -r $(srcdir)/pltfm_pm_rpc.thrift
	$(THRIFT) -o $(builddir) --gen py -r $(srcdir)/pltfm_pm_rpc.thrift
	@mv -f bfplatform_fixed.tmp $@

$(BUILT_SOURCES) : bfplatform.ts
## Recover from the removal of $@
	@if test -f $@; then :; else \
	  trap 'rm -rf bfplatform_fixed.lock bfplatform.ts' 1 2 13 15; \
## mkdir is a portable test-and-set
	if mkdir bfplatform_fixed.lock 2>/dev/null; then \
## This code is being executed by the first process.
	  rm -f bfplatform.ts; \
	  $(MAKE) $(AM_MAKEFLAGS) bfplatform.ts; \
	  result=$$?; rm -rf bfplatform_fixed.lock; exit $$result; \
	else \
## This code is being executed by the follower processes.
## Wait until the first process is done.
	  while test -d bfplatform_fixed.lock; do sleep 1; done; \
## Succeed if and only if the first process succeeded.
	    test -f bfplatform.ts; \
	  fi; \
	fi


CLEANFILES = $(BUILT_SOURCES) \
bfplatform.ts \
thrift_gen/gen-cpp/pltfm_mgr_rpc_server.skeleton.cpp \
thrift_gen/gen-cpp/pltfm_pm_rpc_server.skeleton.cpp 

