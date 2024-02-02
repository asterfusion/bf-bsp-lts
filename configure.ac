#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([bf_platforms], [0.1], [ravindra@barefootnetworks.com])
AM_PATH_PYTHON([2.7],,[:])
# This prevents autotools from automatically adding -O2 flag.
: ${CXXFLAGS=""}
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# enforce -std=c++11
AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])

AC_ARG_WITH([asan],
            [AS_HELP_STRING([--with-asan],
                            [Build with address sanitizer checking])],
            [with_asan=${withval}],
            [with_asan=no])
AS_IF([test "x$with_asan" = "xno"],
      [ASAN_FLAGS=],
      [test "x$with_asan" = "xyes"],
      [ASAN_FLAGS="-fsanitize=address -fsanitize-recover=address"],
      [ASAN_FLAGS="$with_asan"])

MY_CPPFLAGS=
AC_ARG_ENABLE([thrift],
    AS_HELP_STRING([--enable-thrift], [Enable thrift flags]))
AS_IF([test "x$enable_thrift" = "xyes"], [
    AC_DEFINE([ENABLE_THRIFT], [], ["Build with thrift."])
])

AC_ARG_WITH([tcl-only],
              [AS_HELP_STRING([--with-tcl-only],
                              [Build only tcl server])],
              [with_tcl_only=yes],
              [with_tcl_only=no])
AM_CONDITIONAL([TCL_ONLY], [test "$with_tcl_only" = yes])

build_asterfusionbf=yes
build_newport=no
build_tof_brgup=no
AC_ARG_WITH([tof-brgup-plat],
              [AS_HELP_STRING([--with-tof-brgup-plat],
                              [Build Tofino Bringup Platform])],
              [build_asterfusionbf=no
               build_newport=no
               build_tof_brgup=yes],
              [])
AC_ARG_WITH([newport-plat],
              [AS_HELP_STRING([--with-newport-plat],
                              [Build Newport Platform])],
              [build_asterfusionbf=no
               build_newport=yes
               build_tof_brgup=no],
              [])
#AM_CONDITIONAL([BUILD_TOFINO_BRINGUP_PLAT], [test "x$build_tof_brgup" = xyes])
#AM_CONDITIONAL([BUILD_NEWPORT_PLAT], [test "x$build_newport" = xyes])
AM_CONDITIONAL([BUILD_ASTERFUSIONBF_PLAT], [test "x$build_asterfusionbf" = xyes])

asterfusion_diags=no
AC_ARG_WITH([asterfusion-diags],
              [AS_HELP_STRING([--with-asterfusion-diags],
                              [Build Asterfusion Diags])],
              [asterfusion_diags=yes],
              [asterfusion_diags=no])
AM_CONDITIONAL([BUILD_ASTERFUSIONBF_DIAGS], [test "x$asterfusion_diags" = xyes])

want_tofino=no
AC_ARG_WITH([tofino],
    AS_HELP_STRING([--with-tofino], [Build for tofino target]),
    [want_tofino=yes], [])
AM_CONDITIONAL([WITH_TOFINO], [test "$want_tofino" = yes])

build_for_asic_or_model=
AC_ARG_WITH([model],
    AS_HELP_STRING([--with-model], ["Build for the software asic model"]),
    [build_for_asic_or_model=model], 
    [build_for_asic_or_model=asic])
AM_CONDITIONAL([IS_ASIC], [test "x$build_for_asic_or_model" = xasic])

# Option to statically link all SDE libraries
AC_ARG_WITH([static-link-lib],
            AS_HELP_STRING([--with-static-link-lib], ["Statically link all SDE libraries"]))
AM_CONDITIONAL([IS_STATIC_LINKED_LIB],[test "x$with_static_link_lib" = "xyes"])

# Checks for programs.
AC_PROG_CC_STDC
AC_PROG_CXX
LT_INIT

# Checks for header files.
AC_LANG_PUSH(C)
AC_LANG_PUSH(C++)

# To simplify usage, we will update PATH, CPPFLAGS,.. to include the 'prefix'
# ones
adl_RECURSIVE_EVAL([$bindir], [BIN_DIR])
AC_SUBST([BIN_DIR], ["$BIN_DIR"])
adl_RECURSIVE_EVAL([$includedir], [INCLUDE_DIR])
adl_RECURSIVE_EVAL([$pythondir], [PYTHON_DIR])
adl_RECURSIVE_EVAL([$libdir], [LIB_DIR])

