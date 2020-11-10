dnl additional configure macros

dnl this one stolen from GnuPG
######################################################################
# Check whether mlock is broken (hpux 10.20 raises a SIGBUS if mlock
# is not called from uid 0 (not tested whether uid 0 works)
######################################################################
dnl GNUPG_CHECK_MLOCK
dnl
define(GNUPG_CHECK_MLOCK,
  [ AC_CHECK_FUNCS(mlock)
    if test "$ac_cv_func_mlock" = "yes"; then
        AC_MSG_CHECKING(whether mlock is broken)
          AC_CACHE_VAL(gnupg_cv_have_broken_mlock,
             AC_TRY_RUN([
                #include <stdlib.h>
                #include <unistd.h>
                #include <errno.h>
                #include <sys/mman.h>
                #include <sys/types.h>
                #include <fcntl.h>

                int main()
                {
                    char *pool;
                    int err;
                    long int pgsize = getpagesize();

                    pool = malloc( 4096 + pgsize );
                    if( !pool )
                        return 2;
                    pool += (pgsize - ((long int)pool % pgsize));

                    err = mlock( pool, 4096 );
                    if( !err || errno == EPERM )
                        return 0; /* okay */

                    return 1;  /* hmmm */
                }

            ],
            gnupg_cv_have_broken_mlock="no",
            gnupg_cv_have_broken_mlock="yes",
            gnupg_cv_have_broken_mlock="assume-no"
           )
         )
         if test "$gnupg_cv_have_broken_mlock" = "yes"; then
             AC_DEFINE(HAVE_BROKEN_MLOCK, [],
	       [Define as 1 if your mlock fails to return EPERM.])
             AC_MSG_RESULT(yes)
         else
            if test "$gnupg_cv_have_broken_mlock" = "no"; then
                AC_MSG_RESULT(no)
            else
                AC_MSG_RESULT(assuming no)
            fi
         fi
    fi
  ])

