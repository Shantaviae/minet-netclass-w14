#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

#define MONITOR   1

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
  case MINET_ARPREQUESTRESPONSE:
    os << "MINET_ARPREQUESTRESPONSE";
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
  MinetHandle handle;
  MinetModule module;
  int         from, to;
};

class Fifos : public deque<FifoData> {
public:
  iterator FindMatching(const MinetHandle handle) {
    for (iterator x=begin(); x!=end(); ++x) {
      if ((*x).handle==handle) {
	return x;
      }
    }
    return end();
  }
};


static MinetModule MyModuleType=MINET_DEFAULT;
static Fifos MyFifos;
static int   MyNextHandle;
static int   MyMonitorFifo=-1;

MinetHandle MinetGetNextHandle()
{
  return MyNextHandle++;
}



const char *MinetGetMonitorFifoName(const MinetModule &mod)
{
#if MONITOR
  const char *env=getenv("MINET_MONITOR");
  
  const char *mf=0;
  
  if (env!=0) {
    switch (mod) {
    case MINET_MONITOR:
      break;
    case MINET_READER:
      mf=strstr(env,"reader") ? reader2mon_fifo_name : 0;
      break;
    case MINET_WRITER:
      mf=strstr(env,"writer") ? writer2mon_fifo_name : 0;
      break;
    case MINET_DEVICE_DRIVER:
      mf=strstr(env,"device_driver") ? ether2mon_fifo_name : 0;
      break;
    case MINET_ETHERNET_MUX:
      mf=strstr(env,"ethernet_mux") ? ethermux2mon_fifo_name : 0;
      break;
    case MINET_IP_MODULE:
      mf=strstr(env,"ip_module") ? ip2mon_fifo_name : 0;
      break;
    case MINET_ARP_MODULE:
      mf=strstr(env,"arp_module") ? arp2mon_fifo_name : 0;
      break;
    case MINET_OTHER_MODULE:
      mf=strstr(env,"other_module") ? other2mon_fifo_name : 0;
      break;
    case MINET_IP_MUX:
      mf=strstr(env,"ip_mux") ? ipmux2mon_fifo_name : 0;
      break;
    case MINET_IP_OTHER_MODULE:
      mf=strstr(env,"ipother_module") ? ipother2mon_fifo_name : 0;
      break;
    case MINET_ICMP_MODULE:
      mf=strstr(env,"icmp_module") ? icmp2mon_fifo_name : 0;
      break;
    case MINET_UDP_MODULE:
      mf=strstr(env,"udp_module") ? udp2mon_fifo_name : 0;
      break;
    case MINET_TCP_MODULE:
      mf=strstr(env,"tcp_module") ? tcp2mon_fifo_name : 0;
      break;
    case MINET_SOCK_MODULE:
      mf=strstr(env,"sock_module") ? sock2mon_fifo_name : 0;
      break;
    case MINET_SOCKLIB_MODULE:
      mf=strstr(env,"socklib_module") ? socklib2mon_fifo_name : 0;
      break;
    case MINET_APP:
      mf=strstr(env,"app") ? app2mon_fifo_name : 0;
      break;
    case MINET_DEFAULT:
    default:
      mf=0;
      break;
    }
    return mf;
  } else {
    return 0;
  }
#else
  return 0;
#endif
}

bool        MinetIsModuleMonitored(const MinetModule &mod)
{
  return MinetGetMonitorFifoName(mod)!=0;
}


int         MinetInit(const MinetModule &mod)
{
  char *env = getenv("MINET_MONITOR");
  assert(MyModuleType==MINET_DEFAULT);
  MyModuleType=mod;
  MyFifos.clear();
  MyNextHandle=0;
  MyMonitorFifo=-1;
  
#if MONITOR
  const char *mf=MinetGetMonitorFifoName(mod);

  if (mf!=0) { 
    if ((MyMonitorFifo=open(mf,O_WRONLY))<0) { 
      cerr << "Can't connect to monitor\n";
    }
  }
#endif
  
  MinetMonitoringEventDescription desc;

  desc.timestamp=Time();
  desc.source=MyModuleType;
  desc.from=MyModuleType;
  desc.to=MyModuleType;
  desc.datatype=MINET_MONITORINGEVENT;
  desc.optype=MINET_INIT;

  MinetSendToMonitor(desc);
  
  return 0;
}

