#ifndef _constate
#define _constate

#include "sockint.h"
#include <deque>
#include <sys/time.h>
#include <unistd.h>

#include <iostream>
#include <typeinfo>

// connection state mapping

// To be interchangeably used with timevals
// Note this is NOT virtual
struct Time : public timeval
{
  Time(const Time &rhs);
  Time(const timeval &rhs);
  Time(const double time);
  Time(const unsigned sec, const unsigned usec);
  Time();

  Time & operator=(const Time &rhs);
  Time & operator=(const double &rhs);

  operator double() const;

  bool operator<(const Time &rhs) const;
  bool operator>(const Time &rhs) const;
  bool operator==(const Time &rhs) const;

  ostream & Print(ostream &os) const;
};


template <class STATE>
struct ConnectionToStateMapping {
  Connection connection;
  Time       timeout;
  STATE      state;
  
  ConnectionToStateMapping(const ConnectionToStateMapping<STATE> &rhs) : 
    connection(rhs.connection), timeout(rhs.timeout), state(rhs.state) {}
  ConnectionToStateMapping(const Connection &c, 
			   const Time &t,
			   const STATE &s) :
    connection(c), timeout(t), state(s) {}
  ConnectionToStateMapping() : connection(), timeout(), state() {}

  ConnectionToStateMapping<STATE> & operator=(const ConnectionToStateMapping<STATE> &rhs) {
    connection=rhs.connection; timeout=rhs.timeout; state=rhs.state; return *this;
  }
  bool MatchesSource(const Connection &rhs) const {
    return connection.MatchesSource(rhs);
  }
  bool MatchesDest(const Connection &rhs) const {
    return connection.MatchesDest(rhs);
  }
  bool MatchesProtocol(const Connection &rhs) const {
    return connection.MatchesProtocol(rhs);
  }
  bool Matches(const Connection &rhs) const {
    return connection.Matches(rhs);
  }
  ostream & Print(ostream &os) const {
    os << "ConnectionToStateMapping(connection="<<connection<<", timeout="<<timeout<<", state="<<state<<")";
    return os;
  }
};

template <class STATE>
class ConnectionList : public deque<ConnectionToStateMapping<STATE> >
{
 public:
  ConnectionList(const ConnectionList &rhs) : deque<ConnectionToStateMapping<STATE> >(rhs) {}
  ConnectionList() {}

  ConnectionList<STATE>::iterator FindEarliest() {
    ConnectionList<STATE>::iterator ptr=begin();
    Time min=(*ptr).timeout;
    for (ConnectionList<STATE>::iterator i=ptr; i!=end(); ++i) {
      if ((*i).timeout < min) {
	min=(*i).timeout;
	ptr=i;
      }
    }
    return ptr;
  }
  

  ConnectionList<STATE>::iterator FindMatching(const Connection &rhs) {
    for (ConnectionList<STATE>::iterator i=begin(); i!=end(); ++i) {
      if ((*i).Matches(rhs)) {
	return i;
      }
    }
    return end();
  }
  ConnectionList<STATE>::iterator FindMatchingSource(const Connection &rhs) {
    for (ConnectionList<STATE>::iterator i=begin(); i!=end(); ++i) {
      if ((*i).MatchesSource(rhs)) {
	return i;
      }
    }
    return end();
  }
   ConnectionList<STATE>::iterator FindMatchingDest(const Connection &rhs) {
    for (ConnectionList<STATE>::iterator i=begin(); i!=end(); ++i) {
      if ((*i).MatchesDest(rhs)) {
	return i;
      }
    }
    return end();
  }
   ConnectionList<STATE>::iterator FindMatchingProtocol(const Connection &rhs) {
    for (ConnectionList<STATE>::iterator i=begin(); i!=end(); ++i) {
      if ((*i).MatchesProtocol(rhs)) {
	return i;
      }
    }
    return end();
  }

   ostream & Print(ostream &os) const {
     os << "ConnectionList(";
     for (const_iterator i=begin(); i!=end(); ++i) {
       os << (*i);
     }
     os << ")";
     return os;
   }
};



#endif
