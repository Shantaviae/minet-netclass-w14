/*******************************************************************************
 * Project part B
 *
 * Author: Jason Skicewicz
 *
 * Main TCP state machine file.  Needs more testing, but contains base,
 *  normal functionality of TCP.
 *
 ******************************************************************************/

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

#include <string.h>
#include <iostream>

#include "tcpstate.h"
#include "Minet.h"

#define SIM_PASSIVE_CLOSE 0

// Timeout dispatch functions
static void TONull(ConnectionList<TCPState>::iterator &cptr, Time &current_time);
static void TOSynRcvd(ConnectionList<TCPState>::iterator &cptr, Time &current_time);
static void TOSynSent(ConnectionList<TCPState>::iterator &cptr, Time &current_time);
static void TOSynSent1(ConnectionList<TCPState>::iterator &cptr, Time &current_time);
static void TOEstablished(ConnectionList<TCPState>::iterator &cptr, Time &current_time);
static void TOSendData(ConnectionList<TCPState>::iterator &cptr, Time &current_time);
static void TOCloseWait(ConnectionList<TCPState>::iterator &cptr, Time &current_time);
static void TOFinWait1(ConnectionList<TCPState>::iterator &cptr, Time &current_time);
static void TOClosing(ConnectionList<TCPState>::iterator &cptr, Time &current_time);
static void TOLastAck(ConnectionList<TCPState>::iterator &cptr, Time &current_time);
static void TOTimeWait(ConnectionList<TCPState>::iterator &cptr, Time &current_time);

// The dispatch handler
static void TOHandler(ConnectionList<TCPState>::iterator &cptr,
		      Time &current_time,
		      void (*to_function)(ConnectionList<TCPState>::iterator &, Time &)) {
  return(to_function(cptr, current_time));
}

typedef void (*pt2TOFunction)(ConnectionList<TCPState>::iterator &, Time &);
pt2TOFunction TOFuncArray[NUM_TCP_STATES] = 
  { &TONull,         // CLOSED
    &TONull,         // LISTEN
    &TOSynRcvd,      // SYN_RCVD
    &TOSynSent,      // SYN_SENT
    &TOSynSent1,     // SYN_SENT1
    &TOEstablished,  // ESTABLISHED
    &TOSendData,     // SEND_DATA
    &TOCloseWait,    // CLOSE_WAIT
    &TOFinWait1,     // FIN_WAIT1
    &TOClosing,      // CLOSING
    &TOLastAck,      // LAST_ACK
    &TONull,         // FIN_WAIT2
    &TOTimeWait      // TIME_WAIT
  };
    
// IP dataflow dispatch functions
static void DFIPNull(ConnectionList<TCPState>::iterator &cptr, 
		     TCPHeader &tcph, 
		     Buffer &data,
		     Time &current_time);
static void DFIPListen(TCPHeader &tcph, Connection &c, Time &current_time);
static void DFIPSynRcvd(ConnectionList<TCPState>::iterator &cptr, 
			TCPHeader &tcph,
			Buffer &data,
			Time &current_time);
static void DFIPSynSent(ConnectionList<TCPState>::iterator &cptr,
			TCPHeader &tcph,
			Buffer &data,
			Time &current_time);
static void DFIPSynSent1(ConnectionList<TCPState>::iterator &cptr,
			 TCPHeader &tcph,
			 Buffer &data,
			 Time &current_time);
static void DFIPEstablished(ConnectionList<TCPState>::iterator &cptr,
			    TCPHeader &tcph,
			    Buffer &data,
			    Time &current_time);
static void DFIPSendData(ConnectionList<TCPState>::iterator &cptr,
			 TCPHeader &tcph,
			 Buffer &data,
			 Time &current_time);
static void DFIPCloseWait(ConnectionList<TCPState>::iterator &cptr,
			  TCPHeader &tcph,
			  Buffer &data,
			  Time &current_time);
static void DFIPFinWait1(ConnectionList<TCPState>::iterator &cptr,
			 TCPHeader &tcph,
			 Buffer &data,
			 Time &current_time);
static void DFIPClosing(ConnectionList<TCPState>::iterator &cptr,
			TCPHeader &tcph,
			Buffer &data,
			Time &current_time);
static void DFIPLastAck(ConnectionList<TCPState>::iterator &cptr,
			TCPHeader &tcph,
			Buffer &data,
			Time &current_time);
static void DFIPFinWait2(ConnectionList<TCPState>::iterator &cptr, 
			 TCPHeader &tcph,
			 Buffer &data,
			 Time &current_time);
static void DFIPTimeWait(ConnectionList<TCPState>::iterator &cptr, 
			 TCPHeader &tcph,
			 Buffer &data,
			 Time &current_time);

// The dispatch handler
static void DFIPHandler(ConnectionList<TCPState>::iterator &cptr,
			TCPHeader &tcph,
			Buffer &data,
			Time &current_time,
			void (*dfip_function)(ConnectionList<TCPState>::iterator &,
					      TCPHeader &,
					      Buffer &,
					      Time &) ) {
  return(dfip_function(cptr, tcph, data, current_time));
}

typedef void (*pt2DFIPFunction)(ConnectionList<TCPState>::iterator &,
				TCPHeader &,
				Buffer &,
				Time &);
pt2DFIPFunction DFIPFuncArray[NUM_TCP_STATES] = 
  { &DFIPNull,         // CLOSED
    &DFIPNull,         // LISTEN *DFIPListen is not dispatched from this table*
    &DFIPSynRcvd,      // SYN_RCVD
    &DFIPSynSent,      // SYN_SENT
    &DFIPSynSent1,     // SYN_SENT1
    &DFIPEstablished,  // ESTABLISHED
    &DFIPSendData,     // SEND_DATA
    &DFIPCloseWait,    // CLOSE_WAIT
    &DFIPFinWait1,     // FIN_WAIT1
    &DFIPClosing,      // CLOSING
    &DFIPLastAck,      // LAST_ACK
    &DFIPFinWait2,     // FIN_WAIT2
    &DFIPTimeWait      // TIME_WAIT
  };