int         MinetDeinit()
{
  assert(MyModuleType!=MINET_DEFAULT);
  MyModuleType=MINET_DEFAULT;
  MyFifos.clear();
  MyNextHandle=0;

  MinetMonitoringEventDescription desc;

  desc.timestamp=Time();
  desc.from=MyModuleType;
  desc.to=MyModuleType;
  desc.datatype=MINET_MONITORINGEVENT;
  desc.optype=MINET_DEINIT;

  MinetSendToMonitor(desc);

  if (MyMonitorFifo>0) { 
    close(MyMonitorFifo);
    MyMonitorFifo=-1;
  }
  return 0;
}

bool        MinetIsModuleInConfig(const MinetModule &mod)
{
  char *env = getenv("MINET_MODULES");
  
  if (env!=0) {
    switch (mod) {
    case MINET_MONITOR:
      return 0!=strstr(env,"monitor");
      break;
    case MINET_READER:
      return 0!=strstr(env,"reader");
      break;
    case MINET_WRITER:
      return 0!=strstr(env,"writer");
      break;
    case MINET_DEVICE_DRIVER:
      return 0!=strstr(env,"device_driver");
      break;
    case MINET_ETHERNET_MUX:
      return 0!=strstr(env,"ethernet_mux");
      break;
    case MINET_IP_MODULE:
      return 0!=strstr(env,"ip_module");
      break;
    case MINET_ARP_MODULE:
      return 0!=strstr(env,"arp_module");
      break;
    case MINET_OTHER_MODULE:
      return 0!=strstr(env,"other_module");
      break;
    case MINET_IP_MUX:
      return 0!=strstr(env,"ip_mux");
      break;
    case MINET_IP_OTHER_MODULE:
      return 0!=strstr(env,"ipother_module");
      break;
    case MINET_ICMP_MODULE:
      return 0!=strstr(env,"icmp_module");
      break;
    case MINET_UDP_MODULE:
      return 0!=strstr(env,"udp_module");
      break;
    case MINET_TCP_MODULE:
      return 0!=strstr(env,"tcp_module");
      break;
    case MINET_SOCK_MODULE:
      return 0!=strstr(env,"sock_module");
      break;
    case MINET_SOCKLIB_MODULE:
      return 0!=strstr(env,"socklib_module");
      break;
    case MINET_APP:
      return 0!=strstr(env,"app");
      break;
    case MINET_DEFAULT:
      return false;
    default:
      return false;
      break;
    }
  } else {
    return false;
  }
}  


