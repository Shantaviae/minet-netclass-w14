/*******************************************************************************
 * Project part B
 *
 * Authors: Jason Skicewicz
 *          Kohinoor Basu
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

static Packet CreatePacket(ConnectionList<TCPState>::iterator &ptr, 
			   unsigned char flags);

static Packet CreatePayloadPacket(ConnectionList<TCPState>::iterator &ptr,
				  unsigned &bytesize,
				  unsigned int datasize);

MinetHandle ipmux;
MinetHandle sock;

int main(int argc, char *argv[])
{
  // Create the connection list
  ConnectionList<TCPState> clist;

  MinetInit(MINET_TCP_MODULE);

  ipmux = MinetIsModuleInConfig(MINET_IP_MUX) ? MinetConnect(MINET_IP_MUX) : MINET_NOHANDLE;
  sock = MinetIsModuleInConfig(MINET_SOCK_MODULE) ? MinetAccept(MINET_SOCK_MODULE) : MINET_NOHANDLE;

  if (ipmux==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_IP_MUX)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't open connection to IP mux"));
    return -1;
  }
      
  if (sock==MINET_NOHANDLE && MinetIsModuleInConfig(MINET_SOCK_MODULE)) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't open connection to sock module"));
    return -1;
  }
  
  cerr << "tcp_module operational\n";
  MinetSendToMonitor(MinetMonitoringEvent("tcp_module operational"));

  MinetEvent event;

  int rc;

  cerr << "tcp_module handling TCP traffic\n";
    
  // Initialize sequence time to current time, and initial sequence number to 0
  Time seqtime, two_msl;
  unsigned initSeqNum=0;
    
  if (gettimeofday(&seqtime, 0) < 0) {
    cerr << "Can't obtain the current time.\n";
    return -1;
  }

  cerr << "The current time is: " << seqtime << endl;

  initSeqNum += (seqtime.tv_usec * MICROSEC_MULTIPLIER) & SEQ_LENGTH_MASK;
  initSeqNum += (seqtime.tv_sec * SECOND_MULTIPLIER) & SEQ_LENGTH_MASK;

  cerr << "The current sequence number is: " << initSeqNum << endl;

  // Wait 2MSLs (4minutes) before starting
  two_msl = seqtime;
  two_msl.tv_sec += 5;
  //    two_msl.tv_sec += 2*MSL_TIME_SECS;

  while(two_msl > seqtime) {
    if (gettimeofday(&seqtime, 0) < 0) {
      cerr << "Can't obtain the current time.\n";
      return -1;
    }
  }

  cerr << "Starting main processing loop.\n\n";

  bool infinity = true;

  // Start of main processing loop
  while (1) {
    Time current_time,select_timeout, zero_time(0,0);

    if (gettimeofday(&current_time, 0) < 0) {
      cerr << "Can't obtain the current time.\n";
      return -1;
    }

    // Update the sequence number from current time
    initSeqNum += ((current_time.tv_usec - seqtime.tv_usec)
		   * MICROSEC_MULTIPLIER) & SEQ_LENGTH_MASK;
    initSeqNum += ((current_time.tv_sec - seqtime.tv_sec)
		   * SECOND_MULTIPLIER) & SEQ_LENGTH_MASK;
    seqtime = current_time;

    cerr << "Current sequence number is: " << initSeqNum << endl;
	
    // Find earliest expiring timer in the connection list
    ConnectionList<TCPState>::iterator earliest = clist.FindEarliest();
    if (earliest == clist.end()) {
      // No active timers in connection list
      infinity = true;
    } else if ((*earliest).timeout < current_time) {
      select_timeout = zero_time; // Timeout immediately
      infinity = false;
    } else {
      // There is an active timer in connection list
      double t = (double)(*earliest).timeout - (double) current_time;
      select_timeout = t;
      infinity = false;
    }

    if(infinity == true) {
      rc=MinetGetNextEvent(event);
    } else {
      rc=MinetGetNextEvent(event,(double)select_timeout);
    }

    if (rc<0) {
      break;
    } else if (event.eventtype==MinetEvent::Timeout) { 
      cerr << "Timeout ocurred." << endl;

      // Handle all connections that have incurred a timeout
      ConnectionList<TCPState>::iterator next = clist.FindEarliest();
      while((*next).timeout < current_time && next != clist.end()) {
	// Check the state of each timed out connection
	switch((*next).state.GetState()) {
	case LISTEN:
	  {
	  }
	  break;
	case SYN_SENT:
	  {
	    // Decrement the number of tries and resend the SYN
	    if((*next).state.ExpireTimerTries()) {
	      //Send failing result code to sock, and remove connection
	      // from list
	      SockRequestResponse repl;
	      repl.type=WRITE;
	      repl.connection=(*next).connection;
	      repl.bytes=0;
	      repl.error=ECONN_FAILED;
	      MinetSend(sock,repl);

	      cerr << "Connection: " << (*next).connection << "failed.\n";

	      //Remove connection
	      clist.erase(next);
	    } else {
	      //Create a SYN packet and send to IP
	      unsigned char flags=0;
	      SET_SYN(flags);
	      Packet p = CreatePacket(next, flags);
	      MinetSend(ipmux,p);

	      (*next).timeout.tv_sec += 10; //Timeout set to 10 seconds before retry
	    }
	  }
	  break;
	case SYN_SENT1:
	  {
	    //Timeout here implies that we never received a proper SYN after
	    // receiving an ACK
	    SockRequestResponse repl;
	    repl.type=WRITE;
	    repl.connection=(*next).connection;
	    repl.bytes=0;
	    repl.error=ECONN_FAILED;
	    MinetSend(sock,repl);

	    cerr << "Connection: " << (*next).connection << "failed.\n";

	    //Remove connection
	    clist.erase(next);
	  }
	  break;
	case SYN_RCVD:
	  {
	    //Timeout here implies that we sent a SYN, ACK but that we
	    // have not received a SYN from the remote side.  Here we
	    // should resend the SYN, ACK packet.
	    (*next).timeout.tv_sec += 10; //Timeout set to 10 seconds

	    cerr << "Sending a SYN/ACK packet." << endl;
	    //Create a SYN/ACK packet
	    unsigned char flags=0;
	    SET_SYN(flags);
	    SET_ACK(flags);
	    Packet p = CreatePacket(next, flags);
	    MinetSend(ipmux,p);
	  }
	  break;
	case ESTABLISHED:
	  {
	    //Set last_sent to last_acked + 1 and resend a bunch of data
	    // packets
	    (*next).state.SetLastSent((*next).state.GetLastAcked());

	    unsigned datasize = (*next).state.SendBuffer.GetSize();

	    if(datasize > 0) {
	      //Resend some data packets
	      size_t bytesize;
	      Packet p = CreatePayloadPacket(next, bytesize, datasize);
	      MinetSend(ipmux,p);

	      //Create packets until bytesize becomes 0
	      while(1) {
		datasize -= bytesize;
		if(datasize <= 0)
		  break;

		Packet p = CreatePayloadPacket(next, bytesize, datasize);
		MinetSend(ipmux,p);
	      }
	      (*next).timeout.tv_sec += 10; //Timeout set to 10 seconds for now
	    }
	  }
	  break;
	case CLOSE_WAIT:
	  {
	  }
	  break;
	case LAST_ACK:
	  {
	  }
	  break;
	case FIN_WAIT1:
	  {
	  }
	  break;
	case FIN_WAIT2:
	  {
	  }
	  break;
	case TIME_WAIT:
	  {
	    //Remove connection from connection list
	    clist.erase(next);
	  }
	  break;
	default:
	  {
	  }
	  break;
	}
	next = clist.FindEarliest();
      }
    } else if (event.eventtype==MinetEvent::Dataflow && event.direction==MinetEvent::IN) {

      if (event.handle==ipmux) { 
	Packet p;
	MinetReceive(ipmux,p);
	unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
	p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
	IPHeader iph=p.FindHeader(Headers::IPHeader);
	TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

	unsigned short totlen;
	unsigned char iphlen;
	unsigned datalen;
	iph.GetTotalLength(totlen);
	iph.GetHeaderLength(iphlen);
	datalen = (unsigned) totlen - (unsigned) (iphlen*sizeof(int)) -
	  tcphlen;
		
	//Check if checksum correct
	if(tcph.IsCorrectChecksum(p)) {
	  //Grab the connection information
	  Connection c;
	  iph.GetDestIP(c.src);
	  iph.GetSourceIP(c.dest);
	  iph.GetProtocol(c.protocol);
	  tcph.GetDestPort(c.srcport);
	  tcph.GetSourcePort(c.destport);
	  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
	  if(cs != clist.end()) {
	    //First set the window size from remote side
	    unsigned short window;
	    tcph.GetWinSize(window);
	    (*cs).state.SetSendRwnd(window);
	    switch((*cs).state.GetState()) {
	    case LISTEN:
	      {
		unsigned char flags=0;
		tcph.GetFlags(flags);
		if(IS_SYN(flags)) {
		  //Store remote side sequence number and connection
		  // information with a new connection, send SYN/ACK
		  // with my sequence number
		  unsigned int seqnum;
		  tcph.GetSeqNum(seqnum);

		  Connection cnew = c;
		  if (gettimeofday(&current_time, 0) < 0) {
		    cerr << "Can't obtain the current time.\n";
		    return -1;
		  }
		  Time newto = current_time;
		  newto.tv_sec += 5; //Timeout set to 5 seconds before retry
		  TCPState newtcp(initSeqNum, SYN_RCVD, 8);
		  newtcp.SetLastRecvd(seqnum);
		  ConnectionToStateMapping<TCPState> csm(cnew, newto, newtcp, true);
		  clist.push_front(csm);  //Add new connection to list

		  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);

		  if(cs == clist.end()) {
		    cerr << "Some problem finding the newly inserted tuple" << endl;
		  }

		  cerr << "Sending a SYN/ACK packet." << endl;
		  //Create a SYN/ACK packet
		  unsigned char flags=0;
		  SET_SYN(flags);
		  SET_ACK(flags);
		  Packet p = CreatePacket(cs, flags);
		  MinetSend(ipmux,p);
		}
	      }
	      break;
	    case SYN_SENT:
	      {
		unsigned char flags=0;
		tcph.GetFlags(flags);
		if(IS_SYN(flags) && IS_ACK(flags)) {
		  // Check if ACK valid and store info if okay
		  unsigned int ack;
		  tcph.GetAckNum(ack);
		  if((*cs).state.SetLastAcked(ack)) {
		    unsigned int seqnum;
		    tcph.GetSeqNum(seqnum);
		    (*cs).state.SetLastRecvd(seqnum);

		    //Send ACK back
		    unsigned char flags=0;
		    SET_ACK(flags);
		    Packet p = CreatePacket(cs, flags);
		    MinetSend(ipmux,p);
				    
		    //Eventually want to implement some sort of idle timer, and
		    // plus we need a timer equivalent to RTT + 2 seconds for 
		    // receiving ACKs

		    //Create an idle connection timeout
		    if (gettimeofday(&current_time, 0) < 0) {
		      cerr << "Can't obtain the current time.\n";
		      return -1;
		    }
		    (*cs).timeout = current_time;
		    (*cs).timeout.tv_sec += 1000; //Timeout set to 1000 seconds

		    //Change state
		    (*cs).state.SetState(ESTABLISHED);

		    //Indicate to sock that connection established properly
		    SockRequestResponse repl;
		    repl.type=WRITE;
		    repl.connection=(*cs).connection;
		    repl.bytes=0;
		    repl.error=EOK;
		    MinetSend(sock,repl);
		  }
		} else if(IS_ACK(flags) && !IS_SYN(flags)) {
		  // Check if ACK valid and store info if okay
		  unsigned int ack;
		  tcph.GetAckNum(ack);
		  if((*cs).state.SetLastAcked(ack)) {
		    if(gettimeofday(&current_time, 0) < 0) {
		      cerr << "Can't obtain the current time.\n";
		      return -1;
		    }
		    (*cs).timeout = current_time;
		    (*cs).timeout.tv_sec += 80; //Timeout set to 80 seconds

		    //Change state to SYN_SENT1, waits for SYN
		    (*cs).state.SetState(SYN_SENT1);
		  }
		}
	      }
	      break;
	    case SYN_SENT1:
	      {
		unsigned char flags=0;
		tcph.GetFlags(flags);
		if(!IS_ACK(flags) && IS_SYN(flags)) {
		  //Send ACK back, and send connection established
		  // back to sock

		  unsigned int seqnum;
		  tcph.GetSeqNum(seqnum);
		  (*cs).state.SetLastRecvd(seqnum);

		  //Send ACK back
		  flags=0;
		  SET_ACK(flags);
		  Packet p = CreatePacket(cs, flags);
		  MinetSend(ipmux,p);
				    
		  //Eventually want to implement some sort of idle timer, and
		  // plus we need a timer equivalent to RTT + 2 seconds for 
		  // receiving ACKs

		  //Create a new connection with a timeout to receive the SYN/ACK
		  if (gettimeofday(&current_time, 0) < 0) {
		    cerr << "Can't obtain the current time.\n";
		    return -1;
		  }
		  (*cs).timeout = current_time;
		  (*cs).timeout.tv_sec += 1000; //Timeout set to 1000 seconds

		  //Change state
		  (*cs).state.SetState(ESTABLISHED);

		  //Indicate to sock that connection established properly
		  SockRequestResponse repl;
		  repl.type=WRITE;
		  repl.connection=(*cs).connection;
		  repl.bytes=0;
		  repl.error=EOK;
		  MinetSend(sock,repl);

		  (*cs).state.SetState(ESTABLISHED);
		}

	      }
	      break;
	    case SYN_RCVD:
	      {
		unsigned char flags=0;
		tcph.GetFlags(flags);
		if(IS_ACK(flags)) {
		  //Send nothing and progress to ESTABLISHED state if valid ACK
		  unsigned int ack;
		  tcph.GetAckNum(ack);
		  if((*cs).state.SetLastAcked(ack)) {

		    //Eventually want to implement some sort of idle timer, and
		    // plus we need a timer equivalent to RTT + 2 seconds for 
		    // receiving ACKs

		    //Setup a long timeout
		    if (gettimeofday(&current_time, 0) < 0) {
		      cerr << "Can't obtain the current time.\n";
		      return -1;
		    }
		    (*cs).timeout = current_time;
		    (*cs).timeout.tv_sec += 1000; //Timeout set to 1000 seconds

		    //Change state
		    (*cs).state.SetState(ESTABLISHED);
				    
		    cerr << "New connection received: " << endl << (*cs).connection;

		    //Indicate to sock that connection established properly
		    SockRequestResponse repl;
		    repl.type=WRITE;
		    repl.connection=(*cs).connection;
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

		  if (gettimeofday(&current_time, 0) < 0) {
		    cerr << "Can't obtain the current time.\n";
		    return -1;
		  }
		  (*cs).timeout = current_time;
		  (*cs).timeout.tv_sec += 5; //Timeout set to 5 seconds

		  (*cs).state.SetLastRecvd(seqnum);
				    
		  cerr << "Sending a SYN/ACK packet." << endl;
		  //Create a SYN/ACK packet
		  unsigned char flags=0;
		  SET_SYN(flags);
		  SET_ACK(flags);
		  Packet p = CreatePacket(cs, flags);
		  MinetSend(ipmux,p);

		} else if(IS_RST(flags)) {
		  unsigned int seqnum;
		  tcph.GetSeqNum(seqnum);
		  if((*cs).state.SetLastRecvd(seqnum,datalen)) {
		    clist.erase(cs);
		  }
		}
	      }
	      break;
	    case ESTABLISHED:
	      {
		unsigned int seqnum;
		tcph.GetSeqNum(seqnum);
		if((*cs).state.SetLastRecvd(seqnum,datalen)) {

		  unsigned char flags=0;
		  tcph.GetFlags(flags);

		  if(IS_FIN(flags)) {
		    //Change the last received up by 1 since we have
		    // received a FIN.  This is like receiving 1 Byte
		    (*cs).state.SetLastRecvd(seqnum);

		    //Create the ACK packet
		    flags=0;
		    SET_ACK(flags);
		    Packet p = CreatePacket(cs, flags);
				    
		    //Send a ACK TO IP LAYER
		    MinetSend(ipmux,p);
				    
		    //GO TO CLOSE_WAIT STATE
		    (*cs).state.SetState(CLOSE_WAIT);
		  } else if(IS_PSH(flags)&&IS_ACK(flags)) {

		    unsigned int ack;
		    tcph.GetAckNum(ack);
		    if((*cs).state.SetLastAcked(ack)) {
		      //Get the payload from the packet
		      Buffer &data = p.GetPayload().ExtractFront(datalen);

		      //Put the payload into the receive buffer
		      (*cs).state.RecvBuffer.AddBack(data);

		      //Send the ACK back
		      unsigned char flags=0;
		      SET_ACK(flags);
		      Packet p = CreatePacket(cs, flags);
				    
		      //Send a ACK TO IP LAYER
		      MinetSend(ipmux,p);

		      //Send the new data up to the sock layer
		      SockRequestResponse write(WRITE,
						(*cs).connection,
						(*cs).state.RecvBuffer,
						(*cs).state.RecvBuffer.GetSize(),
						EOK);
		      cerr << "Sending data to sock 1." << endl;
		      MinetSend(sock,write);
		    }
		  } else if(IS_ACK(flags)) {
		    //handle the normal one way data packets
		    unsigned int ack;
		    tcph.GetAckNum(ack);
		    if((*cs).state.SetLastAcked(ack)) {
		      //Good ack received, advance timeout if
		      // more packets in channel
		      if((*cs).state.GetLastAcked() ==
			 (*cs).state.GetLastSent()) {
			//Disable the timeout
			(*cs).bTmrActive = false;
		      } else {
			if (gettimeofday(&current_time, 0) < 0) {
			  cerr << "Can't obtain the current time.\n";
			  return -1;
			}
			(*cs).timeout = current_time;
			(*cs).timeout.tv_sec += 10;
			(*cs).bTmrActive = true;
		      }
		    }
		  }
		}
	      } //end case ESTABLISHED
	      break;	
	    case CLOSE_WAIT:
	      break;
	    case LAST_ACK:
	      {
		unsigned char flags=0;
		tcph.GetFlags(flags);
		if(IS_ACK(flags)) {
		  //Send nothing and close connection if valid ACK
		  unsigned int ack;
		  tcph.GetAckNum(ack);
		  if((*cs).state.SetLastAcked(ack))
		    clist.erase(cs);
		}			    
	      } //end case LAST_ACK
	      break;
	    case FIN_WAIT1:
	      {
		unsigned int seqnum;
		tcph.GetSeqNum(seqnum);
		if((*cs).state.SetLastRecvd(seqnum,datalen)) {

		  unsigned char flags=0;
		  tcph.GetFlags(flags);
		  if(IS_ACK(flags)&&IS_FIN(flags))
		    {
		      //Send ACK back
		      //Create a ACK packet
		      flags=0;
		      SET_ACK(flags);
		      Packet p = CreatePacket(cs, flags);

		      //Send a ACK TO IP LAYER
		      MinetSend(ipmux,p);
				   
		      //Go to TIME_WAIT state and wait 2MSL times
		      (*cs).state.SetState(TIME_WAIT);
				    
		      if (gettimeofday(&current_time, 0) < 0) {
			cerr << "Can't obtain the current time.\n";
			return -1;
		      }
				    
		      (*cs).timeout = current_time;
		      //				    (*cs).timeout.tv_sec += 2*MSL_TIME_SECS;
		      (*cs).timeout.tv_sec += 30;
		    }
		  else if(IS_ACK(flags))
		    {
		      //GO TO FIN_WAIT2
		      (*cs).state.SetState(FIN_WAIT2);
		    }
		  else if(IS_FIN(flags))
		    {
		      //Send ACK back
		      //Create a ACK packet
		      flags=0;
		      SET_ACK(flags);
		      Packet p = CreatePacket(cs, flags);

		      //Send a ACK TO IP LAYER
		      MinetSend(ipmux,p);
       
		      //GO TO CLOSING STATE
		      (*cs).state.SetState(CLOSING);
		    }
		}
	      } //End FIN_WAIT1
	      break;
	    case FIN_WAIT2:
	      {
		unsigned int seqnum;
		tcph.GetSeqNum(seqnum);
		if((*cs).state.SetLastRecvd(seqnum,datalen)) {
		  unsigned char flags=0;
		  tcph.GetFlags(flags);

		  if(IS_FIN(flags))
		    {
		      //Send ACK back
		      //Create a ACK packet
		      flags=0;
		      SET_ACK(flags);
		      Packet p = CreatePacket(cs, flags);

		      //Send a ACK TO IP LAYER
		      MinetSend(ipmux,p);

		      //Go to TIME_WAIT state and wait 2MSL times
		      (*cs).state.SetState(TIME_WAIT);
				    
		      if (gettimeofday(&current_time, 0) < 0) {
			cerr << "Can't obtain the current time.\n";
			return -1;
		      }
				    
		      (*cs).timeout = current_time;
		      //				    (*cs).timeout.tv_sec += 2*MSL_TIME_SECS;
		      (*cs).timeout.tv_sec += 30;
		    }
		}
	      } //End of FIN_WAIT2
	      break;
	    case TIME_WAIT:
	      {
	      }
	      break;
	    case CLOSING:
	      {
		unsigned char flags=0;
		tcph.GetFlags(flags);

		if(IS_ACK(flags))
		  {
				   
		    //Go to TIME_WAIT state and wait 2MSL times
		    (*cs).state.SetState(TIME_WAIT);
				    
		    if (gettimeofday(&current_time, 0) < 0) {
		      cerr << "Can't obtain the current time.\n";
		      return -1;
		    }
				    
		    (*cs).timeout = current_time;
		    //				(*cs).timeout.tv_sec += 2*MSL_TIME_SECS;
		    (*cs).timeout.tv_sec += 30;
		  }

	      } //End CLOSING
	      break;
	    default:
	      {
	      }
	      break;
	    }
	  }
	}
		    
      }


      if (event.handle==sock) {
	SockRequestResponse req;
	MinetReceive(sock,req);
	cerr << "SOCK REQUEST: " << req << endl;
	switch (req.type) {
	case CONNECT:
	  {
	    ConnectionList<TCPState>::iterator cs = 
	      clist.FindMatching(req.connection);
	    if(cs == clist.end()) {
	      cerr << "Creating new connection." << endl;
	      //Create a new connection with a timeout to receive the SYN/ACK
	      Connection cnew = req.connection;
	      if (gettimeofday(&current_time, 0) < 0) {
		cerr << "Can't obtain the current time.\n";
		return -1;
	      }
	      Time newto = current_time;
	      newto.tv_sec += 10; //Timeout set to 10 seconds before retry
	      TCPState newtcp(initSeqNum, SYN_SENT, 8);
	      ConnectionToStateMapping<TCPState> csm(cnew, newto, newtcp, true);
	      clist.push_front(csm);  //Add new connection to list
	    }
	    cs = clist.FindMatching(req.connection);

	    //Create a SYN packet
	    cerr << "SENDING SYN Packet." << endl;
	    unsigned char flags=0;
	    SET_SYN(flags);
	    Packet p = CreatePacket(cs, flags);
	    MinetSend(ipmux,p);

	    //Send back result code
	    SockRequestResponse repl;
	    repl.type=STATUS;
	    repl.connection=req.connection;
	    repl.bytes=0;
	    repl.error=EOK;
	    MinetSend(sock,repl);
	  }
	  break;
	case ACCEPT:
	  {
	    cerr << "Got an ACCEPT." << endl;
	    //Perform the passive open with timeout zeroed, and timer inactive
	    Connection cnew = req.connection;
	    Time newto(0,0);  //Set timeout to zero
	    TCPState newtcp(0, LISTEN, 0);
	    ConnectionToStateMapping<TCPState> csm(cnew, newto, newtcp, false);
	    clist.push_back(csm);  //Add new connection to list

	    //Send back result code
	    SockRequestResponse repl;
	    repl.type=STATUS;
	    repl.connection = req.connection;
	    repl.bytes=0;
	    repl.error=EOK;
	    MinetSend(sock,repl);
	  }
	  break;
	case WRITE: 
	  {
	    //Check if connection in list
	    ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
	    if(cs != clist.end()) {
	      switch((*cs).state.GetState()) {
	      case ESTABLISHED:
		{
		  //Connection in list and in ESTABLISHED state

		  //Get the size of bytes the application wants queued
		  unsigned int datasize = req.data.GetSize();

		  //Add bytes to send buffer
		  (*cs).state.SendBuffer.AddBack(req.data);
			    
		  //Create result code
		  SockRequestResponse repl;
		  repl.type=STATUS;
		  repl.connection=req.connection;
		  repl.bytes = datasize;
		  repl.error=EOK;
		  MinetSend(sock,repl);

		  if(datasize > 0) {

		    //Send some data packets
		    size_t bytesize;
		    Packet p = CreatePayloadPacket(cs, bytesize, datasize);
		    MinetSend(ipmux,p);

		    //Create packets until bytesize becomes 0
		    while(1) {
		      datasize -= bytesize;

		      //If no more data break out of while loop
		      if(datasize <= 0)
			break;

		      Packet p = CreatePayloadPacket(cs, bytesize, datasize);
		      MinetSend(ipmux,p);
		    }

		    //Update the timeout.  Should be factored using RTT, but for
		    // simplicity we will use 10 seconds
		    if (gettimeofday(&current_time, 0) < 0) {
		      cerr << "Can't obtain the current time.\n";
		      return -1;
		    }
		    (*cs).timeout = current_time;
		    (*cs).timeout.tv_sec += 10; //Timeout set to 10 seconds for now

		    //Enable the timeout
		    (*cs).bTmrActive = true;
		  }
		}
		break;
	      case LISTEN:
		{
		  //Create a new connection with a timeout to receive the SYN/ACK
		  Connection cnew = req.connection;
		  if (gettimeofday(&current_time, 0) < 0) {
		    cerr << "Can't obtain the current time.\n";
		    return -1;
		  }
		  Time newto = current_time;
		  newto.tv_sec += 10; //Timeout set to 10 seconds before retry
		  TCPState newtcp(initSeqNum, SYN_SENT, 8);
		  ConnectionToStateMapping<TCPState> csm(cnew, newto, newtcp, true);
		  clist.push_front(csm);  //Add new connection to list

		  cs = clist.FindMatching(req.connection);

		  //Create a SYN packet
		  unsigned char flags=0;
		  SET_SYN(flags);
		  Packet p = CreatePacket(cs, flags);
		  MinetSend(ipmux,p);

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
	  break;
	case FORWARD:
	  {
	    //Message ignored, but Send back result code
	    SockRequestResponse repl;
	    repl.type=STATUS;
	    repl.error=EOK;
	    MinetSend(sock,repl);
	  }
	  break;
	case CLOSE:
	  {
	    //Received a CLOSE request from Sock Layer
	    //Create a FIN packet
	    ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
	    if(cs == clist.end()) {
	      SockRequestResponse repl;
	      repl.type = STATUS;
	      repl.error = ENOMATCH;
	      MinetSend(sock,repl);

	    } else if (((*cs).state.GetState() == SYN_RCVD) ||
		       ((*cs).state.GetState() == ESTABLISHED) ||
		       ((*cs).state.GetState() == CLOSE_WAIT)) {

	      //Create a FIN packet
	      unsigned char flags=0;
	      SET_ACK(flags);
	      SET_FIN(flags);
	      Packet p = CreatePacket(cs, flags);
	      MinetSend(ipmux,p);

	      if(((*cs).state.GetState() == ESTABLISHED) ||
		 ((*cs).state.GetState() == SYN_RCVD))
		(*cs).state.SetState(FIN_WAIT1);
	      else if((*cs).state.GetState() == CLOSE_WAIT)
		(*cs).state.SetState(LAST_ACK);

	    } else if ((*cs).state.GetState() == SYN_SENT) {
	      // Send connection failed to client and remove from list

	      SockRequestResponse repl;
	      repl.type=WRITE;
	      repl.connection=(*cs).connection;
	      repl.bytes=0;
	      repl.error=ECONN_FAILED;
	      MinetSend(sock,repl);

	      cerr << "Connection: " << (*cs).connection << "failed.\n";

	      //Remove connection
	      clist.erase(cs);
	    } else {
	      SockRequestResponse repl;
	      repl.type = STATUS;
	      repl.error = EINVALID_OP;
	      MinetSend(sock,repl);
	    }
	  }
	  break;
	case STATUS: 
	  {
	    ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
	    if(cs == clist.end()) {
	      SockRequestResponse repl;
	      repl.type = STATUS;
	      repl.error = ENOMATCH;
	      MinetSend(sock,repl);

	    } else if ((*cs).state.GetState() == ESTABLISHED){
	      //Erase this many bytes from the receive buffer
	      (*cs).state.RecvBuffer.Erase(0, req.bytes);

	      if(req.bytes < (*cs).state.RecvBuffer.GetSize()) {
		//Attempt to send the remaining bytes
		SockRequestResponse write(WRITE,
					  (*cs).connection,
					  (*cs).state.RecvBuffer,
					  (*cs).state.RecvBuffer.GetSize(),
					  EOK);
		//			    cerr << "Sending data to sock 2." << endl;
		MinetSend(sock,write);
	      }
	    }
	  }
	  break;
	default:
	  SockRequestResponse repl;
	  repl.type=STATUS;
	  repl.error=EWHAT;
	  MinetSend(sock,repl);
	}
      }

    }
  }
  MinetDeinit();
  return 0;
}

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
    th.SetSeqNum((*ptr).state.GetLastSent() + 1,p);
  }

  th.SetFlags(flags,p);
  if(IS_ACK(flags))
    th.SetAckNum((*ptr).state.GetLastRecvd() + 1,p);

  //Set the window size
  th.SetWinSize((*ptr).state.GetRwnd(),p);

  if(IS_SYN(flags)) {
    th.SetHeaderLen((TCP_HEADER_BASE_LENGTH+4)/4,p);
  } else {
    th.SetHeaderLen((TCP_HEADER_BASE_LENGTH/4),p);
  }

  p.PushBackHeader(th);

  cerr << ih << endl;
  cerr << th << endl;
  return p;
}

Packet CreatePayloadPacket(ConnectionList<TCPState>::iterator &ptr,
			   unsigned &bytesize,
			   unsigned int datasize)
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

  unsigned char flags=0;
  SET_PSH(flags);
  SET_ACK(flags);
  th.SetFlags(flags,p);    

  th.SetAckNum((*ptr).state.GetLastRecvd() + 1,p);

  //Set the window size
  th.SetWinSize((*ptr).state.GetRwnd(),p);
  th.SetHeaderLen(TCP_HEADER_BASE_LENGTH/4,p);
  p.PushBackHeader(th);

  cerr << ih << endl;
  cerr << th << endl;
  return p;
}
