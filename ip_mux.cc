#include <sys/time.h>
#include <sys/types.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <unistd.h>
#include "netinet/in.h"

#include <iostream>

#include "debug.h"
#include "config.h"
#include "packet.h"
#include "ip.h"

#define UDP   1
#define TCP   1
#define ICMP  0
#define OTHER 0


int main(int argc, char * argv[])
{
  int ipin, ipout;
  int udpin, udpout, tcpin, tcpout, icmpin, icmpout, otherin, otherout;

  ipin=open(ip2ipmux_fifo_name,O_RDONLY);
  ipout=open(ipmux2ip_fifo_name,O_WRONLY);

#if UDP
  if (getenv("MINET_SHIMS") &&
      strstr(getenv("MINET_SHIMS"),"ip_mux+udp_module")) {
    udpout=open((string(ipmux2udp_fifo_name)+string("_shim")).c_str(),O_WRONLY);
    udpin=open((string(udp2ipmux_fifo_name)+string("_shim")).c_str(),O_RDONLY);
  } else {
    udpout=open(ipmux2udp_fifo_name,O_WRONLY);
    udpin=open(udp2ipmux_fifo_name,O_RDONLY);
  }
#endif
#if TCP
  if (getenv("MINET_SHIMS") &&
      strstr(getenv("MINET_SHIMS"),"ip_mux+tcp_module")) {
    tcpout=open((string(ipmux2tcp_fifo_name)+string("_shim")).c_str(),O_WRONLY);
    tcpin=open((string(tcp2ipmux_fifo_name)+string("_shim")).c_str(),O_RDONLY);
  } else {
    tcpout=open(ipmux2tcp_fifo_name,O_WRONLY);
    tcpin=open(tcp2ipmux_fifo_name,O_RDONLY);
  }
#endif
#if ICMP
  icmpout=open(ipmux2icmp_fifo_name,O_WRONLY);
  icmpin=open(icmp2ipmux_fifo_name,O_RDONLY);
#endif
#if OTHER
  otherout=open(ipmux2other_fifo_name,O_WRONLY);
  otherin=open(other2ipmux_fifo_name,O_RDONLY);
#endif

  if (ipin<0 || ipout<0) {
    cerr << "Can't connect to IP module ("
	 << ip2ipmux_fifo_name<<","<< ipmux2ip_fifo_name<<"\n";
    exit(-1);
  }

#if UDP
  if (udpin<0 || udpout<0) {
    cerr << "Can't connect to UDP module ("
	 << udp2ipmux_fifo_name<<","<< ipmux2udp_fifo_name<<"\n";
    exit(-1);
  }
#endif

#if TCP
  if (tcpin<0 || tcpout<0) {
    cerr << "Can't connect to TCP module ("
	 << tcp2ipmux_fifo_name<<","<< ipmux2tcp_fifo_name<<"\n";
    exit(-1);
  }
#endif

#if ICMP
  if (icmpin<0 || icmpout<0) {
    cerr << "Can't connect to ICMP module ("
	 << icmp2ipmux_fifo_name<<","<< ipmux2icmp_fifo_name<<"\n";
    exit(-1);
  }
#endif

#if OTHER
  if (otherin<0 || otherout<0) {
    cerr << "Can't connect to OTHER module ("
	 << other2ipmux_fifo_name<<","<< ipmux2other_fifo_name<<"\n";
    exit(-1);
  }
#endif


  cerr << "ip_mux operating\n";

  fd_set read_fds;
  int rc;
  int maxfd;

  while (1) {
    FD_ZERO(&read_fds);
    FD_SET(ipin,&read_fds);  maxfd=ipin;
#if UDP
    FD_SET(udpin,&read_fds); maxfd=MAX(maxfd,udpin);
#endif
#if TCP
    FD_SET(tcpin,&read_fds); maxfd=MAX(maxfd,tcpin);
#endif
#if ICMP
    FD_SET(icmpin,&read_fds); maxfd=MAX(maxfd,icmpin);
#endif
#if OTHER
    FD_SET(otherin,&read_fds); maxfd=MAX(maxfd,otherin);
#endif

    rc=select(maxfd+1,&read_fds,0,0,0);

    if (rc<0) {
      if (errno==EINTR) {
	continue;
      } else {
	perror("ip_mux error in select");
	exit(-1);
      }
    } else if (rc==0) {
      perror("ip_mux unexpected timeout");
      exit(-1);
    } else {
      if (FD_ISSET(ipin,&read_fds)) {
	Packet p;
	unsigned char proto;
	p.Unserialize(ipin);
	IPHeader iph=p.FindHeader(Headers::IPHeader);
	iph.GetProtocol(proto);
	switch (proto) {
#if UDP
	case IP_PROTO_UDP:
	  if (CanWriteNow(udpout)) {
	    p.Serialize(udpout);
	  } else {
	    cerr << "Discarded incoming UDP packet\n";
	  }
	  break;
#endif
#if TCP
	case IP_PROTO_TCP:
	  if (CanWriteNow(tcpout)) {
	    p.Serialize(tcpout);
	  } else {
	    cerr << "Discarding incoming TCP packet\n";
	  }
	  break;
#endif
#if ICMP
	case IP_PROTO_ICMP:
	  if (CanWriteNow(icmpout)) {
	    p.Serialize(icmpout);
	  } else {
	    cerr << "Discarding incoming ICMP packet\n";
	  }
	  break;
#endif
	default:
#if OTHER
	  if (CanWriteNow(otherout)) { 
	    p.Serialize(otherout);
	  } else {
	    cerr << "Discarding incoming IP packet of protocol "<<(int)proto<<"\n";
	  }
#else
	  cerr << "Discarding incoming IP Packet of protocol "<<(int)proto<<"\n";
#endif
	  break;
	}
      }
#if UDP
      if (FD_ISSET(udpin,&read_fds)) {
	Packet p;
	p.Unserialize(udpin);
	if (CanWriteNow(ipout)) { 
	  p.Serialize(ipout);
	} else {
	  DEBUGPRINTF(1,"ip_mux: Dropped outgoing UDP packet\n");
	}
      }
#endif
#if TCP
      if (FD_ISSET(tcpin,&read_fds)) {
	Packet p;
	p.Unserialize(tcpin);
	if (CanWriteNow(ipout)) { 
	  p.Serialize(ipout);
	} else {
	  DEBUGPRINTF(1,"ip_mux: Dropped outgoing TCP packet\n");
	}
      }
#endif
#if ICMP
      if (FD_ISSET(icmpin,&read_fds)) {
	Packet p;
	p.Unserialize(icmpin);
	if (CanWriteNow(ipout)) { 
	  p.Serialize(ipout);
	} else {
	  DEBUGPRINTF(1,"ip_mux: Dropped outgoing ICMP packet\n");
	}
      }
#endif
#if OTHER
      if (FD_ISSET(otherin,&read_fds)) {
	Packet p;
	p.Unserialize(otherin);
	if (CanWriteNow(ipout)) { 
	  p.Serialize(ipout);
	} else {
	  DEBUGPRINTF(1,"ip_mux: Dropped outgoing OTHER packet\n");
	}
      }
#endif
    }
  }
}





