#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream.h>

#include "Minet.h"

SockStatus socks;
PortStatus ports;

MinetHandle tcp;
Queue tcpq;

MinetHandle udp;
Queue udpq;

MinetHandle icmp;

MinetHandle ipother;

MinetHandle app;


void SendTCPRequest (SockRequestResponse *s, int sock) {
  MinetSend(tcp,*s);
  if (s->type != STATUS) {
    RequestRecord * elt = new RequestRecord(s, sock);
    tcpq.Insert((void *) elt);
  }
  else
    delete s;
}

void ProcessTCPMessage (SockRequestResponse * s, int & respond) {
  srrType type = s->type;
  RequestRecord *elt = NULL;
  SockRequestResponse * request = NULL;
  SockLibRequestResponse *appmsg = NULL;
  Buffer *b;
  Connection *c;
  int sock, newsock;

  switch (type) {

  case WRITE:
    respond = 1;
    s->type = STATUS;
    sock = socks.FindConnection(s->connection);
    if (sock <= 0) {
      s->bytes = 0;
      s->error = ENOMATCH;
      break;
    }
    switch (socks.GetStatus(sock)) {
    case CONNECTED:
      s->bytes = MIN(s->data.GetSize(),
		     (BIN_SIZE - socks.GetBin(sock)->GetSize()));
      if (s->bytes > 0) {
	socks.GetBin(sock)->AddBack(s->data.ExtractFront(s->bytes));
	s->error = EOK;
      }
      else {
	s->error = EBUF_SPACE;
      }
      s->data = Buffer();
      break;
    case READ_PENDING:     // This assumes that the App can read all the data
                           //    we are sending!
      if (app!=MINET_NOHANDLE) {
	appmsg = new SockLibRequestResponse(mSTATUS, 
					    s->connection,
					    sock,
					    s->data,
					    s->bytes,
					    s->error);
	MinetSend(app,*appmsg);
	delete appmsg;
      }
      socks.SetStatus(sock, CONNECTED);
      s->error = EOK;
      s->bytes = s->data.GetSize();
      s->data = Buffer();
      break;
    case ACCEPT_PENDING:   // must remember to deal with port assignment
      socks.SetStatus(sock, LISTENING);
      if (s->error != EOK) {
	if (app!=MINET_NOHANDLE) {
	  appmsg = new SockLibRequestResponse(mSTATUS, 
					      s->connection,
					      sock,
					      s->data,
					      s->bytes,
					      s->error);
	  MinetSend(app,*appmsg);
	  delete appmsg;
	  s->error = EOK;
	  break;
	}
	
	// Need to fix it so that when we close a socket the port is not made
	// available if another socket is still attached to it.  Also do error
	// checking to make sure that local connection didn't get muddled.
	
	newsock = socks.FindFreeSock();
	if (newsock > 0) {
	  c = socks.GetConnection(newsock);
	  c->src = s->connection.src;
	  c->dest = s->connection.dest;
	  c->srcport = s->connection.srcport;
	  c->destport = s->connection.destport;
	  c->protocol = s->connection.protocol;
	  socks.SetStatus(newsock, CONNECTED);
	  if (app!=MINET_NOHANDLE) {
	    socks.SetFifoToApp(newsock, app);
	    socks.SetFifoFromApp(sock, app);
	    appmsg = new SockLibRequestResponse(mSTATUS,
						s->connection,
						newsock,
						s->data,
						s->bytes,
						s->error);
	    MinetSend(app,*appmsg);
	    delete appmsg;
	    break;
	  }
	}
      }
      s->error = ERESOURCE_UNAVAIL;
      break;
    case CONNECT_PENDING:
      if (s->error != EOK) {
	if (app!=MINET_NOHANDLE) {
	  appmsg = new SockLibRequestResponse(mSTATUS, 
					      s->connection,
					      sock,
					      s->data,
					      s->bytes,
					      s->error);
	  MinetSend(app,*appmsg);
	  delete appmsg;
	}
	s->error = EOK;
	socks.CloseSocket(sock);
	break;
      }
      c = socks.GetConnection(sock);
      c->src = s->connection.src;
      c->dest = s->connection.dest;
      c->srcport = s->connection.srcport;
      c->destport = s->connection.destport;
      socks.SetStatus(sock, CONNECTED);
      if (app!=MINET_NOHANDLE) {
	
	appmsg = new SockLibRequestResponse(mSTATUS, 
					    s->connection,
					    sock,
					    s->data,
					    s->bytes,
					    s->error);
	MinetSend(app,*appmsg);
	delete appmsg;
      }
      
      break;
    default:
      respond = 1;
      s->type = STATUS;
      s->error = EINVALID_OP;
      break;
    }
    break;
    
  case STATUS:
    respond = 0;
    elt = (RequestRecord *) tcpq.Remove();
    request = elt->srr;
    sock = elt->sock;
    switch (request->type) {
    case CONNECT:
      if (s->error != EOK) {
	if (app!=MINET_NOHANDLE) {
	  appmsg = new SockLibRequestResponse(mSTATUS, 
					      s->connection,
					      sock,
					      s->data,
					      s->bytes,
					    s->error);
	  MinetSend(app,*appmsg);
	  delete appmsg;
	}
	socks.CloseSocket(sock);
      }
      break;
    case ACCEPT:
      if (s->error != EOK) {
	if (app!=MINET_NOHANDLE) {
	  appmsg = new SockLibRequestResponse(mSTATUS, 
					      s->connection,
					      sock,
					      s->data,
					      s->bytes,
					    s->error);
	  MinetSend(app,*appmsg);
	  delete appmsg;
	}
	socks.SetStatus(sock, LISTENING);
      }
      break;
    case WRITE:
      if (app!=MINET_NOHANDLE) {
	appmsg = new SockLibRequestResponse(mSTATUS, 
					    s->connection,
					    sock,
					    s->data,
					    s->bytes,
					  s->error);
	MinetSend(app,*appmsg);
	delete appmsg;
      }
      socks.SetStatus(sock, CONNECTED);
      break;
    }
    delete elt;
    break;
    
  default:
    respond = 1;
    s->type = STATUS;
    s->error = EINVALID_OP;
    break;
  }
}


