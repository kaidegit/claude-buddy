#ifndef RTCONFIG_PROJECT_H__
#define RTCONFIG_PROJECT_H__

#if defined(_MSC_VER)
    #define RT_HEAP_SIZE (680000)
    #define NORESOURCE
    #define _CRT_ERRNO_DEFINED
    #define _INC_WTIME_INL
    #define _INC_TIME_INL
#endif

#endif
