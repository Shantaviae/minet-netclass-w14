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
#include "util.h"

#define MONITOR   0

#define FIFO_IMPL 1
#define TCP_IMPL  0

ostream & operator<<(ostream &os, const MinetModule &mon)
{
  switch (mon) { 
  case MINET_MONITOR:
    os << "MINET_MONITOR";
    break;
  case MINET_READER:
    os << "MINET_READER";
    break;
  case MINET_WRITER:
    os << "MINET_WRITER";
    break;
  case MINET_DEVICE_DRIVER:
    os << "MINET_DEVICE_DRIVER";
    break;
  case MINET_ETHERNET_MUX:
    os << "MINET_ETHERNET_MUX";
    break;
  case MINET_IP_MODULE:
    os << "MINET_IP_MODULE";
    break;
  case MINET_ARP_MODULE:
    os << "MINET_ARP_MODULE";
    break;
  case MINET_OTHER_MODULE:
    os << "MINET_OTHER_MODULE";
    break;
  case MINET_IP_MUX:
    os << "MINET_IP_MUX";
    break;
  case MINET_IP_OTHER_MODULE:
    os << "MINET_IP_OTHER_MODULE";
    break;
  case MINET_ICMP_MODULE:
    os << "MINET_ICMP_MODULE";
    break;
  case MINET_UDP_MODULE:
    os << "MINET_UDP_MODULE";
    break;
  case MINET_TCP_MODULE:
    os << "MINET_TCP_MODULE";
    break;
  case MINET_SOCK_MODULE:
    os << "MINET_SOCK_MODULE";
    break;
  case MINET_SOCKLIB_MODULE:
    os << "MINET_SOCKLIB_MODULE";
    break;
  case MINET_APP:
    os << "MINET_APP";
    break;
  case MINET_DEFAULT:
    os << "MINET_DEFAULT";
    break;
  default:
    os << "UNKNOWN";
    break;
  }  
  return os;
}

ostream & operator<<(ostream &os, const MinetDatatype &t)
{
  switch (t) {
  case MINET_NONE:
    os << "MINET_NONE";
    break;
  case MINET_EVENT:
    os << "MINET_EVENT";
    break;
  case MINET_MONITORINGEVENT:
    os << "MINET_MONITORINGEVENT";
    break;
  case MINET_RAWETHERNETPACKET:
    os << "MINET_RAWETHERNETPACKET";
    break;
  case MINET_PACKET:
    os << "MINET_PACKET";
    break;
  case MINET_SOCKREQUESTRESPONSE:
    os << "MINET_SOCKREQUESTRESPONSE";
    break;
  case MINET_SOCKLIBREQUESTRESPONSE:
    os << "MINET_SOCKLIBREQUESTRESPONSE";
    break;
  default:
    os << "UNKNOWN";
    break;
  }
  return os;
}
    

MinetEvent::MinetEvent() :  
  eventtype(Error),   
  direction(NONE), 
  handle(-1),   
  error(-1),   
  overtime(0.0)
{}

MinetEvent::MinetEvent(const MinetEvent &rhs) 
{
  *this = rhs;
}

MinetEvent::~MinetEvent()
{}

const MinetEvent & MinetEvent::operator=(const MinetEvent &rhs)
{
  eventtype=rhs.eventtype; 
  direction=rhs.direction; 
  handle=rhs.handle; 
  error=rhs.error; 
  overtime=rhs.overtime;
  return *this;
}
  

void MinetEvent::Serialize(const int fd) const
{
  if (writeall(fd,(const char*)&eventtype,sizeof(eventtype))!=sizeof(eventtype)) {
    throw SerializationException();
  }
  if (writeall(fd,(const char*)&direction,sizeof(direction))!=sizeof(direction)) {
    throw SerializationException();
  }
  if (writeall(fd,(const char*)&handle,sizeof(handle))!=sizeof(handle)) {
    throw SerializationException();
  }
  if (writeall(fd,(const char*)&error,sizeof(error))!=sizeof(error)) {
    throw SerializationException();
  }
  if (writeall(fd,(const char*)&overtime,sizeof(overtime))!=sizeof(overtime)) {
    throw SerializationException();
  }
}