MinetHandle MinetConnect(const MinetModule &mod)
{
  const char *fifoto;
  const char *fifofrom;

  switch (MyModuleType) {
  case MINET_MONITOR:
    switch (mod) {
      // Monitors only do accepts
    default:
      Die("Invalid Connect.");
      break;
    }
    break;
  case MINET_READER:
    // Readers still use their internal protocol
    Die("Invalid Connect");
    break;
  case MINET_WRITER:
    // Writers still use their internal protocol
    Die("Invalid Connect");
    break;
  case MINET_DEVICE_DRIVER:
    // Device drivers do no do active connects
    Die("Invalid Connect.");
    break;
  case MINET_ETHERNET_MUX:
    // active connect to dd, passive connects to several others.
    switch (mod) {
    case MINET_DEVICE_DRIVER:
      fifoto=mux2ether_fifo_name;
      fifofrom=ether2mux_fifo_name;
      break;
    default:
      Die("Invalid Connect.");
      break;
    }
    break;
  case MINET_IP_MODULE:
    // active to ethermux and arp module, passive to ip mux
    switch (mod) { 
    case MINET_ETHERNET_MUX:
      fifoto=ip2mux_fifo_name;
      fifofrom=mux2ip_fifo_name;
      break;
    case MINET_ARP_MODULE:
      fifoto=ip2arp_fifo_name;
      fifofrom=arp2ip_fifo_name;
      break;
    default:
      Die("Invalid Connect.");
      break;
    }
    break;
  case MINET_ARP_MODULE:
    // active to ethermux, passive to ip
    switch (mod) {
    case MINET_ETHERNET_MUX:
      fifoto=arp2mux_fifo_name;
      fifofrom=mux2arp_fifo_name;
      break;
    default:
      Die("Invalid Connect.");
      break;
    }
    break;
  case MINET_OTHER_MODULE:
    // active to ethermux
    switch (mod) {
    case MINET_ETHERNET_MUX:
      fifoto=other2mux_fifo_name;
      fifofrom=mux2other_fifo_name;
      break;
    default:
      Die("Invalid Connect.");
      break;
    }
    break;
  case MINET_IP_MUX:
    // active to ip module, passive others
    switch (mod) {
    case MINET_IP_MODULE:
      fifoto=ipmux2ip_fifo_name;
      fifofrom=ip2ipmux_fifo_name;
      break;
    default:
      Die("Invalid Connect.");
      break;
    }
    break;
  case MINET_ICMP_MODULE:
    // active to ip mux, passive to sock
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=icmp2ipmux_fifo_name;
      fifofrom=ipmux2icmp_fifo_name;
      break;
    default:
      Die("Invalid Connect.");
      break;
    }
    break;
  case MINET_UDP_MODULE:
    // active to ip mux, passive to sock
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=udp2ipmux_fifo_name;
      fifofrom=ipmux2udp_fifo_name;
      break;
    default:
      Die("Invalid Connect.");
      break;
    }
    break;
  case MINET_TCP_MODULE:
    // active to ipmux, passive to sock
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=tcp2ipmux_fifo_name;
      fifofrom=ipmux2tcp_fifo_name;
      break;
    default:
      Die("Invalid Connect.");
      break;
    }
    break;
  case MINET_IP_OTHER_MODULE:
    // active to ip mux, passive to sock
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=other2ipmux_fifo_name;
      fifofrom=ipmux2other_fifo_name;
      break;
    default:
      Die("Invalid Connect.");
      break;
    }
    break;
  case MINET_SOCK_MODULE:
    // active to icmp, udp, tcp, ipother, and other, passive to socklib module
    switch (mod) {
    case MINET_IP_MUX:
    case MINET_OTHER_MODULE:
      Die("Connect type unimplemented.");
      break;
    case MINET_IP_OTHER_MODULE:
      fifoto=sock2ipother_fifo_name;
      fifofrom=ipother2sock_fifo_name;
      break;
    case MINET_ICMP_MODULE:
      fifoto=sock2icmp_fifo_name;
      fifofrom=icmp2sock_fifo_name;
      break;
    case MINET_UDP_MODULE:
      fifoto=sock2udp_fifo_name;
      fifofrom=udp2sock_fifo_name;
      break;
    case MINET_TCP_MODULE:
      fifoto=sock2tcp_fifo_name;
      fifofrom=tcp2sock_fifo_name;
      break;
    default:
      Die("Invalid Connect.");
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
      Die("Invalid Connect.");
      break;
    }
    break;
  case MINET_DEFAULT:
  default:
    Die("Unknown module!");
  }

  FifoData con;
  con.handle=MinetGetNextHandle();
  con.module=mod;

  con.from = fifofrom!=0 ? open(fifofrom,O_RDONLY) : -1;
  con.to = fifoto!=0 ? open(fifoto,O_WRONLY) : -1;

  MyFifos.push_back(con);

  MinetMonitoringEventDescription desc;

  desc.timestamp=Time();
  desc.source=MyModuleType;
  desc.from=MyModuleType;
  desc.to=mod;
  desc.datatype=MINET_MONITORINGEVENT;
  desc.optype=MINET_CONNECT;

  MinetSendToMonitor(desc);

  return con.handle;
}

