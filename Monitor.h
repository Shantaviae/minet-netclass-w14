#ifndef _Monitor
#define _Monitor

#include <iostream>
#include <string>
#include "Minet.h"




struct MinetMonitoringEventDescription {
  double         timestamp;
  MinetModule    from;
  MinetModule    to;
  MinetDatatype  datatype;

  MinetMonitoringEventDescription();
  MinetMonitoringEventDescription(const MinetMonitoringEventDescription &rhs);
  virtual ~MinetMonitoringEventDescription();
  
  virtual const MinetMonitoringEventDescription & operator= (const MinetMonitoringEventDescription &rhs);

  virtual void Serialize(const int fd) const;
  virtual void Unserialize(const int fd);

  virtual ostream & Print(ostream &os) const;
};
  

struct MinetMonitoringEvent : public string {
  MinetMonitoringEvent();
  MinetMonitoringEvent(const char *s);
  MinetMonitoringEvent(const string &s);
  MinetMonitoringEvent(const MinetMonitoringEvent &rhs);
  virtual ~MinetMonitoringEvent();

  virtual const MinetMonitoringEvent & operator= (const MinetMonitoringEvent &rhs);

  virtual void Serialize(const int fd) const;
  virtual void Unserialize(const int fd);

  virtual ostream & Print(ostream &os) const;
};




#endif



  