void MinetEvent::Unserialize(const int fd) 
{
  if (readall(fd,(char*)&eventtype,sizeof(eventtype))!=sizeof(eventtype)) {
    throw SerializationException();
  }
  if (readall(fd,(char*)&direction,sizeof(direction))!=sizeof(direction)) {
    throw SerializationException();
  }
  if (readall(fd,(char*)&handle,sizeof(handle))!=sizeof(handle)) {
    throw SerializationException();
  }
  if (readall(fd,(char*)&error,sizeof(error))!=sizeof(error)) {
    throw SerializationException();
  }
  if (readall(fd,(char*)&overtime,sizeof(overtime))!=sizeof(overtime)) {
    throw SerializationException();
  }
}


ostream & MinetEvent::Print(ostream &os) const
{
  os << "MinetEvent(eventtype=" 
     << (eventtype==Dataflow ? "Dataflow" :
	 eventtype==Exception ? "Exception" :
	 eventtype==Timeout ? "Timeout" :
	 eventtype==Error ? "Error" : "UNKNOWN")
     << ", direction="
     << (direction==IN ? "IN" :
	 direction==OUT ? "OUT" :
	 direction==INOUT ? "INOUT" :
	 direction==NONE ? "NONE" : "UNKNOWN")
     << ", handle=" << handle
     << ", error=" << error
     << ", overtime=" << overtime
     << ")";
  return os;
}


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
static int   MyMonitorFifo;

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
  MyMonitorFifo=-1;
  
#if MONITOR
  char *mf=0;
  switch (MyModuleType) {
  case MINET_MONITOR:
    break;
  case MINET_READER:
    mf=reader2mon_fifo_name;
    break;
  case MINET_WRITER:
    mf=writer2mon_fifo_name;
    break;
  case MINET_DEVICE_DRIVER:
    mf=ethernet2mon_fifo_name;
    break;
  case MINET_ETHERNET_MUX:
    mf=ethermux2mon_fifo_name;
    break;
  case MINET_IP_MODULE:
    mf=ip2mon_fifo_name;
    break;
  case MINET_ARP_MODULE:
    mf=arp2mon_fifo_name;
    break;
  case MINET_OTHER_MODULE:
    mf=other2mon_fifo_name;
    break;
  case MINET_IP_MUX:
    mf=ipmux2mon_fifo_name;
    break;
  case MINET_IP_OTHER_MODULE:
    mf=ipother2mon_fifo_name;
    break;
  case MINET_ICMP_MODULE:
    mf=icmp2mon_fifo_name;
    break;
  case MINET_UDP_MODULE:
    mf=udp2mon_fifo_name;
    break;
  case MINET_TCP_MODULE:
    mf=tcp2mon_fifo_name;
    break;
  case MINET_SOCK_MODULE:
    mf=sock2mon_fifo_name;
    break;
  case MINET_SOCKLIB_MODULE:
    mf=socklib2mon_fifo_name;
    break;
  case MINET_APP:
    mf=app2mon_fifo_name;
    break;
  case MINET_DEFAULT:
  default:
    mf=0;
    break;
  }
  
  if (mf!=0) { 
    if ((MyMonitorFifo=open(mf,WR_ONLY))<0) { 
      cerr << "Can't connect to monitor\n";
    }
  }
#endif
  return 0;
}

int         MinetDeinit()
{
  assert(MyModuleType!=MINET_DEFAULT);
  MyModuleType=MINET_DEFAULT;
  MyFifos.clear();
  MyNextHandle=0;
  if (MyMonitorFifo>0) { 
    close(MyMonitorFifo);
    MyMonitorFifo=-1;
}
  return 0;
}

MinetHandle MinetConnect(const MinetModule &mod, 
                         const MinetModule &connectas=MINET_DEFAULT)
{
  const char *fifoto;
  const char *fifofrom;
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

  MyFifos.push_back(con);
  return con.h;
}

//
// For fifos, accepts are basically just error checked and then
//
// ignored.

MinetHandle MinetAccept(const MinetModule &mod, 
                        bool  await=false)
{
  const char *fifofrom, *fifoto;
  bool above;

  switch (MyModuleType) {
  case MINET_MONITOR:
    switch (mod) {
      // expand this later
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

  MyFifos.push_back(con);
  return con.h;
}



int         MinetClose(const MinetHandle &mh)
{
  Fifos::iterator x=MyFifos.FindMatching(mh);
  if (x!=MyFifos.end()) { 
    close((*x).from);
    close((*x).to);
    MyFifos.erase(x);
  }
  return 0;
}
					  
  

MinetEvent &MinetGetNextEvent(double timeout=-1);


template <class T> 
int MinetSend(const MinetHandle &handle, const T &object);  
template <class T> 
int MinetReceive(const MinetHandle &handle, T &object);





















#endif
