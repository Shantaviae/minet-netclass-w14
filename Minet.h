#ifndef _Minet
#define _Minet


typedef int MinetHandle;

enum MinetModule {
  MINET_MONITOR,
  MINET_READER,
  MINET_WRITER,
  MINET_DEVICE_DRIVER,
  MINET_ETHERNET_MUX,
  MINET_IP_MODULE,
  MINET_ARP_MODULE,
  MINET_OTHER_MODULE,
  MINET_IP_MUX,
  MINET_IP_OTHER_MODULE,
  MINET_ICMP_MODULE,
  MINET_UDP_MODULE,
  MINET_TCP_MODULE,
  MINET_SOCK_MODULE,
  MINET_SOCKLIB_MODULE,
  MINET_APP,
  MINET_DEFAULT,
};


struct MinetEvent {
   enum {Dataflow, Monitor, Exception, Timeout, Error } eventtype;
   enum {IN, OUT, INOUT, NONE}                          direction; 
   int                                                  handle;
   int                                                  errno;
   double                                               overtime;
};


int         MinetInit(const MinetModule &mod);
int         MinetDeinit();

MinetHandle MinetConnect(const MinetModule &mod, 
                         const MinetModule &connectas=MINET_DEFAULT);
MinetHandle MinetAccept(const MinetModule &mod, 
                        bool  await=false);  
int         MinetClose(const MinetHandle &mh);

MinetEvent &MinetGetNextEvent(double timeout=-1);


template <class T> 
int MinetSend(const MinetHandle &handle, const T &object);  
template <class T> 
int MinetReceive(const MinetHandle &handle, T &object);