// Sock dataflow dispatch functions
static void DFSOCKConnect(SockRequestResponse &req);
static void DFSOCKAccept(SockRequestResponse &req);
static void DFSOCKWrite(SockRequestResponse &req);
static void DFSOCKForward(SockRequestResponse &req);
static void DFSOCKClose(SockRequestResponse &req);
static void DFSOCKStatus(SockRequestResponse &req);

// The dispatch handler
static void DFSOCKHandler(SockRequestResponse &req,
			  void (*dfsock_function)(SockRequestResponse &)) {
  return(dfsock_function(req));
}

typedef void (*pt2DFSOCKFunction)(SockRequestResponse &);
pt2DFSOCKFunction DFSOCKFuncArray[NUM_SOCK_TYPES] = 
  { &DFSOCKConnect, // CONNECT
    &DFSOCKAccept,  // ACCEPT
    &DFSOCKWrite,   // WRITE
    &DFSOCKForward, // FORWARD
    &DFSOCKClose,   // CLOSE
    &DFSOCKStatus   // STATUS
  };

// General functions
static unsigned UpdateSequenceNumber(Time &current_time, Time &seqtime, unsigned num);
static bool GetNextTimeoutValue(Time &current_time, Time &select_timeout);
static void PrintConnectionList();

// Worker functions
static void SendSyn(ConnectionList<TCPState>::iterator &ptr);
static void SendFin(ConnectionList<TCPState>::iterator &ptr);
static void SendSynAck(ConnectionList<TCPState>::iterator &ptr);
static void SendAck(ConnectionList<TCPState>::iterator &ptr);
static void SendRst(ConnectionList<TCPState>::iterator &ptr);
static void SendData(ConnectionList<TCPState>::iterator &ptr);
static void ConnectionFailed(ConnectionList<TCPState>::iterator &ptr);

// Packet creation functions
static Packet CreatePacket(ConnectionList<TCPState>::iterator &ptr, 
			   unsigned char flags);

static Packet CreatePayloadPacket(ConnectionList<TCPState>::iterator &ptr,
				  unsigned &bytesize,
				  unsigned int datasize,
				  unsigned char flags);

// Extraction functions
static bool ExtractInfoFromIPPacket(TCPHeader &tcph, Connection &c, Buffer &data);

// Global memory
MinetHandle ipmux;
MinetHandle sock;
ConnectionList<TCPState> clist;
unsigned initSeqNum;

int main(int argc, char *argv[])
{
  int rc;
  bool activetmr = false;
  initSeqNum=0;
  MinetEvent event;
  Time seqtime, two_msl;

  MinetInit(MINET_TCP_MODULE);
  ipmux = MinetIsModuleInConfig(MINET_IP_MUX) ? MinetConnect(MINET_IP_MUX) : MINET_NOHANDLE;
  sock = MinetIsModuleInConfig(MINET_SOCK_MODULE) ? MinetAccept(MINET_SOCK_MODULE) : MINET_NOHANDLE;

  if (ipmux==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_IP_MUX)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't open connection to IP mux"));
    exit(-1);
  }      
  if (sock==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_SOCK_MODULE)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't open connection to sock module"));
    exit(-1);
  }
  
  cerr << "tcp_module operational\n";
  MinetSendToMonitor(MinetMonitoringEvent("tcp_module operational"));

  cerr << "tcp_module handling TCP traffic\n";
    
  // Initialize sequence time to current time, and initial sequence number to 0
  if (gettimeofday(&seqtime, 0) < 0) {
    cerr << "Can't obtain the current time.\n";
    exit(-1);
  }

  cerr << "The current time is: " << seqtime << endl;
  initSeqNum += (seqtime.tv_usec * MICROSEC_MULTIPLIER) & SEQ_LENGTH_MASK;
  initSeqNum += (seqtime.tv_sec * SECOND_MULTIPLIER) & SEQ_LENGTH_MASK;
  cerr << "The initial sequence number is: " << initSeqNum << endl;

  // Wait 2MSLs (4minutes) or 5 seconds as a hack before starting
  two_msl = seqtime;
  two_msl.tv_sec += 5;
  //two_msl.tv_sec += 2*MSL_TIME_SECS;
  while(two_msl > seqtime) {
    if (gettimeofday(&seqtime, 0) < 0) {
      cerr << "Can't obtain the current time.\n";
      exit(-1);
    }
  }

  cerr << "Starting main processing loop.\n\n";
  while (1) {
    Time current_time, select_timeout;
    if (gettimeofday(&current_time, 0) < 0) {
      cerr << "Can't obtain the current time.\n";
      exit(-1);
    }


    initSeqNum = UpdateSequenceNumber(current_time, seqtime, initSeqNum);
    cerr << "Current sequence number is: " << initSeqNum << endl;

    activetmr = GetNextTimeoutValue(current_time, select_timeout);
    cerr << "select timeout: " << (double)select_timeout << endl;

    if(activetmr == false) {
      cerr << "Active timer is FALSE." << endl;
      rc=MinetGetNextEvent(event);
    } else {
      cerr << "Active timer is TRUE." << endl;
      rc=MinetGetNextEvent(event,(double)select_timeout);
    }

    if (rc<0) {
      break;
    } else if (event.eventtype==MinetEvent::Timeout) { 
      cerr << "TIMEOUT EVENT." << endl;

      // Handle all connections that have incurred a timeout
      ConnectionList<TCPState>::iterator next = clist.FindEarliest();
      while(((*next).timeout < current_time) && (next != clist.end())) {
	cerr << "TIMEOUT LOOP." << endl;

	if (gettimeofday(&current_time, 0) < 0) {
	  cerr << "Can't obtain the current time.\n";
	  exit(-1);
	}

	// Check the state of each timed out connection
	TOHandler(next, current_time, TOFuncArray[(*next).state.GetState()]);
	next = clist.FindEarliest();
      }
    } else if (event.eventtype==MinetEvent::Dataflow && event.direction==MinetEvent::IN) {

      if (event.handle==ipmux) {
	cerr << "DATAFLOW IP EVENT." << endl;

	if (gettimeofday(&current_time, 0) < 0) {
	  cerr << "Can't obtain the current time.\n";
	  exit(-1);
	}

	TCPHeader tcph;
	Connection c;
	Buffer data;
	if ( ExtractInfoFromIPPacket(tcph, c, data) ) {

	  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
	  if(cs != clist.end()) {

	    if ((*cs).state.GetState() == LISTEN) {
	      // May need to create a new connection based on incoming packet
	      DFIPListen(tcph, c, current_time);
	    } else {
	      unsigned short window;
	      tcph.GetWinSize(window);
	      (*cs).state.SetSendRwnd(window);
	      DFIPHandler(cs, 
			  tcph,
			  data,
			  current_time,
			  DFIPFuncArray[(*cs).state.GetState()]);
	    }
	  }
	}
      }

      if (event.handle==sock) {
	cerr << "DATAFLOW SOCK EVENT." << endl;
	SockRequestResponse req;
	MinetReceive(sock,req);
	DFSOCKHandler(req, DFSOCKFuncArray[req.type]);
      }
    }
  }
  MinetDeinit();
  return 0;
}


