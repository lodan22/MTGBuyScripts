/* glibconfig.h
 *
 * This is a generated file.  Please modify 'glibconfig.h.in'
 */

#ifndef __GLIBCONFIG_H__
#define __GLIBCONFIG_H__

#include <glib/gmacros.h>

#include <limits.h>
#include <float.h>
/* #undef GLIB_HAVE_ALLOCA_H */

/* Specifies that GLib's g_print*() functions wrap the
 * system printf functions.  This is useful to know, for example,
 * when using glibc's register_printf_function().
 */
#undef GLIB_USING_SYSTEM_PRINTF

/* #undef GLIB_STATIC_COMPILATION */
/* #undef GOBJECT_STATIC_COMPILATION */

G_BEGIN_DECLS

#define G_MINFLOAT	FLT_MIN
#define G_MAXFLOAT	FLT_MAX
#define G_MINDOUBLE	DBL_MIN
#define G_MAXDOUBLE	DBL_MAX
#define G_MINSHORT	SHRT_MIN
#define G_MAXSHORT	SHRT_MAX
#define G_MAXUSHORT	USHRT_MAX
#define G_MININT	INT_MIN
#define G_MAXINT	INT_MAX
#define G_MAXUINT	UINT_MAX
#define G_MINLONG	LONG_MIN
#define G_MAXLONG	LONG_MAX
#define G_MAXULONG	ULONG_MAX

typedef signed char gint8;
typedef unsigned char guint8;

typedef signed short gint16;
typedef unsigned short guint16;

#define G_GINT16_MODIFIER "h"
#define G_GINT16_FORMAT "hi"
#define G_GUINT16_FORMAT "hu"


typedef signed int gint32;
typedef unsigned int guint32;

#define G_GINT32_MODIFIER ""
#define G_GINT32_FORMAT "i"
#define G_GUINT32_FORMAT "u"


#define G_HAVE_GINT64 1          /* deprecated, always true */

G_GNUC_EXTENSION typedef signed long long gint64;
G_GNUC_EXTENSION typedef unsigned long long guint64;

#define G_GINT64_CONSTANT(val)	(G_GNUC_EXTENSION (val##LL))
#define G_GUINT64_CONSTANT(val)	(G_GNUC_EXTENSION (val##ULL))

#define G_GINT64_MODIFIER "ll"
#define G_GINT64_FORMAT "lli"
#define G_GUINT64_FORMAT "llu"


#define GLIB_SIZEOF_VOID_P 8
#define GLIB_SIZEOF_LONG   4
#define GLIB_SIZEOF_SIZE_T 8
#define GLIB_SIZEOF_SSIZE_T 8

typedef signed long long gssize;
typedef unsigned long long gsize;
#define G_GSIZE_MODIFIER "ll"
#define G_GSSIZE_MODIFIER "ll"
#define G_GSIZE_FORMAT "llu"
#define G_GSSIZE_FORMAT "lli"

#define G_MAXSIZE	G_MAXUINT64
#define G_MINSSIZE	G_MININT64
#define G_MAXSSIZE	G_MAXINT64

typedef gint64 goffset;
#define G_MINOFFSET	G_MININT64
#define G_MAXOFFSET	G_MAXINT64

#define G_GOFFSET_MODIFIER      G_GINT64_MODIFIER
#define G_GOFFSET_FORMAT        G_GINT64_FORMAT
#define G_GOFFSET_CONSTANT(val) G_GINT64_CONSTANT(val)

#define G_POLLFD_FORMAT "%#llx"

#define GPOINTER_TO_INT(p)	((gint)  (gint64) (p))
#define GPOINTER_TO_UINT(p)	((guint) (guint64) (p))

#define GINT_TO_POINTER(i)	((gpointer) (gint64) (i))
#define GUINT_TO_POINTER(u)	((gpointer) (guint64) (u))

typedef signed long long gintptr;
typedef unsigned long long guintptr;

#define G_GINTPTR_MODIFIER      "ll"
#define G_GINTPTR_FORMAT        "lli"
#define G_GUINTPTR_FORMAT       "llu"

#define GLIB_MAJOR_VERSION 2
#define GLIB_MINOR_VERSION 68
#define GLIB_MICRO_VERSION 3

#define G_OS_WIN32
#define G_PLATFORM_WIN32

#define G_VA_COPY va_copy


#ifndef __cplusplus
# define G_HAVE_ISO_VARARGS 1
#endif

