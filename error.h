#ifndef _error
#define _error

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define STRINGIZE(T) #T

#define PERROR() \
do {\
fprintf(stderr,"%d: %s(%s:%d): %s\n",getpid(),__PRETTY_FUNCTION__,__FILE__,__LINE__,strerror(errno));\
} while (0)

/*
#define PERROR() do { extern char batman[1024]; sprintf(batman, "%s:%d %s",
  __FILE__, __LINE__, __PRETTY_FUNCTION__); perror(batman); } while(0)
*/


#endif