MinetHandle MinetAccept(const MinetModule &mod)
{
  const char *fifofrom, *fifoto;
  bool above;

  switch (MyModuleType) {
  case MINET_MONITOR:
    switch (mod) {
    case MINET_READER:
      fifofrom=reader2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_WRITER:
      fifofrom=writer2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_DEVICE_DRIVER:
      fifofrom=ether2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_ETHERNET_MUX:
      fifofrom=ethermux2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_ARP_MODULE:
      fifofrom=arp2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_IP_MODULE:
      fifofrom=ip2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_OTHER_MODULE:
      fifofrom=other2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_IP_MUX:
      fifofrom=ipmux2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_UDP_MODULE:
      fifofrom=udp2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_TCP_MODULE:
      fifofrom=tcp2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_ICMP_MODULE:
      fifofrom=icmp2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_IP_OTHER_MODULE:
      fifofrom=ipother2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_SOCK_MODULE:
      fifofrom=sock2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_SOCKLIB_MODULE:
      fifofrom=socklib2mon_fifo_name;
      fifoto=0;
      break;
    case MINET_APP:
      fifofrom=app2mon_fifo_name;
      fifoto=0;
      break;
    default:
      Die("Invalid Accept.");
      break;
    }
    break;
  case MINET_READER:
    Die("Invalid Accept");
    break;
  case MINET_WRITER:
    Die("Invalid Accept");
    break;
  case MINET_DEVICE_DRIVER:
    switch (mod) {
    case MINET_ETHERNET_MUX:
      fifoto=ether2mux_fifo_name;
      fifofrom=mux2ether_fifo_name;
      break;
    default:
      Die("Invalid Accept.");
      break;
    }
    break;
  case MINET_ETHERNET_MUX:
    switch (mod) {
    case MINET_IP_MODULE:
      fifoto=mux2ip_fifo_name;
      fifofrom=ip2mux_fifo_name;
      break;
    case MINET_ARP_MODULE:
      fifoto=mux2arp_fifo_name;
      fifofrom=arp2mux_fifo_name;
      break;
    case MINET_OTHER_MODULE:
      fifoto=mux2other_fifo_name;
      fifofrom=other2mux_fifo_name;
      break;
    default:
      Die("Invalid Accept.");
      break;
    }
    break;
  case MINET_IP_MODULE:
    switch (mod) { 
    case MINET_IP_MUX:
      fifoto=ip2ipmux_fifo_name;
      fifofrom=ipmux2ip_fifo_name;
      break;
    default:
      Die("Invalid Accept.");
      break;
    }
    break;
  case MINET_ARP_MODULE:
    switch (mod) {
    case MINET_IP_MODULE:
      fifoto=arp2ip_fifo_name;
      fifofrom=ip2arp_fifo_name;
      break;
    default:
      Die("Invalid Accept.");
      break;
    }
    break;
  case MINET_OTHER_MODULE:
    // no passives
    Die("Invalid Accept.");
    break;
  case MINET_IP_MUX:
    switch (mod) {
    case MINET_ICMP_MODULE:
      fifoto=ipmux2icmp_fifo_name;
      fifofrom=icmp2ipmux_fifo_name;
      break;
    case MINET_UDP_MODULE:
      fifoto=ipmux2udp_fifo_name;
      fifofrom=udp2ipmux_fifo_name;
      break;
    case MINET_TCP_MODULE:
      fifoto=ipmux2tcp_fifo_name;
      fifofrom=tcp2ipmux_fifo_name;
      break;
    case MINET_IP_OTHER_MODULE:
      fifoto=ipmux2other_fifo_name;
      fifofrom=other2ipmux_fifo_name;
      break;
    default:
      Die("Invalid Accept.");
      break;
    }
    break;
  case MINET_ICMP_MODULE:
    switch (mod) { 
    case MINET_SOCK_MODULE:
      fifoto=icmp2sock_fifo_name;
      fifofrom=sock2icmp_fifo_name;
      break;
    default:
      Die("Invalid Accept.");
      break;
    }
    break;
  case MINET_UDP_MODULE:
    switch (mod) { 
    case MINET_SOCK_MODULE:
      fifoto=udp2sock_fifo_name;
      fifofrom=sock2udp_fifo_name;
      break;
    default:
      Die("Invalid Accept.");
      break;
    }
    break;
  case MINET_TCP_MODULE:
    switch (mod) { 
    case MINET_SOCK_MODULE:
      fifoto=tcp2sock_fifo_name;
      fifofrom=sock2tcp_fifo_name;
      break;
    default:
      Die("Invalid Accept.");
      break;
    }
    break;
  case MINET_IP_OTHER_MODULE:
    switch (mod) { 
    case MINET_SOCK_MODULE:
      fifoto=ipother2sock_fifo_name;
      fifofrom=sock2ipother_fifo_name;
      break;
    default:
      Die("Invalid Accept.");
      break;
    }
    break;
  case MINET_SOCK_MODULE:
    switch (mod) {
    case MINET_SOCKLIB_MODULE:
    case MINET_APP:
      fifoto=sock2socklib_fifo_name;
      fifofrom=socklib2sock_fifo_name;
      break;
    default:
      Die("Invalid Accept.");
      break;
    }
    break;
  case MINET_SOCKLIB_MODULE:
  case MINET_APP:
    // no passives
    Die("Invalid Accept.");
    break;
  case MINET_DEFAULT:
  default:
    Die("Unknown module!");
  }

  FifoData con;
  con.handle=MinetGetNextHandle();
  con.module=mod;

  con.to= fifoto !=0 ? open(fifoto,O_WRONLY) : -1;
  con.from= fifofrom!=0 ? open(fifofrom,O_RDONLY) : -1 ;

  MyFifos.push_back(con);

  MinetMonitoringEventDescription desc;

  desc.timestamp=Time();
  desc.source=MyModuleType;
  desc.from=mod;
  desc.to=MyModuleType;
  desc.datatype=MINET_MONITORINGEVENT;
  desc.optype=MINET_ACCEPT;

  MinetSendToMonitor(desc);

  return con.handle;
}



