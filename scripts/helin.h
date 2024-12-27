#undef TARGET_HELIN
#define TARGET_HELIN 1

/* Default arguments you want when running your
   i686-myos-gcc/x86_64-myos-gcc toolchain */
#undef LIB_SPEC
#define LIB_SPEC "-lc -lm" /* link against C standard library */

/* Files that are linked before user code.
   The %s tells GCC to look for these files in the library directory. */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "crt0.o%s crtbegin.o%s"

/* Files that are linked after user code. */
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "crtend.o%s"

/* Additional predefined macros. */
#undef TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()      \
  do {                                \
    builtin_define ("__helin__");      \
    builtin_define ("__unix__");      \
    builtin_define("_POSIX_VERSION=20112L"); \
    builtin_assert ("system=helin");   \
    builtin_assert ("system=unix");   \
    builtin_assert ("system=posix");   \
  } while(0);

