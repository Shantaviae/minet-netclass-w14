#include <stdlib.h>
#include "Minet.h"
#include "Monitor.h"


int main(int argc, char *argv[])
{
  MinetHandle 
    reader,
    writer,
    device,
    ethermux,
    ip,
    arp,
    other,
    ipmux,
    ipother,
    icmp,
    udp,
    tcp,
    sock,
    socklib,
    app;

  char *mon=getenv("MINET_MONITOR");

  if (mon==0) { 
    cerr << "MINET_MONITOR is not set, so monitor is exiting.\n";
    exit(0);
  }

  MinetInit(MINET_MONITOR);
  
  reader = strstr(mon, "reader") ? MinetAccept(MINET_READER) : MINET_NOHANDLE;
  writer = strstr(mon, "writer") ? MinetAccept(MINET_WRITER) : MINET_NOHANDLE;
  device = strstr(mon, "device_driver") ? MinetAccept(MINET_DEVICE_DRIVER) : MINET_NOHANDLE;
  ethermux = strstr(mon, "ethernet_mux") ? MinetAccept(MINET_ETHERNET_MUX) : MINET_NOHANDLE;
  ip=strstr(mon, "ip_module") ? MinetAccept(MINET_IP_MODULE) : MINET_NOHANDLE;
  arp=strstr(mon, "arp_module") ? MinetAccept(MINET_ARP_MODULE) : MINET_NOHANDLE;
  other=strstr(mon, "other_module") ? MinetAccept(MINET_OTHER_MODULE) : MINET_NOHANDLE;
  ipmux=strstr(mon, "ip_mux") ? MinetAccept(MINET_IP_MUX) : MINET_NOHANDLE;
  ipother=strstr(mon, "ipother_module") ? MinetAccept(MINET_IP_OTHER_MODULE) : MINET_NOHANDLE;
  icmp=strstr(mon, "icmp_module") ? MinetAccept(MINET_ICMP_MODULE) : MINET_NOHANDLE;
  udp=strstr(mon, "udp_module") ? MinetAccept(MINET_UDP_MODULE) : MINET_NOHANDLE;
  tcp=strstr(mon, "tcp_module") ? MinetAccept(MINET_TCP_MODULE) : MINET_NOHANDLE;
  sock=strstr(mon, "sock_module") ? MinetAccept(MINET_SOCK_MODULE) : MINET_NOHANDLE;
  socklib=strstr(mon, "socklib_module") ? MinetAccept(MINET_SOCKLIB_MODULE) : MINET_NOHANDLE;
  app=strstr(mon, "app") ? MinetAccept(MINET_APP) : MINET_NOHANDLE;

  MinetEvent myevent;
  MinetMonitoringEventDescription desc;

  MinetEvent event;
  MinetMonitoringEvent monevent;
  RawEthernetPacket rawpacket;
  Packet packet;
  SockRequestResponse srr;
  SockLibRequestResponse slrr;
  ARPRequestResponse arr;

  while (MinetGetNextEvent(myevent)==0) {
    if (myevent.eventtype!=MinetEvent::Dataflow || myevent.direction!=MinetEvent::IN) {
      cerr << "Ignoring this event: "<<myevent<<endl;
    } else {
      MinetReceive(myevent.handle,desc);
      cerr << desc << " : ";
      switch (desc.datatype) {
      case MINET_EVENT:
	MinetReceive(myevent.handle,event); 
	cerr << event << endl;
	break;
      case MINET_MONITORINGEVENT:
	MinetReceive(myevent.handle,monevent); 
	cerr << monevent << endl;
	break;
      case MINET_RAWETHERNETPACKET:
	MinetReceive(myevent.handle,rawpacket); 
	cerr << rawpacket << endl;
	break;
      case MINET_PACKET:
	MinetReceive(myevent.handle,packet); 
	cerr << packet << endl;
	break;
      case MINET_ARPREQUESTRESPONSE:
	MinetReceive(myevent.handle,arr); 
	cerr << arr << endl;
	break;
      case MINET_SOCKREQUESTRESPONSE:
	MinetReceive(myevent.handle,srr); 
	cerr << srr << endl;
	break;
      case MINET_SOCKLIBREQUESTRESPONSE:
	MinetReceive(myevent.handle,slrr); 
	cerr << slrr << endl;
	break;
      case MINET_NONE:
      default:
	break;
      }
    }
  }
  MinetDeinit();
}
