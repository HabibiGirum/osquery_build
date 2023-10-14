#define WITHOUT_OPTIMIZATION 0
#define ENABLE_DEVEL 0
#define ENABLE_REFCNT_DEBUG 0

#define HAVE_ATOMICS_32 1
#define HAVE_ATOMICS_32_SYNC 0

#if (HAVE_ATOMICS_32)
# if (HAVE_ATOMICS_32_SYNC)
#  define ATOMIC_OP32(OP1,OP2,PTR,VAL) __sync_ ## OP1 ## _and_ ## OP2(PTR, VAL)
# else
#  define ATOMIC_OP32(OP1,OP2,PTR,VAL) __atomic_ ## OP1 ## _ ## OP2(PTR, VAL, __ATOMIC_SEQ_CST)
# endif
#endif

#define HAVE_ATOMICS_64 1
#define HAVE_ATOMICS_64_SYNC 0

#if (HAVE_ATOMICS_64)
# if (HAVE_ATOMICS_64_SYNC)
#  define ATOMIC_OP64(OP1,OP2,PTR,VAL) __sync_ ## OP1 ## _and_ ## OP2(PTR, VAL)
# else
#  define ATOMIC_OP64(OP1,OP2,PTR,VAL) __atomic_ ## OP1 ## _ ## OP2(PTR, VAL, __ATOMIC_SEQ_CST)
# endif
#endif

#define WITH_PKGCONFIG 1
#define WITH_HDRHISTOGRAM 1
#define WITH_ZLIB 1
#define WITH_ZSTD 1
#define WITH_LIBDL 1
#define WITH_PLUGINS 1
#define WITH_SNAPPY 1
#define WITH_SOCKEM 1
#define WITH_SSL 1
#define WITH_SASL 1
#define WITH_SASL_SCRAM 1
#define WITH_SASL_OAUTHBEARER 1
#define WITH_SASL_CYRUS 0
#define WITH_LZ4_EXT 0
#define HAVE_REGEX 1
#define HAVE_STRNDUP 1
#define HAVE_RAND_R 1
#define HAVE_PTHREAD_SETNAME_GNU 0
#define HAVE_PTHREAD_SETNAME_DARWIN 1
#define HAVE_PTHREAD_SETNAME_FREEBSD 0
#define WITH_C11THREADS 0
#define WITH_CRC32C_HW 1
#define SOLIB_EXT ".dylib"
#define BUILT_WITH  "CMAKE AppleClang AppleClang PKGCONFIG HDRHISTOGRAM ZLIB ZSTD LIBDL PLUGINS SSL SASL_SCRAM SASL_OAUTHBEARER CRC32C_HW SNAPPY SOCKEM"
