#include <iostream>

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

#include "debug.h"
#include "config.h"
#include "arp.h"
#include "ethernet.h"
#include "raw_ethernet_packet.h"
#include "packet.h"
#include "util.h"

#define IP 1

void usage()
{
  cerr<<"arp_module myipadx myethernetadx\n";
}


int main(int argc, char *argv[])
{
  ARPCache cache;

  IPAddress ipaddr(MyIPAddr);
  EthernetAddr ethernetaddr(MyEthernetAddr);

  int muxin = open(mux2arp_fifo_name,O_RDONLY);
  int muxout = open(arp2mux_fifo_name,O_WRONLY);
#if IP
  int reqout = open(arp2ip_fifo_name,O_WRONLY);
  int reqin = open(ip2arp_fifo_name,O_RDONLY);
#endif

  if (muxin<0 || muxout<0) { 
    cerr << "arp_module: Can't communicate with mux ("
	 << mux2arp_fifo_name<<", "<<arp2mux_fifo_name<<"\n";
    exit(-1);
  }

#if IP
  if (reqin<0 || reqout<0) { 
    cerr << "arp_module: Can't communicate with IP ("
	 << ip2arp_fifo_name<<", "<<arp2ip_fifo_name<<"\n";
    exit(-1);
  }
#endif

  cache.Update(ARPRequestResponse(ipaddr,ethernetaddr,ARPRequestResponse::RESPONSE_OK));



  cerr << "arp_module: answering ARPs for " << ipaddr 
       << " with " << ethernetaddr <<"\n";

  fd_set read_fds;
  int rc;
  int maxfd;

  while (1) {
    FD_ZERO(&read_fds);
    FD_SET(muxin,&read_fds); maxfd=muxin;
#if IP    
    FD_SET(reqin,&read_fds); maxfd=MAX(maxfd,reqin);
#endif

    rc=select(maxfd+1,&read_fds,0,0,0);

    if (rc<0) {
      if (errno==EINTR) {
	continue;
      } else {
	perror("arp_module error in select");
	exit(-1);
      }
    } else if (rc==0) {
      perror("arp_module unexpected timeout");
      exit(-1);
    } else {
#if IP
      if (FD_ISSET(reqin,&read_fds)) {
	ARPRequestResponse r;
	r.Unserialize(reqin);
	cerr << "Local Request:  "<<r<<"\n";
	cache.Lookup(r);
	cerr << "Local Response: "<<r<<"\n";
	if (CanWriteNow(reqout)) { 
	  r.Serialize(reqout);
	} else {
	  DEBUGPRINTF(1,"arp_module: Dropped request packet\n");
	}
	if (r.flag==ARPRequestResponse::RESPONSE_UNKNOWN) { 
	  ARPPacket request(ARPPacket::Request,
			    ethernetaddr,
			    ipaddr,
			    ETHERNET_BLANK_ADDR,
			    r.ipaddr);
	  EthernetHeader h;
	  h.SetSrcAddr(ethernetaddr);
	  h.SetDestAddr(ETHERNET_BROADCAST_ADDR);
	  h.SetProtocolType(PROTO_ARP);
	  request.PushHeader(h);

	  RawEthernetPacket rawout(request);
	  rawout.Serialize(muxout);
	}
      }
#endif
      if (FD_ISSET(muxin,&read_fds)) {
	RawEthernetPacket rawpacket;
	rawpacket.Unserialize(muxin);
	ARPPacket arp(rawpacket);

	if (arp.IsIPToEthernet()) {
	  IPAddress sourceip;
	  EthernetAddr sourcehw;
	  
	  arp.GetSenderIPAddr(sourceip);
	  arp.GetSenderEthernetAddr(sourcehw);
	  
	  ARPRequestResponse r(sourceip,sourcehw,ARPRequestResponse::RESPONSE_OK);
	  cache.Update(r);

	  //cerr << cache << "\n";
	  
	  if (arp.IsIPToEthernetRequest()) {
	    IPAddress targetip;
	    arp.GetTargetIPAddr(targetip);
	    if (targetip==ipaddr) { 
	      // reply
	      
	      ARPPacket repl(ARPPacket::Reply,
			     ethernetaddr,
			     ipaddr,
			     sourcehw,
			     sourceip);
	      
	      EthernetHeader h;
	      h.SetSrcAddr(ethernetaddr);
	      h.SetDestAddr(sourcehw);
	      h.SetProtocolType(PROTO_ARP);
	      repl.PushHeader(h);
	      
	      RawEthernetPacket rawout(repl);
	      rawout.Serialize(muxout);
	      cerr << "Remote Request:  " << arp <<"\n";
	      cerr << "Remote Response: " << repl <<"\n";
	    }
	  }
	}
      } 
    }
  }
}


  
  

