#define FUNC(x)  (x) + 1
#define ABC 123
#define STRINGIFY(x) #x
#define XSTRINGIFY(x) STRINGIFY(x)
STRINGIFY(ABC)
XSTRINGIFY(ABC)
XSTRINGIFY
(ABC)
XSTRINGIFY(DEF)

#ifdef __cplusplus
    #if __cplusplus >= 201402L
        C++14
    #elif __cplusplus >= 201103L
        C++11
    #else
        C++
    #endif
#endif

#define XXX 111
#define YYY XXX
#define ZZZ YYY + 1
#define WWW(x) x + XXX

#include <stdio.h>