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


	cerr << "Received Packet: " << endl;
	iph.Print(cerr);  cerr << endl;  p.Print(cerr);  cerr << endl;

	IPAddress toip;
	iph.GetDestIP(toip);
	if (toip==MyIPAddr || toip==IPAddress(IP_ADDRESS_BROADCAST)) {
	  if (!(iph.IsChecksumCorrect())) {
	    // discard the packet	  
	    MinetSendToMonitor(MinetMonitoringEvent				\
			("Discarding packet because header checksum is wrong."));
	    cerr << "Discarding following packet because header checksum is wrong: "<<p<<"\n";

	    IPAddress src;  iph.GetSourceIP(src);
	    // "2" specifies the octet that is wrong (in this case, the checksum)
	    icmp_packet error(src, PARAMETER_PROBLEM, 2, p);
	    MinetSendToMonitor(MinetMonitoringEvent("ICMP error message has been sent to host"));

	    // add ethernet header
	    IPHeader error_iph = error.FindHeader(Headers::IPHeader);
	    IPAddress ipaddr;
	    error_iph.GetDestIP(ipaddr);
	    ARPRequestResponse req(ipaddr,
				   EthernetAddr(ETHERNET_BLANK_ADDR),
				   ARPRequestResponse::REQUEST);
	    ARPRequestResponse resp;
	    
	    MinetSend(arp,req);
	    MinetReceive(arp,resp);
	    
	    EthernetHeader error_eh;
	    error_eh.SetSrcAddr(MyEthernetAddr);
	    error_eh.SetDestAddr(resp.ethernetaddr);
	    error_eh.SetProtocolType(PROTO_IP);
	    error.PushHeader(error_eh);
	    
	    RawEthernetPacket e(error);
	    MinetSend(ethermux, e);
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
	  ; // discarded due to different target address
	}

	/*
	// EXAMPLE: RESPONDING TO ICMP REQUESTS FROM IP_MODULE (RAW)
	// respond to packet
	icmp_packet response;
	response.respond(raw);

	// add ethernet header
	IPHeader respond_iph = response.FindHeader(Headers::IPHeader);
	IPAddress ipaddr;
	respond_iph.GetDestIP(ipaddr);
	ARPRequestResponse req(ipaddr,
			       EthernetAddr(ETHERNET_BLANK_ADDR),
			       ARPRequestResponse::REQUEST);
	ARPRequestResponse resp;

	MinetSend(arp,req);
	MinetReceive(arp,resp);

	EthernetHeader respond_eh;
	respond_eh.SetSrcAddr(MyEthernetAddr);
	respond_eh.SetDestAddr(resp.ethernetaddr);
	respond_eh.SetProtocolType(PROTO_IP);
	response.PushHeader(respond_eh);

	if (response.requires_reply())
	{
	  cerr << "Sent Packet: " << endl;
	  DebugDump(response);  cerr << endl;
	  RawEthernetPacket e(response);

	  MinetSend(ethermux, e);
	}
	// END RESPONDING TO ICMP REQUESTS FROM IP_MODULE
	*/

	/*
	// EXAMPLE: RESPONDING TO ICMP REQUESTS FROM IP_MODULE (PACKET)
	// respond to packet
	icmp_packet response;
	response.respond_in_ip_module(p);

	// add ethernet header
	IPHeader respond_iph = response.FindHeader(Headers::IPHeader);
	IPAddress ipaddr;
	respond_iph.GetDestIP(ipaddr);
	ARPRequestResponse req(ipaddr,
			       EthernetAddr(ETHERNET_BLANK_ADDR),
			       ARPRequestResponse::REQUEST);
	ARPRequestResponse resp;

	MinetSend(arp,req);
	MinetReceive(arp,resp);

	EthernetHeader respond_eh;
	respond_eh.SetSrcAddr(MyEthernetAddr);
	respond_eh.SetDestAddr(resp.ethernetaddr);
	respond_eh.SetProtocolType(PROTO_IP);
	response.PushHeader(respond_eh);

	if (response.requires_reply())
	{
	  cerr << "Sent Packet: " << endl;
	  DebugDump(response);  cerr << endl;
	  RawEthernetPacket e(response);

	  MinetSend(ethermux, e);
	}
	// END RESPONDING TO ICMP REQUESTS FROM IP_MODULE
	*/

	/*
	// EXAMPLE: SENDING ICMP ERRORS FROM IP_MODULE
	icmp_packet error("129.105.100.9", DESTINATION_UNREACHABLE, PROTOCOL_UNREACHABLE, p, IP_PROTO_IP);

	// add ethernet header
	IPHeader error_iph = response.FindHeader(Headers::IPHeader);
	IPAddress ipaddr;
        error_iph.GetDestIP(ipaddr);
	ARPRequestResponse req(ipaddr,
			       EthernetAddr(ETHERNET_BLANK_ADDR),
			       ARPRequestResponse::REQUEST);
	ARPRequestResponse resp;

	EthernetHeader error_eh;
	error_eh.SetSrcAddr(MyEthernetAddr);
	error_eh.SetDestAddr(resp.ethernetaddr);
	error_eh.SetProtocolType(PROTO_IP);
	error.PushHeader(error_eh);

	cerr << "Received Packet: " << endl;
	p.ExtractHeaderFromPayload<ICMPHeader>(ICMP_HEADER_LENGTH); DebugDump(p);  cerr << endl;
	cerr << "Sent Packet: " << endl;
	DebugDump(error);  cerr << endl;
	RawEthernetPacket e(error);

	MinetSend(ethermux, e);
	*/


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

	if ((resp.flag==ARPRequestResponse::RESPONSE_OK) || 
	    (ipaddr == "255.255.255.255")) {
	  // set src and dest addrs in header
	  // set protocol ip

	  EthernetHeader h;
	  h.SetSrcAddr(MyEthernetAddr);
	  h.SetDestAddr(resp.ethernetaddr);
	  h.SetProtocolType(PROTO_IP);
	  p.PushHeader(h);

	  RawEthernetPacket e(p);

	  cout << "ABOUT TO SEND OUT: " << endl;
	  Packet check(e);
	  check.ExtractHeaderFromPayload<EthernetHeader>(ETHERNET_HEADER_LEN);
	  check.ExtractHeaderFromPayload<IPHeader>(IPHeader::EstimateIPHeaderLength(check));
	  check.ExtractHeaderFromPayload<ICMPHeader>(ICMP_HEADER_LENGTH);
	  EthernetHeader eh = check.FindHeader(Headers::EthernetHeader);
	  IPHeader iph = check.FindHeader(Headers::IPHeader);
	  ICMPHeader icmph = check.FindHeader(Headers::ICMPHeader);
	  
	  eh.Print(cerr);  cerr << endl;
	  iph.Print(cerr); cerr << endl;
	  icmph.Print(cerr);  cerr << endl;
	  cout << "END OF PACKET" << endl << endl;


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






