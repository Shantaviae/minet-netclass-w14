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


#include "config.h"
#include "ip.h"
#include "arp.h"
#include "udp.h"
#include "packet.h"
#include "ethernet.h"
#include "raw_ethernet_packet.h"
#include "sockint.h"
#include "sock_mod_structs.h"

#define APP    1
#define TCP    1
#define UDP    1
#define ICMP   0

SockStatus socks;
PortStatus ports;

#if TCP
int fromtcp, totcp;
Queue tcpq;
#endif

#if UDP
int fromudp, toudp;
Queue udpq;
#endif

#if ICMP
int fromicmp, toicmp;
#endif

#if APP
int fromapp,  toapp;
#endif

#if TCP
void SendTCPRequest (SockRequestResponse *s, int sock) {
  s->Serialize(totcp);
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
#if APP
      appmsg = new SockLibRequestResponse(mSTATUS, 
					  s->connection,
					  sock,
					  s->data,
					  s->bytes,
					  s->error);
      appmsg->Serialize(toapp);
      delete appmsg;
#endif
      socks.SetStatus(sock, CONNECTED);
      s->error = EOK;
      s->bytes = s->data.GetSize();
      s->data = Buffer();
      break;
    case ACCEPT_PENDING:   // must remember to deal with port assignment
      socks.SetStatus(sock, LISTENING);
      if (s->error != EOK) {
#if APP
	appmsg = new SockLibRequestResponse(mSTATUS, 
					    s->connection,
					    sock,
					    s->data,
					    s->bytes,
					    s->error);
	appmsg->Serialize(toapp);
	delete appmsg;
#endif
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
#if APP
	socks.SetFifoToApp(newsock, toapp);
	socks.SetFifoFromApp(sock, fromapp);
	appmsg = new SockLibRequestResponse(mSTATUS,
					    s->connection,
					    newsock,
					    s->data,
					    s->bytes,
					    s->error);
	appmsg->Serialize(toapp);
	delete appmsg;
#endif
	break;
      }
      s->error = ERESOURCE_UNAVAIL;
      break;
    case CONNECT_PENDING:
      if (s->error != EOK) {
#if APP
	appmsg = new SockLibRequestResponse(mSTATUS, 
					    s->connection,
					    sock,
					    s->data,
					    s->bytes,
					    s->error);
	appmsg->Serialize(toapp);
	delete appmsg;
#endif
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
#if APP
      appmsg = new SockLibRequestResponse(mSTATUS, 
					  s->connection,
					  sock,
					  s->data,
					  s->bytes,
					  s->error);
      appmsg->Serialize(toapp);
      delete appmsg;
#endif
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
#if APP
	appmsg = new SockLibRequestResponse(mSTATUS, 
					    s->connection,
					    sock,
					    s->data,
					    s->bytes,
					    s->error);
	appmsg->Serialize(toapp);
	delete appmsg;
#endif
	socks.CloseSocket(sock);
      }
      break;
    case ACCEPT:
      if (s->error != EOK) {
#if APP
	appmsg = new SockLibRequestResponse(mSTATUS, 
					    s->connection,
					    sock,
					    s->data,
					    s->bytes,
					    s->error);
	appmsg->Serialize(toapp);
	delete appmsg;
#endif
	socks.SetStatus(sock, LISTENING);
      }
      break;
    case WRITE:
#if APP
      appmsg = new SockLibRequestResponse(mSTATUS, 
					  s->connection,
					  sock,
					  s->data,
					  s->bytes,
					  s->error);
      appmsg->Serialize(toapp);
      delete appmsg;
#endif
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
#endif

#if UDP
void SendUDPRequest (SockRequestResponse *s, int sock) {
  s->Serialize(toudp);
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
#if APP
      appmsg = new SockLibRequestResponse(mSTATUS, 
					  s->connection,
					  sock,
					  s->data,
					  s->bytes,
					  s->error);
      appmsg->Serialize(toapp);
      delete appmsg;
#endif
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
#if APP
      appmsg = new SockLibRequestResponse(mSTATUS, 
					  s->connection,
					  sock,
					  s->data,
					  s->bytes,
					  s->error);
      appmsg->Serialize(toapp);
      delete appmsg;
#endif
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
#if APP
      appmsg = new SockLibRequestResponse(mSTATUS, 
					  s->connection,
					  sock,
					  s->data,
					  s->bytes,
					  s->error);
      appmsg->Serialize(toapp);
      delete appmsg;
#endif
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
#endif


#if APP

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
      socks.SetFifoToApp(sock, toapp);
      socks.SetFifoFromApp(sock, fromapp);
      s.sockfd = sock;
      s.error = EOK;
    }
    else 
      s.error = ERESOURCE_UNAVAIL;
    break;
    
  case mBIND:
    sock = s.sockfd;
    if ((socks.GetStatus(sock) != UNBOUND) || 
	(toapp != socks.GetFifoToApp(sock))) {
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
#if UDP
      respond = 0;
      srr = new SockRequestResponse(FORWARD,
				    *c,
				    s.data,
				    s.bytes,
				    s.error);
      SendUDPRequest (srr, sock);
      break;
#else
      s.error = ENOT_IMPLEMENTED;
      break;
#endif
    }
    socks.SetStatus(sock, BOUND);
    s.error = EOK;
    break;
    
  case mLISTEN:
    sock = s.sockfd;
    if ((socks.GetStatus(sock) != BOUND) || 
	(toapp != socks.GetFifoToApp(sock))) {
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
	(toapp != socks.GetFifoToApp(sock))) {
      s.error = EINVALID_OP;
      break;
    }
    protocol = socks.GetConnection(sock)->protocol;
    if (protocol == IP_PROTO_UDP) {
      s.sockfd = 0;
      s.error = ENOT_SUPPORTED;
      break;
    }
#if TCP
    respond = 0;
    srr = new SockRequestResponse(ACCEPT,
				  *socks.GetConnection(sock),
				  s.data,
				  s.bytes,
				  s.error);
    SendTCPRequest (srr, sock);
    socks.SetStatus(sock, ACCEPT_PENDING);
    break;
#else
    s.sockfd = 0;
    s.error = ENOT_IMPLEMENTED;
    break;
#endif
    
  case mCONNECT:
    sock = s.sockfd;
    status = socks.GetStatus(sock);
    if ((toapp != socks.GetFifoToApp(sock)) ||
	((status != UNBOUND) && (status != BOUND))) {
      s.error = EINVALID_OP;
      break;
    }
    c = socks.GetConnection(sock);
    protocol = c->protocol;
    if (protocol == IP_PROTO_UDP) {
#if UDP
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
#else
      s.error = ENOT_IMPLEMENTED;
      break;
#endif
    } else {    
#if TCP
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
#else
      s.error = ENOT_IMPLEMENTED;
      break;
#endif
    }
    
  case mREAD:
    sock = s.sockfd;
    if (((socks.GetStatus(sock) != CONNECTED) &&
	 (! ((socks.GetStatus(sock) == BOUND) &&
	     (socks.GetConnection(sock)->protocol == IP_PROTO_UDP)))) || 
	(toapp != socks.GetFifoToApp(sock))) {
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
	(toapp != socks.GetFifoToApp(sock))) {
      s.bytes = 0;
      s.error = EINVALID_OP;
      break;
    }
    protocol = socks.GetConnection(sock)->protocol;
    if (protocol == IP_PROTO_UDP) {
#if UDP
      respond = 0;
      srr = new SockRequestResponse(WRITE,
				    *socks.GetConnection(sock),
				    s.data,
				    s.bytes,
				    s.error);
      SendUDPRequest (srr, sock);    
      socks.SetStatus(sock, WRITE_PENDING);
      break;
#else
      s.bytes = 0;
      s.error = ENOT_IMPLEMENTED;
      break;
#endif
    } else {
#if TCP
      respond = 0;
      srr = new SockRequestResponse(WRITE,
				    *socks.GetConnection(sock),
				    s.data,
				    s.bytes,
				    s.error);
      SendTCPRequest (srr, sock);    
      socks.SetStatus(sock, WRITE_PENDING);
      break;
#else
      s.bytes = 0;
      s.error = ENOT_IMPLEMENTED;
      break;
#endif
    }
  case mCLOSE:
    sock = s.sockfd;
    if (toapp != socks.GetFifoToApp(sock)) {
      s.error = EINVALID_OP;
      break;
    }
    protocol = socks.GetConnection(sock)->protocol;
    if (protocol == IP_PROTO_UDP) {
#if UDP
      srr = new SockRequestResponse(CLOSE,
				    *socks.GetConnection(sock),
				    s.data,
				    s.bytes,
				    s.error);
      SendUDPRequest (srr, sock);
      s.error = EOK;
#else
      s.error = ENOT_IMPLEMENTED;
#endif
    } else {
#if TCP
      srr = new SockRequestResponse(CLOSE,
				    *socks.GetConnection(sock),
				    s.data,
				    s.bytes,
				    s.error);
      SendTCPRequest (srr, sock);
      s.error = EOK;
#else
      s.error = ENOT_IMPLEMENTED;
#endif
    }
    socks.CloseSocket(sock);
    break;
    
  default:
    break;
  }
}
#endif