void SendUDPRequest (SockRequestResponse *s, int sock) {
  MinetSend(udp,*s);
  if (s->type != STATUS) {
    RequestRecord * elt = new RequestRecord(s, sock);
    udpq.Insert((void *) elt);
  }
  else
    delete s;
}

void ProcessUDPMessage (SockRequestResponse * s, int & respond) {
  srrType type = s->type;
  RequestRecord *elt = NULL;
  SockRequestResponse * request = NULL;
  SockLibRequestResponse *appmsg = NULL;
  Buffer *b;
  int sock;
  
  switch (type) {
    
  case WRITE:
    respond = 1;
    s->type = STATUS;
    sock = socks.FindConnection(s->connection);
    if (sock <= 0) {
      s->bytes = 0;
      s->error = ENOMATCH;
      break;
    }
    switch (socks.GetStatus(sock)) {
    case CONNECTED:
      s->bytes = MIN(s->data.GetSize(),
		     (BIN_SIZE - socks.GetBin(sock)->GetSize()));
      if (s->bytes > 0) {
	socks.GetBin(sock)->AddBack(s->data.ExtractFront(s->bytes));
	s->error = EOK;
      }
      else {
	s->error = EBUF_SPACE;
      }
      s->data = Buffer();
      break;
    case READ_PENDING:     // This assumes that the App can read all the data
                           //    we are sending!
      if (app!=MINET_NOHANDLE) {
	appmsg = new SockLibRequestResponse(mSTATUS, 
					    s->connection,
					    sock,
					    s->data,
					    s->bytes,
					    s->error);
	MinetSend(app,*appmsg);
	delete appmsg;
      }
      socks.SetStatus(sock, CONNECTED);
      s->error = EOK;
      s->bytes = s->data.GetSize();
      s->data = Buffer();
      break;
    default:
      respond = 1;
      s->type = STATUS;
      s->error = EINVALID_OP;
      break;
    }
    break;

  case STATUS:
    respond = 0;
    elt = (RequestRecord *) udpq.Remove();
    request = elt->srr;
    sock = elt->sock;
    switch (request->type) {
    case FORWARD:
      if (app!=MINET_NOHANDLE) {
	appmsg = new SockLibRequestResponse(mSTATUS, 
					    s->connection,
					    sock,
					    s->data,
					    s->bytes,
					    s->error);
	MinetSend(app,*appmsg);
	delete appmsg;
      }
      if (s->error != EOK) {
	socks.CloseSocket(sock);
      }
      else {
	if ((socks.GetConnection(sock)->dest != IP_ADDRESS_ANY) &&
	    (socks.GetConnection(sock)->destport != PORT_ANY)) {
	  socks.SetStatus(sock, CONNECTED);
	}
	else {
	  socks.SetStatus(sock, BOUND);
	}
      }
      break;
    case WRITE:
      if (app!=MINET_NOHANDLE) {
	appmsg = new SockLibRequestResponse(mSTATUS, 
					    s->connection,
					    sock,
					    s->data,
					    s->bytes,
					    s->error);
	MinetSend(app,*appmsg);
	delete appmsg;
      }
      if ((socks.GetConnection(sock)->dest != IP_ADDRESS_ANY) &&
	  (socks.GetConnection(sock)->destport != PORT_ANY)) {
	socks.SetStatus(sock, CONNECTED);
      }
      else {
	socks.SetStatus(sock, BOUND);
      }
      break;
    }
    delete elt;
    break;

  default:
    respond = 1;
    s->type = STATUS;
    s->error = EINVALID_OP;
    break;
  }
}


