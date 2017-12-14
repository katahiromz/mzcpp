#define FUNC(x)  (x) + 1
#define ABC 123
#define STRINGIFY(x) #x
#define XSTRINGIFY(x) STRINGIFY(x)
STRINGIFY(ABC)
XSTRINGIFY(ABC)
XSTRINGIFY
(ABC)
XSTRINGIFY(DEF)

#include <stdio.h>
