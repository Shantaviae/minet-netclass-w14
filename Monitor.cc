#include "Minet.h"
#include "Monitor.h"
#include "util.h"


MinetMonitoringEventDescription::MinetMonitoringEventDescription() :
  timestamp(0.0),
  from(MINET_DEFAULT),
  to(MINET_DEFAULT),
  datatype(MINET_NONE)
{}

MinetMonitoringEventDescription::MinetMonitoringEventDescription(const MinetMonitoringEventDescription &rhs) 
{
  *this = rhs;
}


const MinetMonitoringEventDescription &MinetMonitoringEventDescription::operator= (const MinetMonitoringEventDescription &rhs) 
{
  timestamp=rhs.timestamp;
  from=rhs.from;
  to=rhs.to;
  datatype=rhs.datatype;
  return *this;
}


MinetMonitoringEventDescription::~MinetMonitoringEventDescription()
{}

void MinetMonitoringEventDescription::Serialize(const int fd) const
{
  if (writeall(fd,(char*)&timestamp,sizeof(timestamp))!=sizeof(timestamp)) {
    throw SerializationException();
  }
  if (writeall(fd,(char*)&from,sizeof(from))!=sizeof(from)) {
    throw SerializationException();
  }
  if (writeall(fd,(char*)&to,sizeof(to))!=sizeof(to)) {
    throw SerializationException();
  }
  if (writeall(fd,(char*)&datatype,sizeof(datatype))!=sizeof(datatype)) {
    throw SerializationException();
  }
}

void MinetMonitoringEventDescription::Unserialize(const int fd)
{
  if (readall(fd,(char*)&timestamp,sizeof(timestamp))!=sizeof(timestamp)) {
    throw SerializationException();
  }
  if (readall(fd,(char*)&from,sizeof(from))!=sizeof(from)) {
    throw SerializationException();
  }
  if (readall(fd,(char*)&to,sizeof(to))!=sizeof(to)) {
    throw SerializationException();
  }
  if (readall(fd,(char*)&datatype,sizeof(datatype))!=sizeof(datatype)) {
    throw SerializationException();
  }
}


ostream & MinetMonitoringEventDescription::Print(ostream &os) const
{
  os << "MinetMonitoringEventDescription(timestamp="<<timestamp
     << ", from="<<from<<", to="<<to<<", datatype="<<datatype<<")";
  return os;
}


MinetMonitoringEvent::MinetMonitoringEvent() : string("")
{}


MinetMonitoringEvent::MinetMonitoringEvent(const string &s) : string(s)
{}


MinetMonitoringEvent::MinetMonitoringEvent(const char *s) : string(s)
{}



MinetMonitoringEvent::MinetMonitoringEvent(const MinetMonitoringEvent &rhs)
{
  *this=rhs;
}

const MinetMonitoringEvent &MinetMonitoringEvent::operator=(const MinetMonitoringEvent &rhs)
{
  ((string*)this)->operator=(rhs);
  return *this;
}

MinetMonitoringEvent::~MinetMonitoringEvent()
{}

void MinetMonitoringEvent::Serialize(const int fd) const
{
  char *buf=(char*)this;
  int len=strlen(buf);
  if (writeall(fd,(char*)&len,sizeof(len))!=sizeof(len)) { 
    throw SerializationException();
  }
  if (writeall(fd,buf,len)!=len) { 
    throw SerializationException();
  }
}

void MinetMonitoringEvent::Unserialize(const int fd) 
{
  int len;
  if (readall(fd,(char*)&len,sizeof(len))!=sizeof(len)) { 
    throw SerializationException();
  }
  char buf[len+1];
  if (readall(fd,buf,len)!=len) { 
    throw SerializationException();
  }
  buf[len]=0;
  this->operator=(MinetMonitoringEvent(buf));
}


ostream & MinetMonitoringEvent::Print(ostream &os) const
{
  os << "MinetMonitoringEvent("<<(*((string *)this))<<")";`
  return os;
}