int ResolveSrcPort (int sock, const Connection & c) {
  // If the source port in c is unbound, we try to find and reserve a port.
  // If it is bound, we check that it is available and reserve it.  In either 
  // case, if we are successfull then we return the (newly reserved) port.  If
  // we fail, we return -1.
  int port = c.srcport;
  IPAddress ip = c.src;
  if (port == PORT_NONE) {
    port = ports.FindFreePort(ip, sock);
    if (port < 0)
      return -1;
  }
  else if (ports.Socket(ip, port) != 0) 
    return -1;
  else
    ports.AssignPort(ip, port, sock);
  return port;
}

// request -> response (in place)
// respond=1 if response should be sent to app
void ProcessAppRequest(SockLibRequestResponse & s, int & respond)
{
  int sock, port;
  unsigned char protocol;
  Status status;
  SockRequestResponse * srr;
  Connection *c;

  slrrType type = s.type;

  respond = 1;
  s.type=mSTATUS;

  switch (type) {

  case mSOCKET:
    sock = socks.FindFreeSock();
    if (sock >= 0) {
      socks.SetStatus(sock, UNBOUND);
      c = socks.GetConnection(sock);
      c->protocol = s.connection.protocol;
      socks.SetFifoToApp(sock, app);
      socks.SetFifoFromApp(sock, app);
      s.sockfd = sock;
      s.error = EOK;
    }
    else 
      s.error = ERESOURCE_UNAVAIL;
    break;
    
  case mBIND:
    sock = s.sockfd;
    if ((socks.GetStatus(sock) != UNBOUND) || 
	(app != socks.GetFifoToApp(sock))) {
      s.error = EINVALID_OP;
      break;
    }    
    port = ResolveSrcPort(sock, s.connection);
    if (port < 0) {
      s.error = ERESOURCE_UNAVAIL;
      break;
    }
    s.connection.srcport = port;    
    if (s.connection.src == IP_ADDRESS_ANY) {
      s.connection.src = MyIPAddr;
    }
    c = socks.GetConnection(sock);
    c->src = s.connection.src;
    c->srcport = s.connection.srcport;
    protocol = c->protocol;
    if (protocol == IP_PROTO_UDP) {
      if (udp!=MINET_NOHANDLE) {
	respond = 0;
	srr = new SockRequestResponse(FORWARD,
				      *c,
				      s.data,
				      s.bytes,
				      s.error);
	SendUDPRequest (srr, sock);
	break;
      } else {
	s.error = ENOT_IMPLEMENTED;
	break;
      }
    }
    socks.SetStatus(sock, BOUND);
    s.error = EOK;
    break;
    
  case mLISTEN:
    sock = s.sockfd;
    if ((socks.GetStatus(sock) != BOUND) || 
	(app != socks.GetFifoToApp(sock))) {
      s.error = EINVALID_OP;
      break;
    }
    protocol = socks.GetConnection(sock)->protocol;
    if (protocol == IP_PROTO_UDP) {
      s.sockfd = 0;
      s.error = ENOT_SUPPORTED;
      break;
    }
    c = socks.GetConnection(sock);
    c->dest = IP_ADDRESS_ANY;
    c->destport = PORT_ANY;
    socks.SetStatus(sock, LISTENING);
    s.error = EOK;
    break;
    
  case mACCEPT:
    sock = s.sockfd;
    if ((socks.GetStatus(sock) != LISTENING) || 
	(app != socks.GetFifoToApp(sock))) {
      s.error = EINVALID_OP;
      break;
    }
    protocol = socks.GetConnection(sock)->protocol;
    if (protocol == IP_PROTO_UDP) {
      s.sockfd = 0;
      s.error = ENOT_SUPPORTED;
      break;
    }
    if (tcp!=MINET_NOHANDLE) {
      respond = 0;
      srr = new SockRequestResponse(ACCEPT,
				    *socks.GetConnection(sock),
				    s.data,
				    s.bytes,
				    s.error);
      SendTCPRequest (srr, sock);
      socks.SetStatus(sock, ACCEPT_PENDING);
      break;
    } else {
      s.sockfd = 0;
      s.error = ENOT_IMPLEMENTED;
      break;
    }
    
  case mCONNECT:
    sock = s.sockfd;
    status = socks.GetStatus(sock);
    if ((app != socks.GetFifoToApp(sock)) ||
	((status != UNBOUND) && (status != BOUND))) {
      s.error = EINVALID_OP;
      break;
    }
    c = socks.GetConnection(sock);
    protocol = c->protocol;
    if (protocol == IP_PROTO_UDP) {
      if (udp!=MINET_NOHANDLE) { 
	respond = 0;
	if (status == UNBOUND) { 
	  port =  ResolveSrcPort(sock, s.connection);
	  if (port < 0) {
	    respond = 1;
	    s.error = ERESOURCE_UNAVAIL;
	    break;
	  }
	  c->srcport = port;
	  if (s.connection.src == IP_ADDRESS_ANY) {
	    c->src = MyIPAddr;
	  } else {
	    c->src = s.connection.src;
	  }
	} 
	c->dest = s.connection.dest;
	c->destport = s.connection.destport;
	srr = new SockRequestResponse(FORWARD,
				      *c,
				      s.data,
				      s.bytes,
				      s.error);
	SendUDPRequest (srr, sock);
	socks.SetStatus(sock, CONNECTED);
	break;
      } else {
	s.error = ENOT_IMPLEMENTED;
	break;
      }
    } else {    
      if (tcp!=MINET_NOHANDLE) { 
	respond = 0;
	if (status == UNBOUND) { 
	  port =  ResolveSrcPort(sock, s.connection);
	  if (port < 0) {
	    respond = 1;
	    s.error = ERESOURCE_UNAVAIL;
	    break;
	  }
	  c->srcport = port;
	  if (s.connection.src == IP_ADDRESS_ANY) {
	    c->src = MyIPAddr;
	  } else {
	    c->src = s.connection.src;
	  }
	}
	c->dest = s.connection.dest;
	c->destport = s.connection.destport;
	srr = new SockRequestResponse(CONNECT,
				      *c,
				      s.data,
				      s.bytes,
				      s.error);
	SendTCPRequest (srr, sock);
	socks.SetStatus(sock, CONNECT_PENDING);
	break;
      } else {
	s.error = ENOT_IMPLEMENTED;
	break;
      }
    }
    
  case mREAD:
    sock = s.sockfd;
    if (((socks.GetStatus(sock) != CONNECTED) &&
	 (! ((socks.GetStatus(sock) == BOUND) &&
	     (socks.GetConnection(sock)->protocol == IP_PROTO_UDP)))) || 
	(app != socks.GetFifoToApp(sock))) {
      s.data.Clear();
      s.error = EINVALID_OP;
      break;
    }
    if (socks.GetBin(sock)->GetSize() > 0) {
      s.data = *socks.GetBin(sock);
      s.error = EOK;
      break;
    }
    if (! (socks.GetBlockingStatus(sock))) {
      s.data.Clear();
      s.error = EWOULD_BLOCK;
      break;
    }    
    respond = 0;
    socks.SetStatus(sock, READ_PENDING);
    break;
    
  case mWRITE:
    sock = s.sockfd;
    if ((socks.GetStatus(sock) != CONNECTED) || 
	(app != socks.GetFifoToApp(sock))) {
      s.bytes = 0;
      s.error = EINVALID_OP;
      break;
    }
    protocol = socks.GetConnection(sock)->protocol;
    if (protocol == IP_PROTO_UDP) {
      if (udp!=MINET_NOHANDLE) { 
	respond = 0;
	srr = new SockRequestResponse(WRITE,
				      *socks.GetConnection(sock),
				      s.data,
				      s.bytes,
				      s.error);
	SendUDPRequest (srr, sock);    
	socks.SetStatus(sock, WRITE_PENDING);
	break;
      } else {
	s.bytes = 0;
	s.error = ENOT_IMPLEMENTED;
	break;
      }
    } else {
      if (tcp!=MINET_NOHANDLE) { 
	respond = 0;
	srr = new SockRequestResponse(WRITE,
				      *socks.GetConnection(sock),
				      s.data,
				      s.bytes,
				      s.error);
	SendTCPRequest (srr, sock);    
	socks.SetStatus(sock, WRITE_PENDING);
	break;
      } else {
	s.bytes = 0;
	s.error = ENOT_IMPLEMENTED;
	break;
      }
    }
  case mCLOSE:
    sock = s.sockfd;
    if (app != socks.GetFifoToApp(sock)) {
      s.error = EINVALID_OP;
      break;
    }
    protocol = socks.GetConnection(sock)->protocol;
    if (protocol == IP_PROTO_UDP) {
      if (udp!=MINET_NOHANDLE) { 
	srr = new SockRequestResponse(CLOSE,
				      *socks.GetConnection(sock),
				      s.data,
				      s.bytes,
				      s.error);
	SendUDPRequest (srr, sock);
	s.error = EOK;
      } else {
	s.error = ENOT_IMPLEMENTED;
      }
    } else {
      if (tcp!=MINET_NOHANDLE) { 
	srr = new SockRequestResponse(CLOSE,
				      *socks.GetConnection(sock),
				      s.data,
				      s.bytes,
				      s.error);
	SendTCPRequest (srr, sock);
	s.error = EOK;
      } else {
	s.error = ENOT_IMPLEMENTED;
      }
    }
    socks.CloseSocket(sock);
    break;
    
  default:
    break;
  }
}

