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

#include "Minet.h"


int main(int argc, char *argv[])
{

  MinetHandle ipmux;
  MinetHandle sock;

  MinetInit(MINET_ICMP_MODULE);

  ipmux=MinetIsModuleInConfig(MINET_IP_MUX) ? MinetConnect(MINET_IP_MUX) : MINET_NOHANDLE;
  sock=MinetIsModuleInConfig(MINET_SOCK_MODULE) ? MinetAccept(MINET_SOCK_MODULE) : MINET_NOHANDLE;

  if (ipmux==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_IP_MUX)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to ipmux"));
    return -1;
  }
  if (sock==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_SOCK_MODULE)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to sock_module"));
    return -1;
  }

  MinetSendToMonitor(MinetMonitoringEvent("icmp_module handling icmp traffic"));

  MinetEvent event;

  while (MinetGetNextEvent(event)==0) {
    if (event.eventtype!=MinetEvent::Dataflow 
	|| event.direction!=MinetEvent::IN) {
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    } else {
      if (event.handle==ipmux) {
	Packet p;
	unsigned short len;
	bool checksumok;
	MinetReceive(ipmux,p);
	MinetSendToMonitor(MinetMonitoringEvent("Got ICMP Packet!"));
	cerr << "Got ICMP Packet! " << p << endl;
      }
    }
  }
  MinetDeinit();
  return 0;
}