// Timeout dispatch functions
void TONull(ConnectionList<TCPState>::iterator &cptr, Time &current_time) {
  (*cptr).timeout = current_time;
  (*cptr).timeout.tv_sec += 5;
}

void TOSynSent(ConnectionList<TCPState>::iterator &cptr, Time &current_time) {
  // Decrement the number of tries and resend the SYN
  if((*cptr).state.ExpireTimerTries()) {
    ConnectionFailed(cptr);
  } else {
    SendSyn(cptr);
    (*cptr).timeout = current_time;
    (*cptr).timeout.tv_sec += 5;
  }
}

void TOSynSent1(ConnectionList<TCPState>::iterator &cptr, Time &current_time) {
  //Timeout here implies that we never received a proper SYN after
  // receiving an ACK
  if((*cptr).state.ExpireTimerTries()) {
    ConnectionFailed(cptr);
  } else {
    (*cptr).timeout = current_time;
    (*cptr).timeout.tv_sec += 5;
  }
}

void TOSynRcvd(ConnectionList<TCPState>::iterator &cptr, Time &current_time) {
  //Timeout here implies that we sent a SYN/ACK but that we
  // have not received an ACK from the remote side.  Here we
  // should resend the SYN/ACK packet.
  SendSynAck(cptr);
  (*cptr).timeout = current_time;
  (*cptr).timeout.tv_sec += 5;
}

void TOEstablished(ConnectionList<TCPState>::iterator &cptr, Time &current_time) {
  if ((*cptr).state.SendBuffer.GetSize()) {
    (*cptr).state.SetLastSent((*cptr).state.GetLastAcked());
    SendData(cptr);
  }
  (*cptr).timeout = current_time;
  (*cptr).timeout.tv_sec += 5;
}

void TOSendData(ConnectionList<TCPState>::iterator &cptr, Time &current_time) {
  if ((*cptr).state.SendBuffer.GetSize()) {
    (*cptr).state.SetLastSent((*cptr).state.GetLastAcked());
    SendData(cptr);
  }
  (*cptr).timeout = current_time;
  (*cptr).timeout.tv_sec += 5;
}

void TOCloseWait(ConnectionList<TCPState>::iterator &cptr, Time &current_time) {

#if SIM_PASSIVE_CLOSE==1
  //Simulate receiving the close from the application
  SendFin(cptr);
  (*cptr).state.SetState(LAST_ACK);

#else
  // Send a zero byte write to ask sock for the close signal
  SockRequestResponse write(WRITE,
			    (*cptr).connection,
			    (*cptr).state.RecvBuffer,
			    0,
			    EOK);
  MinetSend(sock,write);
#endif

  (*cptr).timeout = current_time;
  (*cptr).timeout.tv_sec += 5;
}

void TOClosing(ConnectionList<TCPState>::iterator &cptr, Time &current_time) {
  // Resend the ACK, since we are waiting for an ACK from the other
  //  side
  SendAck(cptr);
  (*cptr).timeout = current_time;
  (*cptr).timeout.tv_sec += 5;
}

void TOLastAck(ConnectionList<TCPState>::iterator &cptr, Time &current_time) {
  // Timeout here, means we haven't received an ACK or FIN/ACK, and 
  // must resend a FIN packet
  SendFin(cptr);
  (*cptr).timeout = current_time;
  (*cptr).timeout.tv_sec += 5;
}

void TOFinWait1(ConnectionList<TCPState>::iterator &cptr, Time &current_time) {
  // Timeout here, means we haven't received an ACK, FIN or FIN/ACK, and 
  // must resend a FIN packet
  SendFin(cptr);
  (*cptr).timeout = current_time;
  (*cptr).timeout.tv_sec += 5;
}

void TOTimeWait(ConnectionList<TCPState>::iterator &cptr, Time &current_time) {
  //Remove connection from connection list
  cerr << "Erasing connection: " << *cptr << endl << endl;
  clist.erase(cptr);
  PrintConnectionList();
}
 
// IP dataflow dispatch functions
void DFIPNull(ConnectionList<TCPState>::iterator &cptr,
	      TCPHeader &tcph,
	      Buffer &data,
	      Time &current_time) {}

void DFIPListen(TCPHeader &tcph, Connection &c, Time &current_time) {
  cerr << "LISTEN: New connection came in: " << c << endl;
  unsigned char flags=0;
  tcph.GetFlags(flags);
  if(IS_SYN(flags)) {
    //Store remote side sequence number and connection
    // information with a new connection, send SYN/ACK
    // with my sequence number
    unsigned int seqnum;
    tcph.GetSeqNum(seqnum);

    Connection cnew = c;

    Time newto = current_time;
    newto.tv_sec += 5;
    TCPState newtcp(initSeqNum, SYN_RCVD, 8);
    newtcp.SetLastRecvd(seqnum);
    ConnectionToStateMapping<TCPState> csm(cnew, newto, newtcp, true);
    clist.push_front(csm);  //Add new connection to list

    ConnectionList<TCPState>::iterator cs = clist.FindMatching(cnew);

    unsigned short window;
    tcph.GetWinSize(window);
    (*cs).state.SetSendRwnd(window);

    if(cs == clist.end()) {
      cerr << "Some problem finding the newly inserted tuple" << endl;
    }

    SendSynAck(cs);
  }
}

