#ifndef _Minet
#define _Minet

#include <iostream>

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



enum MinetDatatype {
  MINET_NONE,
  MINET_EVENT,
  MINET_MONITORINGEVENT,
  MINET_RAWETHERNETPACKET,
  MINET_PACKET,
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


int         MinetInit(const MinetModule &mod);
int         MinetDeinit();

MinetHandle MinetConnect(const MinetModule &mod);
MinetHandle MinetAccept(const MinetModule &mod);
int         MinetClose(const MinetHandle &mh);

MinetEvent &MinetGetNextEvent(double timeout=-1);


template <class T> 
int MinetSend(const MinetHandle &handle, const T &object);  
template <class T> 
int MinetReceive(const MinetHandle &handle, T &object);

#endif
