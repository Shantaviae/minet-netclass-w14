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
#include <deque>

#include "Minet.h"
#include "error.h"
#include "config.h"

#define FIFO_IMPL 1
#define TCP_IMPL  0




#if FIFO_IMPL

struct FifoData {
  MinetHandle h;
  int         from, to;
};

class Fifos : public deque<FifoData> {
public:
  iterator FindMatching(MinetHandle handle) {
    for (iterator x=begin(); x!=end(); ++x) {
      if ((*x).h==handle) {
	return x;
      }
    }
    return end();
  }
};


static MinetModule MyModuleType=MINET_DEFAULT;
static Fifos MyFifos;
static int   MyNextHandle;


MinetHandle MinetGetNextHandle()
{
  return MyNextHandle++;
}

int         MinetInit(const MinetModule &mod)
{
  assert(MyModuleType==MINET_DEFAULT);
  MyModuleType=mod;
  MyFifos.clear();
  MyNextHandle=0;
  return 0;
}

int         MinetDeinit()
{
  assert(MyModuleType!=MINET_DEFAULT);
  MyModuleType=MINET_DEFAULT;
  MyFifos.clear();
  MyNextHandle=0;
  return 0;
}

MinetHandle MinetConnect(const MinetModule &mod, 
                         const MinetModule &connectas=MINET_DEFAULT)
{
  char *fifoto;
  char *fifofrom;
  bool above;
  MinetModule realconnectas= (connectas==MINET_DEFAULT) ? MyModuleType : connectas;
    

  switch (realconnectas) {
  case MINET_MONITOR:
    switch (mod) {
      // expand this later
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_SOCK_MODULE:
    switch (mod) {
    case MINET_IP_MUX:  // future connection
      Die("Connection type unimplemented.");
      break;
    case MINET_ICMP_MODULE: 
      fifoto=sock2icmp_fifo_name;
      fifofrom=icmp2sock_fifo_name;
      above=false;
      break;
    case MINET_UDP_MODULE:
      fifoto=sock2udp_fifo_name;
      fifofrom=udp2sock_fifo_name;
      above=false;
      break;
    case MINET_TCP_MODULE:
      fifoto=sock2tcp_fifo_name;
      fifofrom=tcp2sock_fifo_name;
      above=false;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_SOCKLIB_MODULE:
  case MINET_APP:
    switch (mod) {
    case MINET_SOCK_MODULE:  
      fifoto=app2sock_fifo_name;
      fifofrom=sock2app_fifo_name;
      above=false;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_READER:
    Die("Invalid Connection");
    break;
  case MINET_WRITER:
    Die("Invalid Connection");
    break;
  case MINET_DEVICE_DRIVER:
    switch (mod) {
    case MINET_ETHERNET_MUX:
      fifoto=ether2mux_fifo_name;
      fifofrom=mux2ether_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_ETHERNET_MUX:
    switch (mod) {
    case MINET_DEVICE_DRIVER:
      fifoto=mux2ether_fifo_name;
      fifofrom=ether2mux_fifo_name;
      above=false;
      break;
    case MINET_IP_MODULE:
      fifoto=mux2ip_fifo_name;
      fifofrom=ip2mux_fifo_name;
      above=true;
      break;
    case MINET_ARP_MODULE:
      fifoto=mux2arp_fifo_name;
      fifofrom=arp2mux_fifo_name;
      above=true;
      break;
    case MINET_OTHER_MODULE:
      fifoto=mux2other_fifo_name;
      fifofrom=other2mux_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_IP_MODULE:
    switch (mod) { 
    case MINET_ETHERNET_MUX:
      fifoto=ip2mux_fifo_name;
      fifofrom=mux2ip_fifo_name;
      above=false;
      break;
    case MINET_IP_MUX:
      fifoto=ip2ipmux_fifo_name;
      fifofrom=ipmux2ip_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_ARP_MODULE:
    switch (mod) {
    case MINET_ETHERNET_MUX:
      fifoto=arp2mux_fifo_name;
      fifofrom=mux2arp_fifo_name;
      above=false;
      break;
    case MINET_IP_MODULE:
      fifoto=arp2ip_fifo_name;
      fifoto=ip2arp_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_OTHER_MODULE:
    switch (mod) {
    case MINET_ETHERNET_MUX:
      fifoto=other2mux_fifo_name;
      fifofrom=mux2other_fifo_name;
      above=false;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_IP_MUX:
    switch (mod) {
    case MINET_IP_MODULE:
      fifoto=ipmux2ip_fifo_name;
      fifofrom=ip2ipmux_fifo_name;
      above=false;
      break;
    case MINET_ICMP_MODULE:
      fifoto=ipmux2icmp_fifo_name;
      fifofrom=icmp2ipmux_fifo_name;
      above=true;
      break;
    case MINET_UDP_MODULE:
      fifoto=ipmux2udp_fifo_name;
      fifofrom=udp2ipmux_fifo_name;
      above=true;
      break;
    case MINET_TCP_MODULE:
      fifoto=ipmux2tcp_fifo_name;
      fifofrom=tcp2ipmux_fifo_name;
      above=true;
      break;
    case MINET_IP_OTHER_MODULE:
      fifoto=ipmux2other_fifo_name;
      fifofrom=other2ipmux_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_ICMP_MODULE:
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=icmp2ipmux_fifo_name;
      fifofrom=ipmux2icmp_fifo_name;
      above=false;
      break;
    case MINET_SOCK_MODULE:
      fifoto=icmp2sock_fifo_name;
      fifofrom=sock2icmp_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_UDP_MODULE:
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=udp2ipmux_fifo_name;
      fifofrom=ipmux2udp_fifo_name;
      above=false;
      break;
    case MINET_SOCK_MODULE:
      fifoto=udp2sock_fifo_name;
      fifofrom=sock2udp_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_TCP_MODULE:
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=tcp2ipmux_fifo_name;
      fifofrom=ipmux2tcp_fifo_name;
      above=false;
      break;
    case MINET_SOCK_MODULE:
      fifoto=tcp2sock_fifo_name;
      fifofrom=sock2tcp_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_IP_OTHER_MODULE:
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=other2ipmux_fifo_name;
      fifofrom=ipmux2other_fifo_name;
      above=false;
      break;
    case MINET_SOCK_MODULE:
      fifoto=other2sock_fifo_name;
      fifofrom=sock2other_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_SOCK_MODULE:
    switch (mod) {
    case MINET_IP_MUX:
      Die("Connection type unimplemented.");
      break;
    case MINET_ICMP_MODULE:
      fifoto=sock2icmp_fifo_name;
      fifofrom=icmp2sock_fifo_name;
      above=false;
      break;
    case MINET_UDP_MODULE:
      fifoto=sock2udp_fifo_name;
      fifofrom=udp2sock_fifo_name;
      above=false;
      break;
    case MINET_TCP_MODULE:
      fifoto=sock2tcp_fifo_name;
      fifofrom=tcp2sock_fifo_name;
      above=false;
      break;
    case MINET_IP_OTHER_MODULE:
      Die("Connection type unimplemented.");
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_SOCKLIB_MODULE:
  case MINET_APP:
    switch (mod) { 
    case MINET_SOCK_MODULE:
      fifoto=socklib2sock_fifo_name;
      fifofrom=sock2socklib_fifo_name;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_DEFAULT:
  default:
    Die("Unknown module!");
  }

  FifoData con;
  con.h=MinetGetNextHandle();

  if (!above) {
    con.from=open(fifofrom,O_RDONLY);
    con.to=open(fifoto,O_WRONLY);
  } else {
    con.to=open(fifoto,O_WRONLY);
    con.from=open(fifofrom,O_RDONLY);
  }

  MyFifos.pushback(con);
  return con.h;
}

//
// For fifos, accepts are basically just error checked and then
//
// ignored.

MinetHandle MinetAccept(const MinetModule &mod, 
                        bool  await=false)
{
  switch (MyModuleType) {
  case MINET_MONITOR:
    switch (mod) {
      // expand this later
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_SOCK_MODULE:
    switch (mod) {
    case MINET_IP_MUX:  // future connection
      Die("Connection type unimplemented.");
      break;
    case MINET_ICMP_MODULE: 
    case MINET_UDP_MODULE:
    case MINET_TCP_MODULE:
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_SOCKLIB_MODULE:
  case MINET_APP:
    switch (mod) {
    case MINET_SOCK_MODULE:  
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_READER:
    Die("Invalid Connection");
    break;
  case MINET_WRITER:
    Die("Invalid Connection");
    break;
  case MINET_DEVICE_DRIVER:
    switch (mod) {
    case MINET_ETHERNET_MUX:
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_ETHERNET_MUX:
    switch (mod) {
    case MINET_DEVICE_DRIVER:
    case MINET_IP_MODULE:
    case MINET_ARP_MODULE:
    case MINET_OTHER_MODULE:
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_IP_MODULE:
    switch (mod) { 
    case MINET_ETHERNET_MUX:
    case MINET_IP_MUX:
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_ARP_MODULE:
    switch (mod) {
    case MINET_ETHERNET_MUX:
      fifoto=arp2mux_fifo_name;
      fifofrom=mux2arp_fifo_name;
      above=false;
      break;
    case MINET_IP_MODULE:
      fifoto=arp2ip_fifo_name;
      fifoto=ip2arp_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_OTHER_MODULE:
    switch (mod) {
    case MINET_ETHERNET_MUX:
      fifoto=other2mux_fifo_name;
      fifofrom=mux2other_fifo_name;
      above=false;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_IP_MUX:
    switch (mod) {
    case MINET_IP_MODULE:
      fifoto=ipmux2ip_fifo_name;
      fifofrom=ip2ipmux_fifo_name;
      above=false;
      break;
    case MINET_ICMP_MODULE:
      fifoto=ipmux2icmp_fifo_name;
      fifofrom=icmp2ipmux_fifo_name;
      above=true;
      break;
    case MINET_UDP_MODULE:
      fifoto=ipmux2udp_fifo_name;
      fifofrom=udp2ipmux_fifo_name;
      above=true;
      break;
    case MINET_TCP_MODULE:
      fifoto=ipmux2tcp_fifo_name;
      fifofrom=tcp2ipmux_fifo_name;
      above=true;
      break;
    case MINET_IP_OTHER_MODULE:
      fifoto=ipmux2other_fifo_name;
      fifofrom=other2ipmux_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_ICMP_MODULE:
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=icmp2ipmux_fifo_name;
      fifofrom=ipmux2icmp_fifo_name;
      above=false;
      break;
    case MINET_SOCK_MODULE:
      fifoto=icmp2sock_fifo_name;
      fifofrom=sock2icmp_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_UDP_MODULE:
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=udp2ipmux_fifo_name;
      fifofrom=ipmux2udp_fifo_name;
      above=false;
      break;
    case MINET_SOCK_MODULE:
      fifoto=udp2sock_fifo_name;
      fifofrom=sock2udp_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_TCP_MODULE:
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=tcp2ipmux_fifo_name;
      fifofrom=ipmux2tcp_fifo_name;
      above=false;
      break;
    case MINET_SOCK_MODULE:
      fifoto=tcp2sock_fifo_name;
      fifofrom=sock2tcp_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_IP_OTHER_MODULE:
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=other2ipmux_fifo_name;
      fifofrom=ipmux2other_fifo_name;
      above=false;
      break;
    case MINET_SOCK_MODULE:
      fifoto=other2sock_fifo_name;
      fifofrom=sock2other_fifo_name;
      above=true;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_SOCK_MODULE:
    switch (mod) {
    case MINET_IP_MUX:
      Die("Connection type unimplemented.");
      break;
    case MINET_ICMP_MODULE:
      fifoto=sock2icmp_fifo_name;
      fifofrom=icmp2sock_fifo_name;
      above=false;
      break;
    case MINET_UDP_MODULE:
      fifoto=sock2udp_fifo_name;
      fifofrom=udp2sock_fifo_name;
      above=false;
      break;
    case MINET_TCP_MODULE:
      fifoto=sock2tcp_fifo_name;
      fifofrom=tcp2sock_fifo_name;
      above=false;
      break;
    case MINET_IP_OTHER_MODULE:
      Die("Connection type unimplemented.");
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_SOCKLIB_MODULE:
  case MINET_APP:
    switch (mod) { 
    case MINET_SOCK_MODULE:
      fifoto=socklib2sock_fifo_name;
      fifofrom=sock2socklib_fifo_name;
      break;
    default:
      Die("Invalid Connection.");
      break;
    }
    break;
  case MINET_DEFAULT:
  default:
    Die("Unknown module!");
  }

  FifoData con;
  con.h=MinetGetNextHandle();

  if (!above) {
    con.from=open(fifofrom,O_RDONLY);
    con.to=open(fifoto,O_WRONLY);
  } else {
    con.to=open(fifoto,O_WRONLY);
    con.from=open(fifofrom,O_RDONLY);
  }

  MyFifos.pushback(con);
  return con.h;
}
}


int         MinetClose(const MinetHandle &mh)
{
  Fifos::iterator x=MyFifos.FindMatching(mh);
  if (x!=MyFifos.end()) { 
    close((*x).from);
    close((*x).to);
    MyFifos.delete(x);
  }
  return 0;
}
					  
  

MinetEvent &MinetGetNextEvent(double timeout=-1);


template <class T> 
int MinetSend(const MinetHandle &handle, const T &object);  
template <class T> 
int MinetReceive(const MinetHandle &handle, T &object);





















#endif
