 dnl Process this file with autoconf to produce a configure script.
AC_REVISION($Revision$)dnl

dnl initialization
AC_PREREQ(2.69)
AC_INIT(quintuple-agent, 1.0.4)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AC_USE_SYSTEM_EXTENSIONS

dnl internationalization
ALL_LINGUAS="de es_ES fr it pl pt_BR ru"
AM_GNU_GETTEXT(external)

dnl checks for programs
AC_PROG_RANLIB
AC_PATH_PROG(SETCAP, setcap, :, "$PATH:/sbin:/usr/sbin")
AC_PATH_PROG(XMESSAGE, xmessage, , "$PATH:/usr/X11/bin:/usr/bin/X11")
if test x"$XMESSAGE" != x; then
  AC_DEFINE_UNQUOTED(XMESSAGE, "$XMESSAGE",
    [Path of xmessage binary (if available)])
fi

dnl checks for libraries
AC_CHECK_LIB(socket, connect)
AM_PATH_GLIB(1.2.0,,
    AC_MSG_ERROR([
*** GLIB 1.2.0 or better is required. The latest version of GLIB
*** is always available from ftp://ftp.gtk.org/.]))
AM_PATH_GTK(1.2.0,
    GTK_STUFF="secret-query secret-ask"
    AC_DEFINE(HAVE_GTK, [], [Define as 1 if you have GTK+.]),
    AC_MSG_WARN([[
*** GTK+ 1.2.0 or better is required for secret-query, and secret-ask.
*** They will not be built.
*** For the latest version of GTK+, see ftp://ftp.gtk.org/.]]))
AC_SUBST(GTK_STUFF)
AC_CHECK_LIB(cap, cap_set_proc, [
  AC_DEFINE(USE_CAPABILITIES, [],
    [Define as 1 if you have want to use capabilities.])
  LIBCAP=-lcap
])
AC_SUBST(LIBCAP)

dnl checks for header files
AC_CHECK_HEADERS(getopt.h)
AC_CHECK_HEADERS(inttypes.h, , need_inttypes=yes)
if test x$need_inttypes = xyes; then
  AC_CHECK_SIZEOF(unsigned int, 4)
  AC_CHECK_SIZEOF(unsigned long, 4)
fi

dnl checks for typedefs
AC_CHECK_TYPE(socklen_t, int)
AC_CHECK_TYPE(ulong, unsigned long)
AC_CHECK_TYPE(ssize_t, signed long)

dnl checks for compiler characteristics
AC_ARG_ENABLE(debug, [
  --enable-debug[=FLAGS] turns on compiler switches useful in development],
[
  if test "x$enableval" = xyes; then
    CFLAGS="-Wall -g -O"
  else
    CFLAGS="$enableval"
  fi
])

dnl checks for library functions
AC_CHECK_FUNCS(getopt_long,,[
AC_LIBOBJ(getopt)
AC_LIBOBJ(getopt1)
])
AC_CHECK_FUNCS(getdelim seteuid strsignal vsnprintf)
AC_REPLACE_FUNCS(asprintf getline setenv strdup)
GNUPG_CHECK_MLOCK

AC_DEFINE(QUERY_PROGRAM, "secret-query",
  [Program to use for querying the user for a secret])

AC_OUTPUT(Makefile doc/Makefile lib/Makefile po/Makefile.in test/Makefile)