int         MinetClose(const MinetHandle &mh)
{
  MinetModule mod;
  Fifos::iterator x=MyFifos.FindMatching(mh);
  if (x!=MyFifos.end()) { 
    close((*x).from);
    close((*x).to);
    mod=(*x).module;
    MyFifos.erase(x);
  }
  MinetMonitoringEventDescription desc;

  desc.timestamp=Time();
  desc.source=MyModuleType;
  desc.from=MyModuleType;
  desc.to=mod;
  desc.datatype=MINET_MONITORINGEVENT;
  desc.optype=MINET_CLOSE;

  MinetSendToMonitor(desc);
  return 0;
}
					  
  

int MinetGetNextEvent(MinetEvent &event, double timeout=-1)
{
  int maxfd;
  fd_set read_fds;
  int rc;
  
  Time now;
  Time doneby((double)now+timeout);

  

  while (1) { 
    FD_ZERO(&read_fds);
    maxfd=-1;
    Fifos::iterator i;
    for (i=MyFifos.begin(); i!=MyFifos.end(); ++i) {
      FD_SET((*i).from, &read_fds);
      maxfd=MAX(maxfd,(*i).from);
    }
    if (timeout==-1 && maxfd==-1) { 
      MinetSendToMonitor(MinetMonitoringEvent("MinetGetNextEvent called without connections or timeout"));
      return -1;
    }

    if (timeout!=-1) {
      rc = select(maxfd+1, &read_fds,0,0,&doneby);
    } else {
      rc = select(maxfd+1, &read_fds,0,0,0);
    }
    if (rc<0) { 
      if (errno==EINTR) {
	continue;
      } else {
	MinetSendToMonitor(MinetMonitoringEvent("MinetGetNextEvent returning with unknown error"));
	return -1;
      }
    } else if (rc==0) {
      event.eventtype=MinetEvent::Timeout;
      event.direction=MinetEvent::NONE;
      event.handle=MINET_NOHANDLE;
      event.error=0;
      Time now;
      event.overtime=(double)now - (double) doneby;
      MinetSendToMonitor(MinetMonitoringEvent("MinetGetNextEvent returning with timeout"));
      return 0;
    } else {
      for (Fifos::iterator i=MyFifos.begin(); i!=MyFifos.end(); ++i) {
	if (FD_ISSET((*i).from, &read_fds)) {
	  event.eventtype=MinetEvent::Dataflow;
	  event.direction=MinetEvent::IN;
	  event.handle=(*i).handle;
	  event.error=0;
	  event.overtime=0.0;

	  MinetMonitoringEventDescription desc;
	  
	  desc.timestamp=Time();
	  desc.source=MyModuleType;
	  desc.from=(*i).module;
	  desc.to=MyModuleType;
	  desc.datatype=MINET_MONITORINGEVENT;
	  desc.optype=MINET_GETNEXTEVENT;
	  
	  MinetSendToMonitor(desc);

	  return 0;
	}
      }
    }
  }
}