void DFIPSynRcvd(ConnectionList<TCPState>::iterator &cptr,
		 TCPHeader &tcph,
		 Buffer &data,
		 Time &current_time) {
  cerr << "SYN_RCVD" << endl;
  unsigned char flags=0;
  tcph.GetFlags(flags);
  if(IS_ACK(flags)) {
    //Send nothing and progress to ESTABLISHED state if valid ACK
    unsigned int ack;
    tcph.GetAckNum(ack);
    if((*cptr).state.SetLastAcked(ack)) {

      (*cptr).timeout = current_time;
      (*cptr).timeout.tv_sec += 5;

      //Change state
      (*cptr).state.SetState(ESTABLISHED);
				    
      //Indicate to sock that connection established properly
      SockRequestResponse repl;
      repl.type=WRITE;
      repl.connection=(*cptr).connection;
      repl.bytes=0;
      repl.error=EOK;
      MinetSend(sock,repl);
    }
  } else if(IS_SYN(flags)) {
    //Store remote side sequence number and connection
    // information with a new connection, send SYN/ACK
    // with my sequence number.  If we get here, we recvd
    // another syn before timing out.
    unsigned int seqnum;
    tcph.GetSeqNum(seqnum);

    (*cptr).timeout = current_time;
    (*cptr).timeout.tv_sec += 5;

    (*cptr).state.SetLastRecvd(seqnum);

    SendSynAck(cptr);

  } else if(IS_RST(flags)) {
    unsigned int seqnum;
    tcph.GetSeqNum(seqnum);
    if((*cptr).state.SetLastRecvd(seqnum,data.GetSize())) {
      cerr << "Erasing connection: " << *cptr << endl << endl;
      clist.erase(cptr);
      PrintConnectionList();
    }
  }
}

void DFIPSynSent(ConnectionList<TCPState>::iterator &cptr,
		 TCPHeader &tcph,
		 Buffer &data,
		 Time &current_time) {
  cerr << "SYN_SENT" << endl;
  unsigned char flags=0;
  tcph.GetFlags(flags);
  if(IS_SYN(flags) && IS_ACK(flags)) {
    // Check if ACK valid and store info if okay
    unsigned int ack;
    tcph.GetAckNum(ack);
    if((*cptr).state.SetLastAcked(ack)) {
      unsigned int seqnum;
      tcph.GetSeqNum(seqnum);
      (*cptr).state.SetLastRecvd(seqnum);

      SendAck(cptr);

      //Create an idle connection timeout
      (*cptr).timeout = current_time;
      (*cptr).timeout.tv_sec += 5;

      //Change state
      (*cptr).state.SetState(ESTABLISHED);

      //Indicate to sock that connection established properly
      SockRequestResponse repl;
      repl.type=WRITE;
      repl.connection=(*cptr).connection;
      repl.bytes=0;
      repl.error=EOK;
      MinetSend(sock,repl);
    }
  } else if (IS_SYN(flags)) {
    //Store remote side sequence number and connection
    // information with a new connection, send SYN/ACK
    // with my sequence number
    unsigned int seqnum;
    tcph.GetSeqNum(seqnum);

    (*cptr).state.SetLastRecvd(seqnum);

    unsigned short window;
    tcph.GetWinSize(window);
    (*cptr).state.SetSendRwnd(window);

    SendSynAck(cptr);

    (*cptr).timeout = current_time;
    (*cptr).timeout.tv_sec += 5;

    //Change state
    (*cptr).state.SetState(SYN_RCVD);

  } else if (IS_ACK(flags) && !IS_SYN(flags)) {
    // Check if ACK valid and store info if okay
    unsigned int ack;
    tcph.GetAckNum(ack);
    if((*cptr).state.SetLastAcked(ack)) {
      (*cptr).timeout = current_time;
      (*cptr).timeout.tv_sec += 5;

      //Change state to SYN_SENT1, waits for SYN
      (*cptr).state.SetState(SYN_SENT1);
    }
  }
}

void DFIPSynSent1(ConnectionList<TCPState>::iterator &cptr,
		  TCPHeader &tcph,
		  Buffer &data,
		  Time &current_time) {
  cerr << "SYN_SENT1" << endl;
  unsigned char flags=0;
  tcph.GetFlags(flags);
  if(!IS_ACK(flags) && IS_SYN(flags)) {
    //Send ACK back, and send connection established
    // back to sock

    unsigned int seqnum;
    tcph.GetSeqNum(seqnum);
    (*cptr).state.SetLastRecvd(seqnum);

    SendAck(cptr);

    //Create a new connection with a timeout to receive the SYN/ACK
    (*cptr).timeout = current_time;
    (*cptr).timeout.tv_sec += 5;

    //Indicate to sock that connection established properly
    SockRequestResponse repl;
    repl.type=WRITE;
    repl.connection=(*cptr).connection;
    repl.bytes=0;
    repl.error=EOK;
    MinetSend(sock,repl);

    (*cptr).state.SetState(ESTABLISHED);
  }
}

