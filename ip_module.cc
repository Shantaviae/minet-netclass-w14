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
#include "packet.h"
#include "ethernet.h"
#include "raw_ethernet_packet.h"
#include "route.h"

#define ETHER_MUX 1
#define IP_MUX    1
#define ARP       1


int main(int argc, char *argv[])
{
#if ETHER_MUX
  int fromethermux, toethermux;
#endif
#if IP_MUX
  int fromipmux, toipmux;
#endif
#if ARP
  int fromarp, toarp;
#endif

#if ETHER_MUX
  fromethermux=open(mux2ip_fifo_name,O_RDONLY);
  toethermux=open(ip2mux_fifo_name,O_WRONLY);
#endif
#if IP_MUX
  toipmux=open(ip2ipmux_fifo_name,O_WRONLY);
  fromipmux=open(ipmux2ip_fifo_name,O_RDONLY);
#endif
#if ARP
  toarp=open(ip2arp_fifo_name,O_WRONLY);
  fromarp=open(arp2ip_fifo_name,O_RDONLY);
#endif

#if ETHER_MUX
  if (toethermux<0 || fromethermux<0) {
    cerr << "Can't open connection to ethernet mux\n";
    return -1;
  }
#endif

#if IP_MUX
  if (toipmux<0 || fromipmux<0) {
    cerr << "Can't open connection to IP mux\n";
    return -1;
  }
#endif

#if ARP
  if (toarp<0 || fromarp<0) {
    cerr << "Can't open connection to ARP module\n";
    return -1;
  }
#endif

  cerr << "ip_module handling IP traffic...\n";

  int maxfd=0;
  fd_set read_fds;
  int rc;

  while (1) {
    maxfd=0;
    FD_ZERO(&read_fds);
#if ETHER_MUX
    FD_SET(fromethermux,&read_fds); maxfd=MAX(maxfd,fromethermux);
#endif
#if IP_MUX
    FD_SET(fromipmux,&read_fds); maxfd=MAX(maxfd,fromipmux);
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
	Packet p(raw);
	p.ExtractHeaderFromPayload<EthernetHeader>(ETHERNET_HEADER_LEN);
	p.ExtractHeaderFromPayload<IPHeader>(IPHeader::EstimateIPHeaderLength(p));
	IPHeader iph;
	iph=p.FindHeader(Headers::IPHeader);
	EthernetHeader eh;
	eh = p.FindHeader(Headers::EthernetHeader);

	IPAddress toip;
	iph.GetDestIP(toip);
	if (toip==MyIPAddr || toip==IPAddress(IP_ADDRESS_BROADCAST)) {
	  if (!(iph.IsChecksumCorrect())) {
	    cerr << "Discarding following packet because header checksum is wrong: "<<p<<"\n";
	    continue;
	  }
	  unsigned short fragoff;
	  unsigned char flags;
	  iph.GetFlags(flags);
	  iph.GetFragOffset(fragoff);
	  if ((flags&IP_HEADER_FLAG_MOREFRAG) || (fragoff!=0)) { 
	    cerr << "Discarding following packet because it is a fragment: "<<p<<"\n";
	    cerr << "NOTE: NO ICMP PACKET WAS SENT BACK\n";
	    continue;
	  }

	  
	  Buffer payload = p.GetPayload();
	  // Printing incoming RawEthernetPackets from the EtherMux
	  cerr << "===========================================\n";
	  cerr << "Incoming RawEthernetPackets from EtherMux: \n";	
          cerr << "EthernetHeader: \n";
	  cerr << eh << "\n";
	  cerr << "IPHeader: \n";
	  cerr << iph << "\n";
	  cerr << "Data: \n";
	  cerr << "===========================================\n";	  
   
#if IP_MUX
	  p.Serialize(toipmux);
#else
#endif
	} 
	else {
	  // discarded due to different target address
	}
      }
#endif

#if IP_MUX
      if (FD_ISSET(fromipmux,&read_fds)) {
	Packet p;
	p.Unserialize(fromipmux);
	IPHeader iph = p.FindHeader(Headers::IPHeader);

	// ROUTE
        // Loading up the routing table


        
	IPAddress ipaddr;
	iph.GetDestIP(ipaddr);
	
	if(ipaddr == IP_ADDRESS_LO) {
	  cerr << "This packet has destination IP 127.0.0.1\n";
	  cerr << "It's forwarded back up the stack\n";
	  p.Serialize(toipmux);
	}
	
	else {
	  ARPRequestResponse req(ipaddr,
			       EthernetAddr(ETHERNET_BLANK_ADDR),
			       ARPRequestResponse::REQUEST);
 	  ARPRequestResponse resp;

	  req.Serialize(toarp);
	  resp.Unserialize(fromarp);

	  if (resp.ethernetaddr!=ETHERNET_BLANK_ADDR) {
	    resp.flag=ARPRequestResponse::RESPONSE_OK;
	  }

	  if (resp.flag==ARPRequestResponse::RESPONSE_OK) {
	    // set src and dest addrs in header
	    // set protocol ip

	    EthernetHeader h;
	    h.SetSrcAddr(MyEthernetAddr);
	    h.SetDestAddr(resp.ethernetaddr);
	    h.SetProtocolType(PROTO_IP);
	    p.PushHeader(h);
	    RawEthernetPacket e(p);
	    e.Serialize(toethermux);

	    Buffer payload = p.GetPayload();
            // Printing outgoing RawEthernetPackets from the IPMux
            cerr << "===========================================\n";
            cerr << "Outgoing RawEthernetPackets from IPMux: \n";
            cerr << "EthernetHeader: \n";
            cerr << h << "\n";
            cerr << "IPHeader: \n";
            cerr << iph << "\n";
            cerr << "Data: \n";
            cerr << "===========================================\n";
	  } 
	  else {
	    cerr << "Discarded IP packet because there is no arp entry\n";
	  }
	}
      }
#endif
    }
  }
  return 0;
}
