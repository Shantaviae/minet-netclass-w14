#ifndef _shim
#define _shim

#include <iostream>
#include "util.h"

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define MAKESHIM(UPPERMSG,LOWERMSG) 				\
inline int RunShim(const char *uppername,			\
	    const char *lowername,				\
	    const char *fifotoupper,				\
	    const char *fifofromupper,				\
	    const char *fifotolower,				\
	    const char *fifofromlower)				\
{								\
  int fromupper, toupper, fromlower, tolower;			\
  								\
  fromlower=open(fifofromlower,O_RDONLY);			\
  tolower=open(fifotolower,O_WRONLY);				\
  toupper=open(fifotoupper,O_WRONLY);				\
  fromupper=open(fifofromupper,O_RDONLY);			\
  								\
  if (fromlower<0 || tolower<0 || fromupper<0 || toupper<0) { 	\
    close(fromlower);						\
    close(tolower);						\
    close(fromupper);						\
    close(toupper);						\
    return -1;					\
  }						\
						\
  cerr << uppername << " <-> " << lowername 	\
       << " shim functioning\n";		\
						\
  fd_set r;                                     \
  int maxfd;							\
  int rc;							\
  								\
  while (1) {							\
    maxfd=0;							\
    FD_ZERO(&r);						\
    FD_SET(fromupper,&r); 					\
    maxfd=MAX(maxfd,fromupper);					\
    FD_SET(fromlower,&r); 					\
    maxfd=MAX(maxfd,fromlower);					\
    								\
    rc=select(maxfd+1,&r,0,0,0);				\
								\
    if (rc<0) { 						\
      if (errno==EINTR) { 					\
	continue;						\
      } else {							\
	cerr << "Unknown error in select\n";			\
	return -1;						\
      }								\
    } else if (rc==0) { 					\
      cerr << "Unexpected timeout in select\n";			\
      return -1;						\
    } else {							\
      if (FD_ISSET(fromlower,&r)) { 				\
	LOWERMSG m;						\
	m.Unserialize(fromlower);				\
	cerr << lowername << " to " << uppername << " : ";	\
	cerr << m << endl;					\
	m.Serialize(toupper);					\
      }								\
      if (FD_ISSET(fromupper,&r)) { 				\
	UPPERMSG m;						\
	m.Unserialize(fromupper);				\
	cerr << uppername << " to " << lowername << " : ";	\
	cerr << m << endl;					\
	m.Serialize(tolower);					\
      }								\
    }								\
  }								\
  return 0;							\
}
   
#endif