void DFIPEstablished(ConnectionList<TCPState>::iterator &cptr,
		     TCPHeader &tcph,
		     Buffer &data,
		     Time &current_time) {
  cerr << "ESTABLISHED" << endl;
  unsigned int seqnum;
  tcph.GetSeqNum(seqnum);
  if((*cptr).state.SetLastRecvd(seqnum,data.GetSize())) {

    unsigned char flags=0;
    tcph.GetFlags(flags);

    if(IS_ACK(flags)) {
      unsigned int ack;
      tcph.GetAckNum(ack);
      if((*cptr).state.SetLastAcked(ack)) {

	if(IS_FIN(flags) && data.GetSize()==0) {
	  (*cptr).state.SetLastRecvd(seqnum);

	  //GO TO CLOSE_WAIT STATE
	  (*cptr).state.SetState(CLOSE_WAIT);

	  (*cptr).timeout = current_time;
	  (*cptr).timeout.tv_sec += 20;

	  //Send a zero byte read to sock
	  SockRequestResponse write(WRITE,
				    (*cptr).connection,
				    (*cptr).state.RecvBuffer,
				    0,
				    EOK);
	  MinetSend(sock,write);
	  SendAck(cptr);
	} else {
	  if(data.GetSize()) {
	    //Put the payload into the receive buffer
	    (*cptr).state.RecvBuffer.AddBack(data);

	    //Send the new data up to the sock layer
	    SockRequestResponse write(WRITE,
				      (*cptr).connection,
				      (*cptr).state.RecvBuffer,
				      (*cptr).state.RecvBuffer.GetSize(),
				      EOK);
	    MinetSend(sock,write);
	    SendAck(cptr);
	  }
	}
      }
    }
  }
}


void DFIPSendData(ConnectionList<TCPState>::iterator &cptr,
		  TCPHeader &tcph,
		  Buffer &data,
		  Time &current_time) {
  cerr << "SEND_DATA" << endl;
  
  // If incoming data comes in, we must send a reset
  if (data.GetSize() > 0) {
    SendRst(cptr);
  } else {
    unsigned char flags=0;
    tcph.GetFlags(flags);

    if(IS_ACK(flags)) {
      unsigned int ack;
      tcph.GetAckNum(ack);
      if(((*cptr).state.SetLastAcked(ack))&&
	 ((*cptr).state.SendBuffer.GetSize()==0)) {

	SendFin(cptr);
	(*cptr).state.SetState(FIN_WAIT1);
      }
    }
  }
  (*cptr).timeout = current_time;
  (*cptr).timeout.tv_sec += 5;
}

void DFIPCloseWait(ConnectionList<TCPState>::iterator &cptr,
		 TCPHeader &tcph,
		 Buffer &data,
		 Time &current_time) {
  cerr << "CLOSE_WAIT" << endl;
  unsigned char flags=0;
  tcph.GetFlags(flags);

  if(IS_FIN(flags)) {
    //Change the last received up by 1 since we have
    // received a FIN.  This is like receiving 1 Byte

    //(*cptr).state.SetLastRecvd(seqnum);
    SendAck(cptr);
				    
    (*cptr).timeout = current_time;
    (*cptr).timeout.tv_sec += 5;
  }
}


void DFIPFinWait1(ConnectionList<TCPState>::iterator &cptr,
		  TCPHeader &tcph,
		  Buffer &data,
		  Time &current_time) {
  cerr << "FIN_WAIT1" << endl;

  unsigned int seqnum;
  tcph.GetSeqNum(seqnum);
  if((*cptr).state.SetLastRecvd(seqnum,data.GetSize())) {
    unsigned char flags=0;
    unsigned int ack;
    tcph.GetFlags(flags);
    tcph.GetAckNum(ack);

    if(IS_ACK(flags)&&IS_FIN(flags)&&(*cptr).state.SetLastAcked(ack-1)) {
      // ACK uses up one sequence number
      (*cptr).state.SetLastRecvd(seqnum);
      SendAck(cptr);

      //Go to TIME_WAIT state and wait 2MSL times
      (*cptr).state.SetState(TIME_WAIT);

      (*cptr).timeout = current_time;
      //(*cptr).timeout.tv_sec += 2*MSL_TIME_SECS;
      (*cptr).timeout.tv_sec += 20;
    }
    else if(IS_ACK(flags)) {
      if((*cptr).state.SetLastAcked(ack-1)) {

	//GO TO FIN_WAIT2
	(*cptr).state.SetState(FIN_WAIT2);
      }
      (*cptr).timeout = current_time;
      (*cptr).timeout.tv_sec += 5;
    }
    else if(IS_FIN(flags)) {
      // ACK uses up one sequence number
      (*cptr).state.SetLastRecvd(seqnum);
      SendAck(cptr);

      //GO TO CLOSING STATE
      (*cptr).state.SetState(CLOSING);

      (*cptr).timeout = current_time;
      (*cptr).timeout.tv_sec += 5;
    }
  }
}

void DFIPClosing(ConnectionList<TCPState>::iterator &cptr,
		 TCPHeader &tcph,
		 Buffer &data,
		 Time &current_time) {
  cerr << "CLOSING" << endl;

  unsigned char flags=0;
  tcph.GetFlags(flags);
  if(IS_ACK(flags)) {
    //Send nothing and close connection if valid ACK
    unsigned int ack;
    tcph.GetAckNum(ack);
    if((*cptr).state.SetLastAcked(ack-1)) {
      (*cptr).state.SetState(TIME_WAIT);

      //Go to TIME_WAIT state and wait 2MSL times
      (*cptr).state.SetState(TIME_WAIT);

      (*cptr).timeout = current_time;
      //(*cptr).timeout.tv_sec += 2*MSL_TIME_SECS;
      (*cptr).timeout.tv_sec += 20;
    }
  }
}

void DFIPLastAck(ConnectionList<TCPState>::iterator &cptr,
		 TCPHeader &tcph,
		 Buffer &data,
		 Time &current_time) {
  cerr << "LAST_ACK" << endl;
  unsigned char flags=0;
  tcph.GetFlags(flags);
  if(IS_ACK(flags)) {
    //Send nothing and close connection if valid ACK
    unsigned int ack;
    tcph.GetAckNum(ack);
    if((*cptr).state.SetLastAcked(ack-1)) {
      cerr << "Erasing connection: " << *cptr << endl << endl;
      clist.erase(cptr);
      PrintConnectionList();
    }
  }
}

