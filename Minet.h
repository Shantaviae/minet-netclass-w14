#ifndef _Minet
#define _Minet

#include <iostream>
#include <string>

#include "config.h"

#include "buffer.h"
#include "debug.h"
#include "error.h"
#include "util.h"

#include "raw_ethernet_packet.h"
#include "ethernet.h"

#include "arp.h"

#include "headertrailer.h"
#include "packet.h"

#include "ip.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"

#include "sock.h"
#include "sockint.h"
#include "sock_mod_structs.h"
#include "constate.h"



typedef int MinetHandle;

const MinetHandle MINET_NOHANDLE=-1;

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



enum MinetDatatype {
  MINET_NONE,
  MINET_EVENT,
  MINET_MONITORINGEVENT,
  MINET_RAWETHERNETPACKET,
  MINET_PACKET,
  MINET_ARPREQUESTRESPONSE,
  MINET_SOCKREQUESTRESPONSE,
  MINET_SOCKLIBREQUESTRESPONSE,
};

ostream & operator<<(ostream &os, const MinetModule &mon);
ostream & operator<<(ostream &os, const MinetDatatype &t);


struct MinetEvent {
  enum {Dataflow, Exception, Timeout, Error }          eventtype;
  enum {IN, OUT, INOUT, NONE}                          direction; 
  MinetHandle                                          handle;
  int                                                  error;
  double                                               overtime;

  MinetEvent();
  MinetEvent(const MinetEvent &rhs);
  virtual ~MinetEvent();

  virtual const MinetEvent & operator= (const MinetEvent &rhs);

  virtual void Serialize(const int fd) const;
  virtual void Unserialize(const int fd);

  virtual ostream & Print(ostream &os) const;
};


class MinetException : public string {
 private:
  MinetException() {}

 public:
  MinetException(const MinetException &rhs) : string(rhs) {}
  MinetException(const string &rhs) : string(rhs) {}
  MinetException(const char *rhs) : string(rhs) {}
  virtual ~MinetException() {}
  const MinetException &operator=(const MinetException &rhs) 
    { ((string*)this)->operator=((const string &)rhs);}
  virtual ostream & Print(ostream &os) const 
    { os << "MinetException("<<((const string &)(*this))<<")"; }
};


int         MinetInit(const MinetModule &mod);
int         MinetDeinit();

MinetHandle MinetConnect(const MinetModule &mod);
MinetHandle MinetAccept(const MinetModule &mod);
int         MinetClose(const MinetHandle &mh);

int         MinetGetNextEvent(MinetEvent &event, double timeout=-1);


#define MINET_DECL(TYPE)					\
int MinetSend(const MinetHandle &handle, const TYPE &object);	        \
int MinetReceive(const MinetHandle &handle, const TYPE &object);        \
int MinetMonitorSend(const MinetHandle &handle, const TYPE &object);	\
int MinetMonitorReceive(const MinetHandle &handle, const TYPE &object); \


MINET_DECL(MinetEvent)
MINET_DECL(MinetMonitoringEvent)
MINET_DECL(RawEthernetPacket)
MINET_DECL(Packet)
MINET_DECL(ARPRequestResponse)
MINET_DECL(SockRequestResponse)
MINET_DECL(SockLibRequestResponse)

#include "Monitor.h"

int MinetSendToMonitor(const MinetMonitoringEvent &object);


#endif
