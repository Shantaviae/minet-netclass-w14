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
#include "ip.h"
#include "arp.h"
#include "udp.h"
#include "tcp.h"
#include "packet.h"
#include "ethernet.h"
#include "raw_ethernet_packet.h"
#include "sockint.h"

#define IP_MUX    1
#define SOCK      1


int main(int argc, char *argv[])
{
#if IP_MUX
  int fromipmux, toipmux;
#endif
#if SOCK
  int fromsock, tosock;
#endif

#if IP_MUX
  fromipmux=open(ipmux2tcp_fifo_name,O_RDONLY);
  toipmux=open(tcp2ipmux_fifo_name,O_WRONLY);
#endif
#if SOCK
  if (getenv("MINET_SHIMS") &&
      strstr(getenv("MINET_SHIMS"),"tcp_module+sock_module")) {
    tosock=open((string(tcp2sock_fifo_name)+string("_shim")).c_str(),O_WRONLY);
    fromsock=open((string(sock2tcp_fifo_name)+string("_shim")).c_str(),O_RDONLY);
  } else {
    tosock=open(tcp2sock_fifo_name,O_WRONLY);
    fromsock=open(sock2tcp_fifo_name,O_RDONLY);
  }
#endif

#if IP_MUX
  if (toipmux<0 || fromipmux<0) {
    cerr << "Can't open connection to IP mux\n";
    return -1;
  }
#endif

#if SOCK
  if (tosock<0 || fromsock<0) {
    cerr << "Can't open connection to SOCK module\n";
    return -1;
  }
#endif

  cerr << "tcp_module handling TCP traffic\n";

  int maxfd=0;
  fd_set read_fds;
  int rc;

  while (1) {
    maxfd=0;
    FD_ZERO(&read_fds);
#if IP_MUX
    FD_SET(fromipmux,&read_fds); maxfd=MAX(maxfd,fromipmux);
#endif
#if SOCK
    FD_SET(fromsock,&read_fds); maxfd=MAX(maxfd,fromsock);
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
#if IP_MUX
      if (FD_ISSET(fromipmux,&read_fds)) { 
	Packet p;
	p.Unserialize(fromipmux);
	unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
	cerr << "estimated header len="<<tcphlen<<"\n";
	p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
	IPHeader ipl=p.FindHeader(Headers::IPHeader);
	TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

	cerr << "TCP Packet: IP Header is "<<ipl<<" and ";
	cerr << "TCP Header is "<<tcph << " and ";

	cerr << "Checksum is " << (tcph.IsCorrectChecksum(p) ? "VALID" : "INVALID");
	
      }
#endif
#if SOCK
      if (FD_ISSET(fromsock,&read_fds)) {
	SockRequestResponse s;
	s.Unserialize(fromsock);
	cerr << "Received Socket Request:" << s << endl;
      }
#endif
    }
  }
  return 0;
}