void DFIPFinWait2(ConnectionList<TCPState>::iterator &cptr,
		  TCPHeader &tcph,
		  Buffer &data,
		  Time &current_time) {
  cerr << "FIN_WAIT2" << endl;

  unsigned int seqnum;
  tcph.GetSeqNum(seqnum);
  if((*cptr).state.SetLastRecvd(seqnum,data.GetSize())) {
    unsigned char flags=0;
    tcph.GetFlags(flags);

    if(IS_FIN(flags)) {
      // ACK uses up one sequence number
      (*cptr).state.SetLastRecvd(seqnum);
      SendAck(cptr);

      //Go to TIME_WAIT state and wait 2MSL times
      (*cptr).state.SetState(TIME_WAIT);
				    
      (*cptr).timeout = current_time;
      //(*cptr).timeout.tv_sec += 2*MSL_TIME_SECS;
      (*cptr).timeout.tv_sec += 20;
    }
  }
}

static void DFIPTimeWait(ConnectionList<TCPState>::iterator &cptr, 
			 TCPHeader &tcph,
			 Buffer &data,
			 Time &current_time) {
  cerr << "TIME_WAIT" << endl;

  unsigned int seqnum;
  tcph.GetSeqNum(seqnum);
  if((*cptr).state.SetLastRecvd(seqnum+1,0)) {
    (*cptr).state.SetLastRecvd(seqnum);
    unsigned char flags=0;
    tcph.GetFlags(flags);

    if(IS_FIN(flags)) {
      SendAck(cptr);
    }

    (*cptr).timeout = current_time;
    //(*cptr).timeout.tv_sec += 2*MSL_TIME_SECS;
    (*cptr).timeout.tv_sec += 20;
  }
}

// Sock dataflow dispatch functions
void DFSOCKConnect(SockRequestResponse &req) {
  cerr << "Got a CONNECT request." << endl;

  Time current_time;
  if (gettimeofday(&current_time, 0) < 0) {
    cerr << "Can't obtain the current time.\n";
    exit(-1);
  }

  ConnectionList<TCPState>::iterator cs = 
    clist.FindMatching(req.connection);

  if(cs == clist.end()) {
    cerr << "Creating new connection." << endl;
    //Create a new connection with a timeout to receive the SYN/ACK
    Connection cnew = req.connection;

    Time newto = current_time;
    newto.tv_sec += 5;

    TCPState newtcp(initSeqNum, SYN_SENT, 8);
    ConnectionToStateMapping<TCPState> csm(cnew, newto, newtcp, true);
    clist.push_front(csm);  //Add new connection to list
  }

  cs = clist.FindMatching(req.connection);
  if(cs == clist.end()) {
    cerr << "Some problem finding the newly inserted tuple" << endl;
  }
  SendSyn(cs);

  //Send back result code
  SockRequestResponse repl;
  repl.type=STATUS;
  repl.connection=req.connection;
  repl.bytes=0;
  repl.error=EOK;
  MinetSend(sock,repl);
}

void DFSOCKAccept(SockRequestResponse &req) {
  cerr << "Got an ACCEPT request." << endl;

  //Perform the passive open with timeout zeroed, and timer inactive
  Connection cnew = req.connection;

  cerr << req.connection << endl;

  ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
  if(cs == clist.end()) {
    Time newto(0,0);  //Set timeout to zero
    TCPState newtcp(0, LISTEN, 0);
    ConnectionToStateMapping<TCPState> csm(cnew, newto, newtcp, false);
    clist.push_back(csm);  //Add new connection to list
  }

  //Send back result code
  SockRequestResponse repl;
  repl.type=STATUS;
  repl.connection = req.connection;
  repl.bytes=0;
  repl.error=EOK;
  MinetSend(sock,repl);
}

void DFSOCKWrite(SockRequestResponse &req) {
  cerr << "Got a WRITE request." << endl;

  Time current_time;
  if (gettimeofday(&current_time, 0) < 0) {
    cerr << "Can't obtain the current time.\n";
    exit(-1);
  }

  //Check if connection in list
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);

  if(cs != clist.end()) {
    switch((*cs).state.GetState()) {
    case ESTABLISHED:
      {
	//Connection in list and in ESTABLISHED state

	//Get the size of bytes the application wants queued
	unsigned int datasize = req.data.GetSize();

	//Create result code
	SockRequestResponse repl;
	repl.type=STATUS;
	repl.connection=req.connection;
	repl.bytes = datasize;
	repl.error=EOK;
	MinetSend(sock,repl);

	if (datasize) {
	  //Add bytes to send buffer
	  (*cs).state.SendBuffer.AddBack(req.data);

	  //(*cs).state.SetLastSent((*cs).state.GetLastAcked());
	  SendData(cs);
	}
	
	//Enable the timeout
	(*cs).bTmrActive = true;

	//Update the timeout.  Should be factored using RTT, but for
	// simplicity we will use 10 seconds
	(*cs).timeout = current_time;
	(*cs).timeout.tv_sec += 5;
      }
      break;
    case LISTEN:
      {
	//Create a new connection with a timeout to receive the SYN/ACK
	Connection cnew = req.connection;

	Time newto = current_time;
	newto.tv_sec += 5;
	TCPState newtcp(initSeqNum, SYN_SENT, 8);
	ConnectionToStateMapping<TCPState> csm(cnew, newto, newtcp, true);
	clist.push_front(csm);  //Add new connection to list

	cs = clist.FindMatching(req.connection);
	if(cs == clist.end()) {
	  cerr << "Some problem finding the newly inserted tuple" << endl;
	}

	SendSyn(cs);

	//Send back result code
	SockRequestResponse repl;
	repl.type=STATUS;
	repl.connection=req.connection;
	repl.bytes=0;
	repl.error=EOK;
	MinetSend(sock,repl);
      }
      break;
    default:
      SockRequestResponse repl;
      repl.type=STATUS;
      repl.bytes=0;
      repl.error=EINVALID_OP;
      MinetSend(sock,repl);
    }
  } else {
    SockRequestResponse repl;
    repl.type=STATUS;
    repl.bytes=0;
    repl.error=ENOMATCH;
    MinetSend(sock,repl);
  }
}

void DFSOCKForward(SockRequestResponse &req) {
  cerr << "Got a FORWARD request." << endl;

  //Message ignored, but Send back result code
  SockRequestResponse repl;
  repl.type=STATUS;
  repl.error=EOK;
  MinetSend(sock,repl);
}

