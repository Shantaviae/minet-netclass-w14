#include <stdlib.h>
#include "Minet.h"
#include "Monitor.h"


int main(int argc, char *argv[])
{
  char *mon=getenv("MINET_MONITOR");
  
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

  MinetInit(MINET_MONITOR);
  
  reader= (strstr(mon, MinetAccept(MINET_READER);
  writer=MinetAccept(MINET_WRITER);
  device=MinetAccept(MINET_DEVICE_DRIVER);
  ethermux=MinetAccept(MINET_ETHERNET_MUX);
  ip=MinetAccept(MINET_IP_MODULE);
  arp=MinetAccept(MINET_ARP_MODULE);
  other=MinetAccept(MINET_OTHER_MODULE);
  ipmux=MinetAccept(MINET_IP_MUX);
  ipother=MinetAccept(MINET_IP_OTHER_MODULE);
  icmp=MinetAccept(MINET_ICMP_MODULE);
  udp=MinetAccept(MINET_UDP_MODULE);
  tcp=MinetAccept(MINET_TCP_MODULE);
  sock=MinetAccept(MINET_SOCK_MODULE);
  socklib=MinetAccept(MINET_SOCKLIB_MODULE);
  app=MinetAccept(MINET_APP);

  







}