int main(int argc, char *argv[]) {

  MinetInit(MINET_SOCK_MODULE);
  
  tcp=MinetIsModuleInConfig(MINET_TCP_MODULE) ? MinetConnect(MINET_TCP_MODULE) : MINET_NOHANDLE;
  udp=MinetIsModuleInConfig(MINET_UDP_MODULE) ? MinetConnect(MINET_UDP_MODULE) : MINET_NOHANDLE;
  icmp=MinetIsModuleInConfig(MINET_ICMP_MODULE) ? MinetConnect(MINET_ICMP_MODULE) : MINET_NOHANDLE;
  ipother=MinetIsModuleInConfig(MINET_IP_OTHER_MODULE) ? MinetConnect(MINET_IP_OTHER_MODULE) : MINET_NOHANDLE;
  app=MinetIsModuleInConfig(MINET_APP) ? MinetAccept(MINET_APP) : MinetIsModuleInConfig(MINET_SOCKLIB_MODULE) ? MinetAccept(MINET_SOCKLIB_MODULE) : MINET_NOHANDLE;
  
  if (tcp==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_TCP_MODULE)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to tcp module"));
    return -1;
  }
  if (udp==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_UDP_MODULE)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to udp module"));
    return -1;
  }
  if (icmp==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_ICMP_MODULE)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to icmp module"));
    return -1;
  }
  if (ipother==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_IP_OTHER_MODULE)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to ipother module"));
    return -1;
  }
  if (app==MINET_NOHANDLE && (MinetIsModuleInConfig(MINET_APP) || MinetIsModuleInConfig(MINET_SOCKLIB_MODULE))) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to app or socklib module"));
    return -1;
  }
  
  
  MinetSendToMonitor(MinetMonitoringEvent("sock_module fully armed and operational"));
		     
		     
  MinetEvent event;

  while (MinetGetNextEvent(event)==0) {
    if (event.eventtype!=MinetEvent::Dataflow 
	|| event.direction!=MinetEvent::IN) {
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    } else {
      if (event.handle==tcp) {
	int respond;
	SockRequestResponse *s = new SockRequestResponse;
	MinetReceive(tcp,*s);
	ProcessTCPMessage(s, respond);
	if (respond)
	  MinetSend(tcp,*s);
      }	
      if (event.handle==udp) {
	int respond;
	SockRequestResponse *s = new SockRequestResponse;
	MinetReceive(udp,*s);
	ProcessUDPMessage(s, respond);
	if (respond)
	  MinetSend(udp,*s);
      }
      if (event.handle==icmp) {
	SockRequestResponse s;
	MinetReceive(icmp,s);
	MinetSendToMonitor(MinetMonitoringEvent("Ignoring request from icmp - unimplemented"));
      }
      if (event.handle==app) {
	int respond;
	SockLibRequestResponse s; 
	MinetReceive(app,s);
	ProcessAppRequest(s, respond);
	if (respond)
	  MinetSend(app,s);
      }
    }
  }
  return 0;
}