void DFSOCKClose(SockRequestResponse &req) {
  cerr << "Got a CLOSE request." << endl;

  //Received a CLOSE request from Sock Layer
  //Create a FIN packet
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
  
  if(cs == clist.end()) {
    SockRequestResponse repl;
    repl.type = STATUS;
    repl.error = ENOMATCH;
    MinetSend(sock,repl);

  } else if (((*cs).state.GetState() == ESTABLISHED) ||
	     ((*cs).state.GetState() == CLOSE_WAIT)) {

    if((*cs).state.SendBuffer.GetSize()) {
      // Need to make sure that all data is acked before sending the FIN
      // This is taken care of in the SEND_DATA state
      (*cs).state.SetState(SEND_DATA);
    } else {
      SendFin(cs);

      if((*cs).state.GetState() == ESTABLISHED) {
	(*cs).state.SetState(FIN_WAIT1);
      } else if((*cs).state.GetState() == CLOSE_WAIT) {
	(*cs).state.SetState(LAST_ACK);
      }
    }

  } else if ((*cs).state.GetState() == SYN_RCVD) {
    SendFin(cs);
    (*cs).state.SetState(FIN_WAIT1);
  } else if ((*cs).state.GetState() == SYN_SENT) {
    // Send connection failed to client and remove from list
    ConnectionFailed(cs);
  } else {
    SockRequestResponse repl;
    repl.type = STATUS;
    repl.error = EINVALID_OP;
    MinetSend(sock,repl);
  }

  Time timenow;
  if (gettimeofday(&timenow, 0) < 0) {
    cerr << "Can't obtain the current time.\n";
    exit(-1);
  }
  (*cs).timeout = timenow;
  (*cs).timeout.tv_sec += 5;
}

void DFSOCKStatus(SockRequestResponse &req) {
  cerr << "Got a STATUS request." << endl;

  ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);

  if(cs == clist.end()) {
    SockRequestResponse repl;
    repl.type = STATUS;
    repl.error = ENOMATCH;
    MinetSend(sock,repl);

  } else if (((*cs).state.GetState() == ESTABLISHED) ||
	     ((*cs).state.GetState() == CLOSE_WAIT) ||
	     ((*cs).state.GetState() == SEND_DATA)){
    //Erase this many bytes from the receive buffer
    (*cs).state.RecvBuffer.Erase(0, req.bytes);

    if(req.bytes < (*cs).state.RecvBuffer.GetSize()) {
      //Attempt to send the remaining bytes
      SockRequestResponse write(WRITE,
				(*cs).connection,
				(*cs).state.RecvBuffer,
				(*cs).state.RecvBuffer.GetSize(),
				EOK);

      MinetSend(sock,write);
    }
  }
}

// Worker Functions
void SendSyn(ConnectionList<TCPState>::iterator &ptr) {
  cerr << "Sending a SYN packet." << endl;

  unsigned char flags=0;
  SET_SYN(flags);
  Packet p = CreatePacket(ptr, flags);
  MinetSend(ipmux,p);
}

void SendFin(ConnectionList<TCPState>::iterator &ptr) {
  cerr << "Sending a FIN packet." << endl;

  //Create a FIN packet
  unsigned char flags=0;
  SET_FIN(flags);
  SET_ACK(flags);
  Packet p = CreatePacket(ptr, flags);
  MinetSend(ipmux,p);
}

void SendSynAck(ConnectionList<TCPState>::iterator &ptr) {
  cerr << "Sending a SYN/ACK packet." << endl;

  //Create a SYN/ACK packet
  unsigned char flags=0;
  SET_SYN(flags);
  SET_ACK(flags);
  Packet p = CreatePacket(ptr, flags);
  MinetSend(ipmux,p);
}

void SendAck(ConnectionList<TCPState>::iterator &ptr) {
  cerr << "Sending an ACK packet." << endl;

  //Create an ACK packet
  unsigned char flags=0;
  SET_ACK(flags);
  Packet p = CreatePacket(ptr, flags);
  MinetSend(ipmux,p);
}

void SendRst(ConnectionList<TCPState>::iterator &ptr) {
  cerr << "Sending a RST packet." << endl;

  //Create a RST packet
  unsigned char flags=0;
  SET_RST(flags);
  SET_ACK(flags);
  Packet p = CreatePacket(ptr, flags);
  MinetSend(ipmux,p);
}


void SendData(ConnectionList<TCPState>::iterator &ptr) {
  unsigned datasize = (*ptr).state.SendBuffer.GetSize();

  unsigned char flags=0;
  SET_PSH(flags);
  SET_ACK(flags);

  if(datasize > 0) {
    //Resend some data packets
    size_t bytesize;
    Packet p = CreatePayloadPacket(ptr, bytesize, datasize, flags);
    MinetSend(ipmux,p);

    //Create packets until bytesize becomes 0
    while(1) {
      datasize -= bytesize;
      if(datasize == 0)
	break;

      Packet p = CreatePayloadPacket(ptr, bytesize, datasize, flags);
      MinetSend(ipmux,p);
    }
  }
}

void ConnectionFailed(ConnectionList<TCPState>::iterator &ptr) {
  cerr << "Connection: " << (*ptr).connection << "failed.\n";

  SockRequestResponse repl;
  repl.type=WRITE;
  repl.connection=(*ptr).connection;
  repl.bytes=0;
  repl.error=ECONN_FAILED;
  MinetSend(sock,repl);

  //Remove connection
  cerr << "Erasing connection: " << *ptr << endl << endl;
  clist.erase(ptr);
  PrintConnectionList();
}

// General functions
unsigned UpdateSequenceNumber(Time &current_time, Time &seqtime, unsigned num) {
  // Update the sequence number from current time
  num += ((current_time.tv_usec - seqtime.tv_usec)
	  * MICROSEC_MULTIPLIER) & SEQ_LENGTH_MASK;
  num += ((current_time.tv_sec - seqtime.tv_sec)
	  * SECOND_MULTIPLIER) & SEQ_LENGTH_MASK;
  seqtime = current_time;
  return num;
}

