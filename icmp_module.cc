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

#define IP_MUX    1
#define SOCK      0


#define ECHO      0



int main(int argc, char *argv[])
{

#if IP_MUX
  MinetHandle ipmux;
#endif
#if SOCK
  MinetHandle sock;
#endif

  MinetInit(MINET_ICMP_MODULE);

#if IP_MUX
  ipmux=MinetConnect(MINET_IP_MUX);
#endif
#if SOCK
  sock=MinetAccept(MINET_SOCK_MODULE);
#endif

#if IP_MUX
  if (ipmux==MINET_NOHANDLE) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to ipmux"));
    return -1;
  }
#endif

#if SOCK
  if (sock==MINET_NOHANDLE) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to sock_module"));
    return -1;
  }
#endif

  MinetSendToMonitor(MinetMonitoringEvent("icmp_module handling icmp traffic"));

  MinetEvent event;

  while (MinetGetNextEvent(event)==0) {
    if (event.eventtype!=MinetEvent::Dataflow 
	|| event.direction!=MinetEvent::IN) {
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    } else {
#if IP_MUX
      if (event.handle==ipmux) {
	Packet p;
	unsigned short len;
	bool checksumok;
	MinetReceive(ipmux,p);
	MinetSendToMonitor(MinetMonitoringEvent("Got ICMP Packet!"));
	cerr << "Got ICMP Packet! " << p << endl;
      }
#endif
    }
  }
  MinetDeinit();
  return 0;
}
