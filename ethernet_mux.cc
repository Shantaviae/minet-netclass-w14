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
#include "ethernet.h"
#include "raw_ethernet_packet.h"

#define ARP   1
#define IP    1
#define OTHER 1


int main(int argc, char * argv[])
{
  int etherin,etherout, arpin,arpout, ipin,ipout, otherin, otherout;

  etherin=open(ether2mux_fifo_name,O_RDONLY);
  etherout=open(mux2ether_fifo_name,O_WRONLY);
  //etherout=open("fifos/test",O_WRONLY);
#if ARP
  arpout=open(mux2arp_fifo_name,O_WRONLY);
  arpin=open(arp2mux_fifo_name,O_RDONLY);
#endif
#if IP
  ipout=open(mux2ip_fifo_name,O_WRONLY);
  ipin=open(ip2mux_fifo_name,O_RDONLY);
#endif
#if OTHER
  otherout=open(mux2other_fifo_name,O_WRONLY);
  otherin=open(other2mux_fifo_name,O_RDONLY);
#endif

  if (etherin<0 || etherout<0) {
    cerr << "Can't connect to ethernet device driver ("
	 << ether2mux_fifo_name<<","<< mux2ether_fifo_name<<"\n";
    exit(-1);
  }
#if ARP
  if (arpin<0 || arpout<0) {
    cerr << "Can't connect to ARP module ("
	 << arp2mux_fifo_name<<","<< mux2arp_fifo_name<<"\n";
    exit(-1);
  }
#endif
#if IP
  if (ipin<0 || ipout<0) {
    cerr << "Can't connect to IP module ("
	 << arp2mux_fifo_name<<","<< mux2arp_fifo_name<<"\n";
    exit(-1);
  }
#endif
#if OTHER
  if (otherin<0 || otherout<0) {
    cerr << "Can't connect to other module ("
	 << other2mux_fifo_name<<","<< mux2other_fifo_name<<"\n";
    exit(-1);
  }
#endif


  cerr << "ethernet_mux operating\n";

  fd_set read_fds;
  int rc;
  int maxfd;

  while (1) {
    FD_ZERO(&read_fds);
    FD_SET(etherin,&read_fds);  maxfd=etherin;
#if ARP
    FD_SET(arpin,&read_fds); maxfd=MAX(maxfd,arpin);
#endif
#if IP
    FD_SET(ipin,&read_fds); maxfd=MAX(maxfd,ipin);
#endif
#if OTHER
    FD_SET(otherin,&read_fds); maxfd=MAX(maxfd,otherin);
#endif

    rc=select(maxfd+1,&read_fds,0,0,0);

    if (rc<0) {
      if (errno==EINTR) {
	continue;
      } else {
	perror("ethernet_mux error in select");
	exit(-1);
      }
    } else if (rc==0) {
      perror("ethernet_mux unexpected timeout");
      exit(-1);
    } else {
      if (FD_ISSET(etherin,&read_fds)) {
	RawEthernetPacket raw;
	unsigned short type;
	raw.Unserialize(etherin);
	memcpy((char*)(&type),&(raw.data[12]),2);
	type=ntohs(type);
	switch (type) {
	case PROTO_ARP:
	  if (ARP && CanWriteNow(arpout)) {
	    raw.Serialize(arpout);
	  } else {
	    DEBUGPRINTF(1,"ethernet_mux: Dropped incoming ARP packet\n");
	  }
	  break;
	case PROTO_IP:
	  if (IP && CanWriteNow(ipout)) { 
	    raw.Serialize(ipout);
	  } else {
	    DEBUGPRINTF(1,"ethernet_mux: Dropped incoming IP packet\n");
	  }
	  break;
	default:
	  if (OTHER && CanWriteNow(otherout)) { 
	    raw.Serialize(otherout);
	  } else {
	    DEBUGPRINTF(1,"ethernet_mux: Dropped incoming OTHER packet\n");
	  }
	}
      }
#if ARP
      if (FD_ISSET(arpin,&read_fds)) {
	RawEthernetPacket p;
	p.Unserialize(arpin);
	if (CanWriteNow(etherout)) { 
	  cerr << "Writing out ARP Packet: " << p <<"\n";
	  p.Serialize(etherout);
	} else {
	  DEBUGPRINTF(1,"ethernet_mux: Dropped outgoing ARP packet\n");
	}
      }
#endif
#if IP
      if (FD_ISSET(ipin,&read_fds)) {
	RawEthernetPacket p;
	p.Unserialize(ipin);
	if (CanWriteNow(etherout)) { 
	  p.Serialize(etherout);
	} else {
	  DEBUGPRINTF(1,"ethernet_mux: Dropped outgoing IP packet\n");
	}
      }
#endif
#if OTHER
      if (FD_ISSET(otherin,&read_fds)) {
	RawEthernetPacket p;
	p.Unserialize(otherin);
	if (CanWriteNow(etherout)) { 
	  p.Serialize(etherout);
	} else {
	  DEBUGPRINTF(1,"ethernet_mux: Dropped outgoing OTHER packet\n");
	}
      }
#endif
    }
  }
}