bool GetNextTimeoutValue(Time &current_time, Time &select_timeout) {
  Time zero_time(0,0);
  bool tmractive=false;

  PrintConnectionList();

  // Find earliest expiring timer in the connection list
  ConnectionList<TCPState>::iterator earliest = clist.FindEarliest();
  if (earliest != clist.end()) {
    if ((*earliest).timeout < current_time) {
      select_timeout = zero_time; // Timeout immediately
      tmractive=true;
    } else {
      // There is an active timer in connection list
      double t = (double)(*earliest).timeout - (double)current_time;
      select_timeout = t;
      tmractive = true;
    }
  }
  return tmractive;
}

void PrintConnectionList() {
  cerr << "The connection list: " << endl;
  ConnectionList<TCPState>::iterator iter = clist.begin();
  while (iter != clist.end()) {
    cerr << *iter << endl << endl;
    iter++;
  }
}

// Packet creation functions
Packet CreatePacket(ConnectionList<TCPState>::iterator &ptr, unsigned char flags)
{
  //Create a SYN packet
  Packet p;

  //Build IP header info
  IPHeader ih;
  ih.SetProtocol(IP_PROTO_TCP);
  ih.SetSourceIP((*ptr).connection.src);
  ih.SetDestIP((*ptr).connection.dest);

  if(IS_SYN(flags))
    ih.SetTotalLength(TCP_HEADER_BASE_LENGTH + 4 + IP_HEADER_BASE_LENGTH);
  else
    ih.SetTotalLength(TCP_HEADER_BASE_LENGTH + IP_HEADER_BASE_LENGTH);

  p.PushFrontHeader(ih);

  //Build TCP header info
  TCPHeader th;
  th.SetSourcePort((*ptr).connection.srcport,p);
  th.SetDestPort((*ptr).connection.destport,p);

  if(IS_SYN(flags)) {
    th.SetSeqNum((*ptr).state.GetLastAcked(),p);

    //Set the MSS length for the connection
    TCPOptions opts;
    opts.len = TCP_HEADER_OPTION_KIND_MSS_LEN;
    opts.data[0] = (char) TCP_HEADER_OPTION_KIND_MSS;
    opts.data[1] = (char) TCP_HEADER_OPTION_KIND_MSS_LEN;
    opts.data[2] = (char) ((TCP_MAXIMUM_SEGMENT_SIZE & 0xFF00) >> 8);
    opts.data[3] = (char) (TCP_MAXIMUM_SEGMENT_SIZE & 0x00FF);
    th.SetOptions(opts);
  } else {
    th.SetSeqNum((*ptr).state.GetLastSent()+1,p);
  }

  th.SetFlags(flags,p);
  if(IS_ACK(flags))
    th.SetAckNum((*ptr).state.GetLastRecvd()+1,p);

  //Set the window size
  th.SetWinSize((*ptr).state.GetRwnd(),p);

  if(IS_SYN(flags)) {
    th.SetHeaderLen((TCP_HEADER_BASE_LENGTH+4)/4,p);
  } else {
    th.SetHeaderLen((TCP_HEADER_BASE_LENGTH/4),p);
  }

  p.PushBackHeader(th);

  return p;
}

Packet CreatePayloadPacket(ConnectionList<TCPState>::iterator &ptr,
			   unsigned &bytesize,
			   unsigned int datasize,
			   unsigned char flags)
{
  //Find number of data bytes for payload
  unsigned dataoffset;
  (*ptr).state.SendPacketPayload(dataoffset, bytesize, datasize);

  char tempBuffer[TCP_MAXIMUM_SEGMENT_SIZE];
  (*ptr).state.SendBuffer.GetData(tempBuffer, bytesize, dataoffset);
  Buffer quickBuffer(tempBuffer, bytesize);

  //Create a data packet
  Packet p(quickBuffer);

  //Build IP header info
  IPHeader ih;
  ih.SetProtocol(IP_PROTO_TCP);
  ih.SetSourceIP((*ptr).connection.src);
  ih.SetDestIP((*ptr).connection.dest);

  ih.SetTotalLength(TCP_HEADER_BASE_LENGTH + IP_HEADER_BASE_LENGTH + bytesize);
  p.PushFrontHeader(ih);

  //Build TCP header info
  TCPHeader th;
  th.SetSourcePort((*ptr).connection.srcport,p);
  th.SetDestPort((*ptr).connection.destport,p);
  th.SetSeqNum((*ptr).state.GetLastSent() + 1,p);

  //Set last sent to next byte sent only if there was data generated
  (*ptr).state.SetLastSent((*ptr).state.GetLastSent() + bytesize);

  th.SetFlags(flags,p);    

  th.SetAckNum((*ptr).state.GetLastRecvd() + 1,p);

  //Set the window size
  th.SetWinSize((*ptr).state.GetRwnd(),p);
  th.SetHeaderLen(TCP_HEADER_BASE_LENGTH/4,p);
  p.PushBackHeader(th);

  return p;
}

// Extraction functions
bool ExtractInfoFromIPPacket(TCPHeader &tcph, Connection &c, Buffer &data) {
  bool goodpacket=false;
  Packet p;
  MinetReceive(ipmux,p);
  unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
  p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
  IPHeader iph=p.FindHeader(Headers::IPHeader);
  tcph=p.FindHeader(Headers::TCPHeader);

  unsigned short totlen;
  unsigned char iphlen;
  iph.GetTotalLength(totlen);
  iph.GetHeaderLength(iphlen);
  unsigned datalen = (unsigned) totlen - (unsigned) (iphlen*sizeof(int)) -
    tcphlen;

  if (datalen) {
    data=p.GetPayload().ExtractFront(datalen);
  }
		
  //Check if checksum correct
  if(tcph.IsCorrectChecksum(p)) {
    //Grab the connection information
    iph.GetDestIP(c.src);
    iph.GetSourceIP(c.dest);
    iph.GetProtocol(c.protocol);
    tcph.GetDestPort(c.srcport);
    tcph.GetSourcePort(c.destport);
    goodpacket=true;
  }
  
  return goodpacket;
}