int main(int argc, char *argv[]) {
#if TCP
  fromtcp=open(tcp2sock_fifo_name,O_RDONLY);
  totcp=open(sock2tcp_fifo_name,O_WRONLY);
  if (totcp<0 || fromtcp<0) {
    cerr << "Can't open connection to TCP module\n";
    return -1;
  }
#endif

#if UDP
  fromudp=open(udp2sock_fifo_name,O_RDONLY);
  toudp=open(sock2udp_fifo_name,O_WRONLY);
  if (toudp<0 || fromudp<0) {
    cerr << "Can't open connection to UDP module\n";
    return -1;
  }
#endif

#if ICMP
  fromicmp=open(icmp2_fifo_name,O_RDONLY);
  toicmp=open(icmp2sock_fifo_name,O_WRONLY);
  if (toicmp<0 || fromicmp<0) {
    cerr << "Can't open connection to ICMP module\n";
    return -1;
  }
#endif

#if APP
  toapp=open(sock2app_fifo_name,O_WRONLY);
  fromapp=open(app2sock_fifo_name,O_RDONLY);
  if (toapp<0 || fromapp<0) {
    cerr << "Can't open connection to APP\n";
    return -1;
  }
#endif

  cerr << "sock_module fully armed and operational...\n";

  int maxfd;
  fd_set read_fds;
  int rc;

  while (1) {
    maxfd=0;
    FD_ZERO(&read_fds);
#if TCP
    FD_SET(fromtcp, &read_fds); 
    maxfd = MAX(maxfd, fromtcp);
#endif
#if UDP
    FD_SET(fromudp, &read_fds); 
    maxfd = MAX(maxfd, fromudp);
#endif
#if ICMP
    FD_SET(fromicmp, &read_fds); 
    maxfd = MAX(maxfd, fromicmp);
#endif
#if APP
    FD_SET(fromapp, &read_fds); 
    maxfd = MAX(maxfd, fromapp);
#endif

    rc=select(maxfd+1, &read_fds, 0, 0, 0);

    if (rc<0) { 
      if (errno==EINTR) { 
	continue;
      } else {
	cerr << "Unknown error in select\n";
	return -1;
      }
    } else if (rc==0) { 
      cerr << "Unexpected timeout in select\n";
      return -1;
    } else {
#if TCP
      if (FD_ISSET(fromtcp, &read_fds)) {
	int respond;
	SockRequestResponse *s = new SockRequestResponse;
	s->Unserialize(fromtcp);
	ProcessTCPMessage(s, respond);
	if (respond)
	  s->Serialize(totcp);
      }	
#endif
#if UDP
      // Fill this in later.
      if (FD_ISSET(fromudp, &read_fds)) { 
	int respond;
	SockRequestResponse *s = new SockRequestResponse;
	s->Unserialize(fromudp);
	ProcessUDPMessage(s, respond);
	if (respond)
	  s->Serialize(toudp);
      }
#endif
#if ICMP
      // Fill this in once ICMP is implemented :)
      if (FD_ISSET(fromicmp, &read_fds)) { 
	SockRequestResponse s;
	s.Unserialize(fromicmp);
	cerr << s << "\n";
      }
#endif
#if APP
      if (FD_ISSET(fromapp, &read_fds)) { 
	int respond;
	SockLibRequestResponse s; 
	s.Unserialize(fromapp);
	ProcessAppRequest(s, respond);
	if (respond)
	  s.Serialize(toapp);
      }
#endif
    }
  }
  return 0;
}
