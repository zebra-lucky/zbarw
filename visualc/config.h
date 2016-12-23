/* manually customized for Visual Studio */

/* Contrary to the autotools build system found on linux this file
   rather lists compiler/sdk/system capabilities, which zbar queries.

   Enable/Disable library features in "visualc\zbarw-config.props",
   preferrably via the VS IDE, unless you want to reload the solution
   solution file.
   */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H

/* Define to 1 if you have the <features.h> header file. */
#undef HAVE_FEATURES_H

/* Define if you have the iconv() function and it works. */
#undef HAVE_ICONV

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `jpeg' library (-ljpeg). */
#undef HAVE_LIBJPEG

/* Define to 1 if you have the <poll.h> header file. */
#undef HAVE_POLL_H

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#undef HAVE_SYS_IOCTL_H

/* Define to 1 if you have the <sys/mman.h> header file. */
#undef HAVE_SYS_MMAN_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#undef HAVE_SYS_TIME_H

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if the system has the type `uintptr_t'. */
#define HAVE_UINTPTR_T 1

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Define to 1 if you have the <vfw.h> header file. */
#define HAVE_VFW_H 1

/* Define as const if the declaration of iconv() needs const. */
#undef ICONV_CONST

/* Library major version */
#define LIB_VERSION_MAJOR 0

/* Library minor version */
#define LIB_VERSION_MINOR 10

/* Library revision */
#define LIB_VERSION_REVISION 0

/* Name of package */
#define PACKAGE "zbar"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "spadix@users.sourceforge.net"

/* Define to the full name of this package. */
#define PACKAGE_NAME "zbar"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "zbar 0.10"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "zbar"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.10"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "0.10"

/* Define to 1 if the X Window System is missing or not being used. */
#define X_DISPLAY_MISSING 1

/* Program major version (before the '.') as a number */
#define ZBAR_VERSION_MAJOR 0

/* Program minor version (after '.') as a number */
#define ZBAR_VERSION_MINOR 10
