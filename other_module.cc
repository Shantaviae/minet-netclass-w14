#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#include <iostream>
#include "config.h"
#include "packet.h"
#include "ethernet.h"
#include "headertrailer.h"
#include "raw_ethernet_packet.h"

#define ETHER_MUX 1


int main(int argc, char *argv[])
{
#if ETHER_MUX
  int fromethermux, toethermux;
#endif

#if ETHER_MUX
  fromethermux=open(mux2other_fifo_name,O_RDONLY);
  toethermux=open(other2mux_fifo_name,O_WRONLY);
#endif

#if ETHER_MUX
  if (toethermux<0 || fromethermux<0) {
    cerr << "Can't open connection to ethernet mux\n";
    return -1;
  }
#endif

  cerr << "other_module: handling OTHER traffic\n";

  int maxfd=0;
  fd_set read_fds;
  int rc;

  while (1) {
    maxfd=0;
    FD_ZERO(&read_fds);
#if ETHER_MUX
    FD_SET(fromethermux,&read_fds); maxfd=MAX(maxfd,fromethermux);
#endif

    rc=select(maxfd+1,&read_fds,0,0,0);

    if (rc<0) { 
      if (errno==EINTR) { 
	continue;
      } else {
	cerr << "Unknown error in select\n";
	return -1;
      }
    } else if (rc==0) { 
      cerr << "Unexpected timeout in select\n";
      return -1;
    } else {
#if ETHER_MUX
      if (FD_ISSET(fromethermux,&read_fds)) { 
	RawEthernetPacket raw;
	raw.Unserialize(fromethermux);
      }
#endif
    }
  }
  return 0;
}