#define MINET_IMPL(TYPE, MINETTYPE) 					\
int MinetMonitorSend(const MinetHandle &handle, const TYPE &obj)	\
{									\
  if (MyModuleType!=MINET_MONITOR) { 					\
    Fifos::iterator fifo = MyFifos.FindMatching(handle);		\
    if (fifo==MyFifos.end()) {						\
      return -1;							\
    } else {								\
      MinetMonitoringEventDescription desc;				\
      desc.timestamp=(double)Time();					\
      desc.source=MyModuleType;                                         \
      desc.from=MyModuleType;						\
      desc.to=(*fifo).module;						\
      desc.datatype = MINETTYPE;					\
      desc.optype= MINET_SEND;                                          \
      desc.Serialize(MyMonitorFifo);					\
      obj.Serialize(MyMonitorFifo);					\
    }									\
    return 0;								\
  } else {								\
    return 0;								\
  }									\
}									\
int MinetMonitorReceive(const MinetHandle &handle, TYPE &obj)	\
{									\
  if (MyModuleType!=MINET_MONITOR) { 					\
    Fifos::iterator fifo = MyFifos.FindMatching(handle);		\
    if (fifo==MyFifos.end()) {						\
      return -1;							\
    } else {								\
      MinetMonitoringEventDescription desc;				\
      desc.Unserialize(MyMonitorFifo);					\
      obj.Unserialize(MyMonitorFifo);					\
    }									\
    return 0;								\
  } else {								\
    return 0;								\
  }								\
}								\
int MinetSend(const MinetHandle &handle, const TYPE &object)	\
{								\
  if (MinetMonitorSend(handle,object)) { 			\
    return -1;							\
  } else {							\
    Fifos::iterator fifo=MyFifos.FindMatching(handle);		\
    if (fifo==MyFifos.end()) { 					\
      return -1;						\
    } else {							\
      object.Serialize((*fifo).to);				\
    }								\
    return 0;							\
  }								\
};								\
int MinetReceive(const MinetHandle &handle, TYPE &object)	\
{								\
  Fifos::iterator fifo=MyFifos.FindMatching(handle);		\
  if (fifo==MyFifos.end()) { 					\
    return -1;							\
  } else {							\
    object.Unserialize((*fifo).from);				\
    if (MinetMonitorReceive(handle,object)) {			\
      return -1;						\
    } else {							\
      return 0;							\
    }								\
  }								\
};


MINET_IMPL(MinetEvent,MINET_EVENT)
MINET_IMPL(MinetMonitoringEvent,MINET_MONITORINGEVENT)
MINET_IMPL(MinetMonitoringEventDescription,MINET_MONITORINGEVENTDESC)
MINET_IMPL(RawEthernetPacket, MINET_RAWETHERNETPACKET)
MINET_IMPL(Packet, MINET_PACKET)
MINET_IMPL(ARPRequestResponse, MINET_ARPREQUESTRESPONSE)
MINET_IMPL(SockRequestResponse, MINET_SOCKREQUESTRESPONSE)
MINET_IMPL(SockLibRequestResponse, MINET_SOCKLIBREQUESTRESPONSE)


int MinetSendToMonitor(const MinetMonitoringEvent &obj)
{
  if (MyMonitorFifo>0) { 
    MinetMonitoringEventDescription desc;
    desc.timestamp=Time();
    desc.source=MyModuleType;                                         
    desc.from=MyModuleType;
    desc.to=MINET_MONITOR;
    desc.datatype=MINET_MONITORINGEVENT;
    desc.optype=MINET_SENDTOMONITOR;
    desc.Serialize(MyMonitorFifo);
    obj.Serialize(MyMonitorFifo);
    return 0;
  } else {
    return 0;
  }
}


int MinetSendToMonitor(const MinetMonitoringEventDescription &desc, const MinetMonitoringEvent &obj)
{
  if (MyMonitorFifo>0) { 
    desc.Serialize(MyMonitorFifo);
    obj.Serialize(MyMonitorFifo);
    return 0;
  } else {
    return 0;
  }
}
#endif
