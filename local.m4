#
# RNDC override for libdesrim
#
# None of this is required to build libdesrim
# setting CFLAGS=.... -O3 will have the same 
# effect (for gcc)
#

# This library should be compiled optimised
ifdef([GN_OPT_ARG],[GN_OPT_ARG()])

AC_PROG_CC
AM_PROG_LIBTOOL

#
# the Sun WorkShop C compiler is preferred if available
#
ifdef([GN_PROG_CC_SUNW], [GN_PROG_CC_SUNW()])
ifdef([GN_PROG_CC_OPTIMIZE], [GN_PROG_CC_OPTIMIZE(3)])
