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
  MinetHandle mux;

  MinetInit(MINET_OTHER_MODULE);

  mux=MinetIsModuleInConfig(MINET_ETHERNET_MUX) ? MinetConnect(MINET_ETHERNET_MUX) : MINET_NOHANDLE;

  if (mux==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_ETHERNET_MUX)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to ethermux"));
    return -1;
  }
  
  MinetSendToMonitor(MinetMonitoringEvent("other_module: handling OTHER traffic"));

  MinetEvent event;
  int rc;

  while (MinetGetNextEvent(event)) {
    if (event.eventtype!=MinetEvent::Dataflow 
	|| event.direction!=MinetEvent::IN) {
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    } else {
      if (event.handle==mux) {
	RawEthernetPacket raw;
	MinetReceive(mux,raw);
      }
    }
  }
  return 0;
}