AC_CHECK_HEADERS([assert.h math.h stdio.h string.h time.h unistd.h], \
[], [AC_MSG_ERROR([Missing header file])])


AC_C_INLINE
AC_FUNC_MALLOC
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

AC_SUBST([PREFIX_PATH], ["$(if test x$prefix != xNONE; then \
                             echo $prefix; \
                            else \
                              echo $ac_default_prefix; \
                            fi \
                           )"])
export PREFIX_PATH
AC_SUBST([BFPLATFORMS_CFLAGS], ["-g -Wall -Wextra -Wpedantic -Wno-missing-field-initializers -Wno-unused-parameter -Werror"])
AC_SUBST([AM_CFLAGS], ["$COVERAGE_FLAGS $ASAN_FLAGS -I$PREFIX_PATH/include"])
AC_SUBST([AM_CXXFLAGS], ["$COVERAGE_FLAGS $ASAN_FLAGS -I$PREFIX_PATH/include"])
AC_SUBST([AM_CPPFLAGS], ["$MY_CPPFLAGS -I\$(top_srcdir)/drivers/include -I\$(top_srcdir)/platforms/include -I\$(top_srcdir)/platforms/asterfusion-bf/tcl_server/include"])
AC_SUBST([AM_LDFLAGS], ["$COVERAGE_FLAGS -L$LIB_DIR"])
AC_SUBST([MY_PYTHONPATH], ["$PYTHON_DIR"])

AC_CHECK_LIB([pthread], [pthread_create], [],
             [AC_MSG_ERROR([required library pthread missing])])

AM_CONDITIONAL([IS_THRIFT_ENABLED], [test "x$enable_thrift" = "xyes"])
AS_IF([test "x$enable_thrift" = "xyes"], [
  AC_PATH_PROG([THRIFT], [thrift], [])
  AS_IF([test x"$THRIFT" = x], [AC_MSG_ERROR([cannot find thrift])])
  #AC_CHECK_LIB([thrift], [_init], [], [AC_MSG_ERROR([Missing thrift library])])
  AC_CHECK_HEADER([thrift/Thrift.h], [], [AC_MSG_ERROR([Thrift headers not found])])
  AC_SUBST([THRIFT_LIB], ["-lthrift"])
  AC_CHECK_HEADER([thrift/stdcxx.h],
                  [AC_DEFINE([HAVE_THRIFT_STDCXX_H],
                             [1],
                             [Found Thrift stdcxx wrapper])],
                  [])
  AS_ECHO("Thrift enabled")
])

# Generate makefiles
AC_CONFIG_FILES([Makefile
		 drivers/Makefile
		 drivers/include/Makefile
		 drivers/src/Makefile
		 platforms/Makefile
		 platforms/include/Makefile
		 platforms/asterfusion-bf/Makefile
		 platforms/asterfusion-bf/include/Makefile
		 platforms/asterfusion-bf/src/Makefile
		 platforms/asterfusion-bf/cp2112_util/Makefile
		 platforms/asterfusion-bf/uart_util/Makefile
		 platforms/asterfusion-bf/thrift/Makefile
		 platforms/asterfusion-bf/tofino_spi_util/Makefile
		 platforms/asterfusion-bf/tofino_pci_util/Makefile])
		 #platforms/asterfusion-bf/tcl_server/Makefile
		 #platforms/asterfusion-bf/diags/Makefile
		 #platforms/asterfusion-bf/diags/mavericks/Makefile
		 #platforms/asterfusion-bf/diags/mavericks/targets/Makefile
		 #platforms/asterfusion-bf/diags/mavericks/targets/tofino/mav_diag.conf
		 #platforms/newport/Makefile
		 #platforms/newport/include/Makefile
		 #platforms/newport/kdrv/Makefile
		 #platforms/newport/src/Makefile
		 #platforms/newport/fpga_util/Makefile
		 #platforms/newport/tofino_spi_util/Makefile
		 #platforms/tofino-bringup/Makefile])

AC_OUTPUT

AS_ECHO("")
AS_ECHO("P4 path ..... : $P4_PATH")