#ifdef __cplusplus
# define G_HAVE_ISO_VARARGS 1
#endif

/* gcc-2.95.x supports both gnu style and ISO varargs, but if -ansi
 * is passed ISO vararg support is turned off, and there is no work
 * around to turn it on, so we unconditionally turn it off.
 */
#if __GNUC__ == 2 && __GNUC_MINOR__ == 95
#  undef G_HAVE_ISO_VARARGS
#endif

#define G_HAVE_GROWING_STACK 0
#define G_HAVE_GNUC_VISIBILITY 1

#ifndef _MSC_VER
# define G_HAVE_GNUC_VARARGS 1
#endif

#if defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define G_GNUC_INTERNAL __attribute__((visibility("hidden")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
#define G_GNUC_INTERNAL __hidden
#elif defined (__GNUC__) && defined (G_HAVE_GNUC_VISIBILITY)
#define G_GNUC_INTERNAL __attribute__((visibility("hidden")))
#else
#define G_GNUC_INTERNAL
#endif

#define G_THREADS_ENABLED
#define G_THREADS_IMPL_WIN32

#define G_ATOMIC_LOCK_FREE

#define GINT16_TO_LE(val)	((gint16) (val))
#define GUINT16_TO_LE(val)	((guint16) (val))
#define GINT16_TO_BE(val)	((gint16) GUINT16_SWAP_LE_BE (val))
#define GUINT16_TO_BE(val)	(GUINT16_SWAP_LE_BE (val))

#define GINT32_TO_LE(val)	((gint32) (val))
#define GUINT32_TO_LE(val)	((guint32) (val))
#define GINT32_TO_BE(val)	((gint32) GUINT32_SWAP_LE_BE (val))
#define GUINT32_TO_BE(val)	(GUINT32_SWAP_LE_BE (val))

#define GINT64_TO_LE(val)	((gint64) (val))
#define GUINT64_TO_LE(val)	((guint64) (val))
#define GINT64_TO_BE(val)	((gint64) GUINT64_SWAP_LE_BE (val))
#define GUINT64_TO_BE(val)	(GUINT64_SWAP_LE_BE (val))

#define GLONG_TO_LE(val)	((glong) GINT32_TO_LE (val))
#define GULONG_TO_LE(val)	((gulong) GUINT32_TO_LE (val))
#define GLONG_TO_BE(val)	((glong) GINT32_TO_BE (val))
#define GULONG_TO_BE(val)	((gulong) GUINT32_TO_BE (val))
#define GINT_TO_LE(val)		((gint) GINT32_TO_LE (val))
#define GUINT_TO_LE(val)	((guint) GUINT32_TO_LE (val))
#define GINT_TO_BE(val)		((gint) GINT32_TO_BE (val))
#define GUINT_TO_BE(val)	((guint) GUINT32_TO_BE (val))
#define GSIZE_TO_LE(val)	((gsize) GUINT64_TO_LE (val))
#define GSSIZE_TO_LE(val)	((gssize) GINT64_TO_LE (val))
#define GSIZE_TO_BE(val)	((gsize) GUINT64_TO_BE (val))
#define GSSIZE_TO_BE(val)	((gssize) GINT64_TO_BE (val))
#define G_BYTE_ORDER G_LITTLE_ENDIAN

#define GLIB_SYSDEF_POLLIN =1
#define GLIB_SYSDEF_POLLOUT =4
#define GLIB_SYSDEF_POLLPRI =2
#define GLIB_SYSDEF_POLLHUP =16
#define GLIB_SYSDEF_POLLERR =8
#define GLIB_SYSDEF_POLLNVAL =32

#define G_MODULE_SUFFIX "dll"

typedef void* GPid;
#define G_PID_FORMAT "p"

#define GLIB_SYSDEF_AF_UNIX 1
#define GLIB_SYSDEF_AF_INET 2
#define GLIB_SYSDEF_AF_INET6 23

#define GLIB_SYSDEF_MSG_OOB 1
#define GLIB_SYSDEF_MSG_PEEK 2
#define GLIB_SYSDEF_MSG_DONTROUTE 4

#define G_DIR_SEPARATOR '\\'
#define G_DIR_SEPARATOR_S "\\"
#define G_SEARCHPATH_SEPARATOR ';'
#define G_SEARCHPATH_SEPARATOR_S ";"

G_END_DECLS

#endif /* __GLIBCONFIG_H__ */
