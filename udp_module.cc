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

#include <string.h>
#include <iostream>
#include "config.h"
#include "ip.h"
#include "arp.h"
#include "udp.h"
#include "packet.h"
#include "ethernet.h"
#include "raw_ethernet_packet.h"
#include "constate.h"

#define IP_MUX    1
#define SOCK      1


#define ECHO      0


struct UDPState {
  ostream & Print(ostream &os) const { os <<"UDPState()"; return os;}
};


int main(int argc, char *argv[])
{
#if IP_MUX
  int fromipmux, toipmux;
#endif
#if SOCK
  int fromsock, tosock;
#endif

  ConnectionList<UDPState> clist;

#if IP_MUX
  fromipmux=open(ipmux2udp_fifo_name,O_RDONLY);
  toipmux=open(udp2ipmux_fifo_name,O_WRONLY);
#endif
#if SOCK
  if (getenv("MINET_SHIMS") &&
      strstr(getenv("MINET_SHIMS"),"udp_module+sock_module")) {
    tosock=open((string(udp2sock_fifo_name)+string("_shim")).c_str(),O_WRONLY);
    fromsock=open((string(sock2udp_fifo_name)+string("_shim")).c_str(),O_RDONLY);
  } else {
    tosock=open(udp2sock_fifo_name,O_WRONLY);
    fromsock=open(sock2udp_fifo_name,O_RDONLY);
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

  cerr << "udp_module handling UDP traffic\n";

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
	unsigned short len;
	bool checksumok;
	p.Unserialize(fromipmux);
	p.ExtractHeaderFromPayload<UDPHeader>(8);
	UDPHeader udph;
	udph=p.FindHeader(Headers::UDPHeader);
	checksumok=udph.IsCorrectChecksum(p);
	IPHeader iph;
	iph=p.FindHeader(Headers::IPHeader);
	Connection c;
	// note that this is flipped around because
	// "source" is interepreted as "this machine"
	iph.GetDestIP(c.src);
	iph.GetSourceIP(c.dest);
	iph.GetProtocol(c.protocol);
	udph.GetDestPort(c.srcport);
	udph.GetSourcePort(c.destport);
	ConnectionList<UDPState>::iterator cs = clist.FindMatching(c);
	if (cs!=clist.end()) {
	  udph.GetLength(len);
	  len-=UDP_HEADER_LENGTH;
	  Buffer &data = p.GetPayload().ExtractFront(len);
	  SockRequestResponse write(WRITE,
				    (*cs).connection,
				    data,
				    len,
				    EOK);
	  if (!checksumok) {
	    cerr << "forwarding packet to sock even though checksum failed.";
	  }
	  write.Serialize(tosock);
	}
      }
#endif
#if SOCK
      if (FD_ISSET(fromsock,&read_fds)) {
	SockRequestResponse req;
	req.Unserialize(fromsock);
	switch (req.type) {
	  // case SockRequestResponse::CONNECT: 
	case CONNECT: 
	  // case SockRequestResponse::ACCEPT: 
	case ACCEPT: 
	  { // ignored, send OK response
	    SockRequestResponse repl;
	    // repl.type=SockRequestResponse::STATUS;
	    repl.type=STATUS;
	    repl.connection=req.connection;
	    // buffer is zero bytes
	    repl.bytes=0;
	    repl.error=EOK;
	    repl.Serialize(tosock);
	  }
	  break;
	  // case SockRequestResponse::STATUS: 
	case STATUS: 
	  // ignored, no response needed
	  break;
	  // case SockRequestResponse::WRITE: 
	case WRITE: 
	  {
	    unsigned bytes = MIN(UDP_MAX_DATA, req.data.GetSize());
	    // create the payload of the packet
	    Packet p(req.data.ExtractFront(bytes));
	    // Make the IP header first since we need it to do the udp checksum
	    IPHeader ih;
	    ih.SetProtocol(IP_PROTO_UDP);
	    ih.SetSourceIP(req.connection.src);
	    ih.SetDestIP(req.connection.dest);
	    ih.SetTotalLength(bytes+UDP_HEADER_LENGTH+IP_HEADER_BASE_LENGTH);
	    // push it onto the packet
	    p.PushFrontHeader(ih);
	    // Now build the UDP header
	    // notice that we pass along the packet so that the udpheader can find
	    // the ip header because it will include some of its fields in the checksum
	    UDPHeader uh;
	    uh.SetSourcePort(req.connection.srcport,p);
	    uh.SetDestPort(req.connection.destport,p);
	    uh.SetLength(UDP_HEADER_LENGTH+bytes,p);
	    // Now we want to have the udp header BEHIND the IP header
	    p.PushBackHeader(uh);
	    p.Serialize(toipmux);
	    SockRequestResponse repl;
	    // repl.type=SockRequestResponse::STATUS;
	    repl.type=STATUS;
	    repl.connection=req.connection;
	    repl.bytes=bytes;
	    repl.error=EOK;
	    repl.Serialize(tosock);
	  }
	  break;
	  // case SockRequestResponse::FORWARD:
	case FORWARD:
	  {
	    ConnectionToStateMapping<UDPState> m;
	    m.connection=req.connection;
	    // remove any old forward that might be there.
	    ConnectionList<UDPState>::iterator cs = clist.FindMatching(req.connection);
	    if (cs!=clist.end()) { 
	      clist.erase(cs);
	    }
	    clist.push_back(m);
	    SockRequestResponse repl;
	    // repl.type=SockRequestResponse::STATUS;
	    repl.type=STATUS;
	    repl.connection=req.connection;
	    repl.error=EOK;
	    repl.bytes=0;
	    repl.Serialize(tosock);
	  }
	  break;
	  // case SockRequestResponse::CLOSE:
	case CLOSE:
	  {
	    ConnectionList<UDPState>::iterator cs = clist.FindMatching(req.connection);
	    SockRequestResponse repl;
	    repl.connection=req.connection;
	    // repl.type=SockRequestResponse::STATUS;
	    repl.type=STATUS;
	    if (cs==clist.end()) {
	      repl.error=ENOMATCH;
	    } else {
	      repl.error=EOK;
	      clist.erase(cs);
	    }
	    repl.Serialize(tosock);
	  }
	  break;
	default:
	  {
	    SockRequestResponse repl;
	    // repl.type=SockRequestResponse::STATUS;
	    repl.type=STATUS;
	    repl.error=EWHAT;
	    repl.Serialize(tosock);
	  }
	}
      }
#endif
    }
  }
  return 0;
}
