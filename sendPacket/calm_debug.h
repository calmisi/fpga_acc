#ifndef _CALM_DEBUG_H_
#define _CALM_DEBUG_H_

#ifdef _DEBUG /* debug macros */
/* debug support */
//	#define DM(_fmt, ...) do {} while(0)
#define D(_fmt, ...)						\
	{							\
		struct timeval _t0;				\
		gettimeofday(&_t0, NULL);			\
		fprintf(stderr, "[%03d.%06d] [%s]--%s() [%d] " _fmt "\n",	\
		    (int)(_t0.tv_sec % 1000), (int)_t0.tv_usec,	\
		    __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);	\
        }

/* Rate limited version of "D", lps indicates how many per second */
#define RD(lps, format, ...)                                    \
    {                                                        \
        static int __t0, __cnt;                                 \
        struct timeval __xxts;                                  \
        gettimeofday(&__xxts, NULL);                            \
        if (__t0 != __xxts.tv_sec) {                            \
            __t0 = __xxts.tv_sec;                               \
            __cnt = 0;                                          \
        }                                                       \
        if (__cnt++ < lps) {                                    \
            D(format, ##__VA_ARGS__);                           \
        }                                                       \
    }
#else
#define D(_fmt, ...)
#endif



#endif
