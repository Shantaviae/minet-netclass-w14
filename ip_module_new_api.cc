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

#include "Minet.h"


int main(int argc, char *argv[])
{
  MinetHandle ethermux, ipmux, arp;

  MinetInit(MINET_IP_MODULE);

  ethermux=MinetIsModuleInConfig(MINET_ETHERNET_MUX) ? 				\
			MinetConnect(MINET_ETHERNET_MUX) : MINET_NOHANDLE;
  arp=MinetIsModuleInConfig(MINET_ARP_MODULE) ? 				\
			MinetConnect(MINET_ARP_MODULE) : MINET_NOHANDLE;
  ipmux=MinetIsModuleInConfig(MINET_IP_MUX) ?					\
			MinetAccept(MINET_IP_MUX) : MINET_NOHANDLE;

  if (ethermux==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_ETHERNET_MUX)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to ethermux"));
    return -1;
  }
  if (ipmux==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_ETHERNET_MUX)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't accept from ipmux"));
    return -1;
  }
  if (arp==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_ETHERNET_MUX)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to arp_module"));
    return -1;
  }


  MinetSendToMonitor(MinetMonitoringEvent("ip_module handling IP traffic........"));

  MinetEvent event;

  while (MinetGetNextEvent(event)==0) {
    if (event.eventtype!=MinetEvent::Dataflow 
	|| event.direction!=MinetEvent::IN) {
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    } else {
      if (event.handle==ethermux) {
	RawEthernetPacket raw;
	MinetReceive(ethermux,raw);
	Packet p(raw);
	p.ExtractHeaderFromPayload<EthernetHeader>(ETHERNET_HEADER_LEN);
	p.ExtractHeaderFromPayload<IPHeader>(IPHeader::EstimateIPHeaderLength(p));
	IPHeader iph;
	iph=p.FindHeader(Headers::IPHeader);

	IPAddress toip;
	iph.GetDestIP(toip);
	if (toip==MyIPAddr || toip==IPAddress(IP_ADDRESS_BROADCAST)) {
	  if (!(iph.IsChecksumCorrect())) {
	    MinetSendToMonitor(MinetMonitoringEvent				\
			("Discarding packet because header checksum is wrong."));
	    cerr << "Discarding following packet because header checksum is wrong: "<<p<<"\n";
	    continue;
	  }
	  unsigned short fragoff;
	  unsigned char flags;
	  iph.GetFlags(flags);
	  iph.GetFragOffset(fragoff);
	  if ((flags&IP_HEADER_FLAG_MOREFRAG) || (fragoff!=0)) { 
	    MinetSendToMonitor(MinetMonitoringEvent				\
			("Discarding packet because it is a fragment"));
	    cerr << "Discarding following packet because it is a fragment: "<<p<<"\n";
	    cerr << "NOTE: NO ICMP PACKET WAS SENT BACK\n";
	    continue;
	  }
	  MinetSend(ipmux,p);
	} else {
	  // discarded due to different target address
	}
      }
      if (event.handle==ipmux) {
	Packet p;
	MinetReceive(ipmux,p);
	IPHeader iph = p.FindHeader(Headers::IPHeader);

	// ROUTE

	IPAddress ipaddr;
	iph.GetDestIP(ipaddr);

	ARPRequestResponse req(ipaddr,
			       EthernetAddr(ETHERNET_BLANK_ADDR),
			       ARPRequestResponse::REQUEST);
	ARPRequestResponse resp;

	MinetSend(arp,req);
	MinetReceive(arp,resp);

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
	  MinetSend(ethermux,e);
	} else {
	  MinetSendToMonitor(MinetMonitoringEvent 				\
			("Discarding packet because there is no arp entry"));
	  cerr << "Discarded IP packet because there is no arp entry\n";
	}
      }
    }
  }
  MinetDeinit();
  return 0;
}
