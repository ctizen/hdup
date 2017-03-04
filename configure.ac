#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(hdup, 2.0.14, hdup-user@miek.nl, hdup2)
AC_CONFIG_SRCDIR([src/prototype.h])
AC_PREFIX_DEFAULT(/usr)

sinclude(glib-2.0.m4)

AC_AIX
# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
# my own checks
AC_PATH_PROG(tar, tar, "/bin/tar")
AC_PATH_PROG(mcrypt, mcrypt,"/usr/bin/mcrypt")
AC_PATH_PROG(gpg, gpg,"/usr/bin/gpg")
AC_PATH_PROG(ssh, ssh, "/usr/bin/ssh")
AC_PATH_PROG(bzip, bzip2, "/usr/bin/bzip2")
AC_PATH_PROG(gzip, gzip, "/bin/gzip")
AC_PATH_PROG(lzop, lzop, "/bin/lzop")
AC_PATH_PROG(split, split, "/usr/bin/split")
AC_PATH_PROG(cat, cat, "/bin/cat")

# Checks for glib 
AM_PATH_GLIB_2_0(2.0.0)

AC_CHECK_HEADERS([getopt.h sys/vfs.h sys/statvfs.h])
AC_CHECK_HEADERS(sys/param.h sys/mount.h,,,
[
  [
   #if HAVE_SYS_PARAM_H
   # include <sys/param.h>
   #endif
  ]
])

if test $prefix = "NONE"; then
	if test $sysconfdir = '${prefix}/etc'; then
		sysconfdir="/etc"
	fi
fi
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/hdup.h
		 contrib/hdup.spec])
AC_CONFIG_HEADER([src/config.h])
AC_OUTPUT
