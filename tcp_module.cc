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


#include <iostream>

#include "Minet.h"


#define WINDOW_SIZE 65536
#define TIMEOUT 30 
#define TIMEWAIT_TIMEOUT 30


static const char *printstates[] = 
{
  "LISTEN","SYN_SENT","SYN_RECEIVED","ESTABLISHED",
  "FIN_WAIT_1","FIN_WAIT_2","CLOSE_WAIT","CLOSING","LAST_ACK",
  "TIME_WAIT","CLOSED"
};

enum states 
{
  LISTEN,SYN_SENT,SYN_RECEIVED,ESTABLISHED,\
  FIN_WAIT_1,FIN_WAIT_2,CLOSE_WAIT,CLOSING,LAST_ACK,\
  TIME_WAIT,NEW,CLOSED
};

struct TCPState 
{
  /* sender side */
  unsigned int sndUna;
  unsigned int sndNxt;
  unsigned int sndWnd;
  unsigned int sndWL1;
  unsigned int sndWL2;
  bool sndUp;
  unsigned int sndIss;
  Buffer sndBuff;


  /* receiver side */
  unsigned int rcvNxt;
  unsigned int rcvWnd;
  bool rcvUp;
  unsigned int rcvIrs;
  Buffer rcvBuff;

  bool last_status; //flags whether we should forward data up to sock_module 
  //(last_status means sock_module has responded to our last WRITE)

  bool lastFin;  //flag whether we need to set FIN 
  bool lastSyn;  // or SYN on timeout

  states state;

  TCPState() : sndBuff(), rcvBuff()
  { 
    state = NEW;
    sndWnd = WINDOW_SIZE;
    rcvWnd = WINDOW_SIZE;
    last_status=false;
    lastFin=false;
    lastSyn=false;
    sndWL1 = 0;
    sndWL2 = 0;
  }

  TCPState(states st) : sndBuff(), rcvBuff()
  { 
    state = st;
    sndWnd = WINDOW_SIZE;
    rcvWnd = WINDOW_SIZE;
    last_status=false;
    lastFin=false;
    lastSyn=false;
    sndWL1 = 0;
    sndWL2 = 0;
  }
  
  ostream & Print(ostream &os) const 
  { 
    os<<"TCState(";
    os<<"state="<<printstates[state];
    os<<",sndUna="<<sndUna;
    os<<",sndNx="<<sndNxt;
    os<<",sndWnd="<<sndWnd;
    os<<",sndIss="<<sndIss;
    os<<",rcvNxt="<<rcvNxt;
    os<<",rcvWnd="<<rcvWnd;
    os<<",rcvIrs="<<rcvIrs;
    os<<")";
  }
};

void handleTimeout(ConnectionToStateMapping<TCPState> &c);
void writePacket2(ConnectionToStateMapping<TCPState> c, char *data, unsigned dataLen);
void writeSockResponse2(Connection c, srrType type, int error, char *data, unsigned dataLen);
void writeSockResponse(Connection c, srrType type, int error, const Buffer &data, unsigned dataLen);
void writePacket(ConnectionToStateMapping<TCPState> *cs, unsigned int seq, unsigned int ack, unsigned char flags, char *data, unsigned dataLen);
void displayFlags(unsigned char);const unsigned int TCP_MAX_DATA = IP_PACKET_MAX_LENGTH-IP_HEADER_BASE_LENGTH-TCP_HEADER_BASE_LENGTH;

MinetHandle ipmux;
MinetHandle sock;

int main(int argc, char *argv[])
{
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
  
  MinetSendToMonitor(MinetMonitoringEvent("tcp_module operational"));

  MinetEvent event;

  int rc;

  while (1) {
    /* find connection closest to timing out */
    Time current_time;
    Time earliest_timeout;
    Time select_timeout;
    gettimeofday(&current_time,NULL);
    ConnectionList<TCPState>::iterator earliest = clist.FindEarliest();
    earliest_timeout = (*earliest).timeout;
    
    //cerr << "EARLIEST TIMEOUT IS " << earliest_timeout << endl;
    if ((double)earliest_timeout == 0) { 
      //no timers have been set
      select_timeout = 0; //no timeout
    } else {
      select_timeout = earliest_timeout - current_time;
    }

    if ((double)select_timeout < 0) { //if some connections timed out
      select_timeout.tv_sec = 0; //timeout right away
      select_timeout.tv_usec = 0;
    }
   
    //cerr << "SELECT_TIMEOUT IS " << select_timeout << endl;
    
    if ((double)earliest_timeout==0) {
      rc=MinetGetNextEvent(event);
    } else {
      rc=MinetGetNextEvent(event,(double)select_timeout);
    }
    
    if (rc<0) { 
      break;
    } 

    if (event.eventtype==MinetEvent::Timeout) {
      
      cerr << "timeout in select\n";
      
      ConnectionList<TCPState>::iterator i;
      gettimeofday(&current_time,NULL);
      for (i = clist.begin(); i != clist.end(); i++) {
	if ((*i).timeout < current_time)  {
	  if ((*i).state.state == TIME_WAIT) {
	    cerr << "ERASING THE CONNECTION, TIMEWAIT TIMED OUT\n";
	    //writeSockResponse2((*i).connection,WRITE,EOK,"",0);
	    //clist.erase(i);
	  } else {
		handleTimeout(*i);
	  }
	}
      }
      continue;
    } else if (event.eventtype==MinetEvent::Dataflow && event.direction==MinetEvent::IN) {
      /* HANDLING IP_MUX -> TCP_MODULE INTERACTION */
      if (event.handle==ipmux) {
	
	Packet p,replP;
	MinetReceive(ipmux,p);
	
	cerr << "\n\n\nTCP_MODULE: received packet " << endl;
	
	unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
	//cerr << "estimated header len="<<tcphlen<<"\n";
	p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
	IPHeader iph=p.FindHeader(Headers::IPHeader);
	TCPHeader tcph=p.FindHeader(Headers::TCPHeader);
	
	unsigned char iphlen1;
	unsigned iphlen;
	unsigned short len1;
	unsigned len;
	iph.GetHeaderLength(iphlen1);
	iphlen = iphlen1*4;
	iph.GetTotalLength(len1);
	len = len1 - iphlen - tcphlen;  
	cerr << "Length of segment is " << len << endl;
	
	bool checksumok;
	unsigned char flags,replyF=0;
	
	//cerr << "TCP Packet: IP Header is "<<iph<<endl;
	//cerr << "TCP Header is "<<tcph << endl;
	//cerr << "TEXT IS: " << p.GetPayload() << endl;
	// cerr << "Checksum is " << (tcph.IsCorrectChecksum(p) ? "VALID" : "INVALID");
	
	Connection c;
	// note that this is flipped around because
	// "source" is interepreted as "this machine"
	iph.GetDestIP(c.src);
	iph.GetSourceIP(c.dest);
	iph.GetProtocol(c.protocol);
	tcph.GetDestPort(c.srcport);
	tcph.GetSourcePort(c.destport);
	tcph.GetFlags(flags); displayFlags(flags);
	
	unsigned int ack;
	tcph.GetAckNum(ack);
	cerr << "ACK IS " << ack << endl;
	unsigned int seq;
	tcph.GetSeqNum(seq);
	cerr << "SEQ IS " << seq << endl;
	
	ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
	
	//cerr << "CONNECTION THAT REPRESENTS IP PACKET: " << c << endl;
	
	//cerr << "CURRENT CLIST: " << clist;
	
	if (cs!=clist.end()) {
	  
	  if (!tcph.IsCorrectChecksum(p)) {
	    cerr << "Received invalid packet, sending ack " << (*cs).state.rcvNxt << endl;
	    SET_ACK(replyF);
	    writePacket(&(*cs),(*cs).state.sndNxt,(*cs).state.rcvNxt,replyF,"",0);
	    continue;
	  }
	  
	  //cerr << "OUR STATE IS: " << printstates[(*cs).state.state] << endl;
	  bool unbound = ((*cs).connection.dest == IP_ADDRESS_ANY && (*cs).connection.destport == PORT_ANY);
	  
	  if (!unbound)	 {
	    //cerr << "FOUND MATCHING CONNECTION " << (*cs) << endl;
	    
	    switch ((*cs).state.state)  {
	    case SYN_SENT:
	      {
		//cerr << "processing in SYN_SENT\n";
		bool ackAcceptable;
		if (IS_ACK(flags)) {
		  //cerr << "ACK IS ON\n";
		  if (ack > (*cs).state.sndNxt || ack <= (*cs).state.sndIss)  {
		    ackAcceptable=false;
		    cerr << "ACK " << ack << " is not acceptable\n";
		    if (!IS_RST(flags))  {
		      SET_RST(replyF);
		      writePacket(&(*cs),ack,0,replyF,"",0);
		      break;
		    }
		  } else if ((*cs).state.sndUna <= ack && ack <= (*cs).state.sndNxt)  {
				//cerr << "ack is acceptable\n";
		    ackAcceptable = true;
		  }
		}
		if (IS_RST(flags)) {
		  cerr << "segment contains RST\n";
		  if (ackAcceptable)  {
		    cerr << "sending an ECONN_FAILED to app\n";
		    writeSockResponse2((*cs).connection,WRITE,ECONN_FAILED,"",0);
		    (*cs).state.state = CLOSED;
				//cs.erase();
		  }
		  break;
		}
		if (IS_SYN(flags)) {
		  //cerr << "segment contains SYN\n";
		  (*cs).state.rcvNxt = seq+1;
		  (*cs).state.rcvIrs = seq;
		  (*cs).state.sndUna = ack; //for now, assume it's not a simultaneous open, so ack is set
		  if ((*cs).state.sndUna > (*cs).state.sndIss) { //our syn has been acknowledged
		    cerr << "TRANSFERRING CONNECTION TO ESTABLISHED\n";
		    (*cs).state.state = ESTABLISHED;
		    (*cs).timeout = 0;
		    SET_ACK(replyF);
		    writePacket(&(*cs),(*cs).state.sndNxt,(*cs).state.rcvNxt,replyF,"",0);
		    writeSockResponse2(c,WRITE,EOK,"",0);
		  } else {
		    cerr << "Ignoring simultaneous open\n";
		  }
		}
		if (!(IS_RST(flags) || IS_SYN(flags))) {
		  //cerr << "neither RST nor SYN are on, breaking out of handling SYN_SENT state\n";
		  break;
		}
	      }
	      break;
	      
	    case SYN_RECEIVED:
	    case ESTABLISHED:
	    case FIN_WAIT_1:
	    case FIN_WAIT_2:
	    case CLOSE_WAIT:
	    case CLOSING:
	    case LAST_ACK:
	    case TIME_WAIT:
	      {      
		unsigned rcvWnd = (*cs).state.rcvWnd;
		unsigned rcvNxt = (*cs).state.rcvNxt;
		bool seqAcceptable;
		bool ackAcceptable;
		
		//check sequence number
		if (len == 0 && rcvWnd==0) {
		  seqAcceptable = (seq == rcvNxt);
		} else if (len == 0 && rcvWnd > 0) {
		  seqAcceptable = (rcvNxt <= seq && seq < rcvNxt+rcvWnd);
		} else if (len > 0 && rcvWnd == 0) {
		  seqAcceptable = false;
		} else {
		  seqAcceptable = ((rcvNxt <= seq && seq < rcvNxt+rcvWnd) ||
				   (rcvNxt <= seq+len-1 && seq+len-1 < rcvNxt+rcvWnd));			
		}
		
		if (!seqAcceptable) {
		  cerr << "seq " << seq << " is not acceptable\n";
		  
		  if (!IS_RST(flags)) {
				//cerr << "reset not set, sending ack " << rcvNxt << endl;
		    SET_ACK(replyF);
		    writePacket(&(*cs),(*cs).state.sndNxt,rcvNxt,replyF,"",0);
		  }
		  break;
		}

		//now that we know SEQ is acceptable, trim the segment so it fits within the window			
		seq  = max((*cs).state.rcvNxt,seq);
		unsigned int end  = min((*cs).state.rcvNxt + (*cs).state.rcvWnd,seq+len);
		len = end - seq;
		//cerr << " Trimmed seq is " << seq << endl;
		//cerr << " Trimmed len is " << len << endl;
		
		//check the RST bit
		if (IS_RST(flags)) {
		  cerr << "RST IS ON\n";
		  //stopProcessing=true;
		  switch((*cs).state.state)   {
		  case SYN_RECEIVED:
		    {
		      //assume it was opened with a passive OPEN (didn't get here through simul. open
		      (*cs).state.state = LISTEN;
		    }
		    break;
		  case ESTABLISHED:
		  case FIN_WAIT_1:
		  case FIN_WAIT_2:
		  case CLOSE_WAIT:
		    {
		      writeSockResponse2(c,WRITE,ECONN_FAILED,"",0);
		    }
		    break;
		  case CLOSING:
		  case LAST_ACK:
		  case TIME_WAIT:
		    (*cs).state.state = CLOSED;
		    clist.erase(cs);
		  }
		  break; 
		}
		
		// check the SYN bit
		if (IS_SYN(flags)) {
		  //cerr << "SYN IS ON\n";
		  
		  writeSockResponse2(c,WRITE,ECONN_FAILED,"",0);
		  (*cs).state.state = CLOSED;
		  clist.erase(cs);
		  break;
		}
		    
		unsigned int newBytesAcknowledged=0;
		//check the ACK field
		if (!IS_ACK(flags)) {
		  break;
		} else  {
		  ackAcceptable = ((*cs).state.sndUna <= ack && ack <= (*cs).state.sndNxt);
		  unsigned short win;
		  tcph.GetWinSize(win);
		  if (ackAcceptable) {
		    if (ack  == (*cs).state.sndNxt && win > 0) {
		      // if everything has been acknowledged and sndWnd > 0
		      if ((*cs).state.state != TIME_WAIT) {
			cerr << "cancelling timer...\n";
			(*cs).timeout = 0; 
		      }
		    } else {
		      cerr << "restarting timer...\n";
		      Time current_time;
		      gettimeofday(&current_time,NULL);
		      (*cs).timeout = (double)current_time + TIMEOUT; //set timer for unacknowledged byte
		      //cerr << "timeout is now " << (*cs).timeout << endl;
		    }
		  }
		  switch((*cs).state.state) {
		  case SYN_RECEIVED:
		    {
		      if (ackAcceptable){ 
			cerr << "\nentering ESTABLISHED state\n";
			(*cs).state.state = ESTABLISHED;
			(*cs).state.sndUna = ack;
			
			//cerr << "Letting the sock_module know...\n";
			writeSockResponse2(c,WRITE,EOK,"",0);
		      }  else {
			cerr << ack << " is not acceptable, sending reset \n";
			SET_RST(replyF);
			writePacket(&(*cs),ack,0,replyF,"",0);
			break;
		      }
		    }
		    break;
		  case ESTABLISHED:
		  case FIN_WAIT_1:
		  case FIN_WAIT_2:
		  case LAST_ACK:
		    {
		      if (ackAcceptable) {
			newBytesAcknowledged = ack - (*cs).state.sndUna;
			
			/* delete successfully sent data */
			(*cs).state.sndBuff.ExtractFront(newBytesAcknowledged);
		      }	 else if (ack > (*cs).state.sndNxt) {
			SET_ACK(replyF);
			writePacket(&(*cs),(*cs).state.sndNxt,rcvNxt,replyF,"",0);
			break;
		      } else {
			break;
		      }
		      
		      //update SEND WINDOW
		      if ((*cs).state.sndUna < ack && ack <= (*cs).state.sndNxt) {
			if ((*cs).state.sndWL1 < seq ||
			    ((*cs).state.sndWL1 == seq && (*cs).state.sndWL2 <= ack))  {
			  unsigned short win;
			  tcph.GetWinSize(win);					  
			  (*cs).state.sndWnd = win * 4;
			  //cerr << "UPDATING sndWnd, it is now " << (*cs).state.sndWnd << endl;
			  (*cs).state.sndWL1 = seq;
			  (*cs).state.sndWL2 = ack;
			}
		      }

		      (*cs).state.sndUna = ack;

		      /* FINISHED PROCESSING ESTABLISHED STATE AS FAR AS ACK IS CONCERNED*/
		      if ((*cs).state.state == ESTABLISHED) {
			break;
		      }

		      if ((*cs).state.state == FIN_WAIT_1){
			//cerr << "(*cs).state.sndUna is " << (*cs).state.sndUna << endl;
			//cerr << "ack is " << ack << endl;
			//cerr << "newBytesAcknowledged is " << newBytesAcknowledged << endl;
			if ((*cs).state.sndUna == (*cs).state.sndNxt)  {
			  cerr << "ENTERING FIN_WAIT_2, (our FIN is acknowledged, now wait for other side's FIN)\n";
			  (*cs).state.state = FIN_WAIT_2;
			}
			break;
		      }
					  
		      if ((*cs).state.state == FIN_WAIT_2) {
			if (IS_FIN(flags)) {
			  cerr << "Entering the TIME_WAIT(sent FIN, got FIN, got ACK,)\n";
			  (*cs).state.state = TIME_WAIT;
			  //(*cs).timeout = TIMEWAIT_TIMEOUT;
			  Time current_time;
			  gettimeofday(&current_time,NULL);
			  cerr << "ENTERING TIMEWAIT, setting timer...\n";
			  (*cs).timeout = (double)current_time + TIMEWAIT_TIMEOUT;
			  //send out anything that's left in the rcv buffer
			  //cerr << "Sending out whatever's left in the buffer\n";	
			  //writeSockResponse(c,WRITE,EOK,(*cs).state.rcvBuff,(*cs).state.rcvBuff.GetSize());
			}
			break;
		      }

		      if ((*cs).state.state == LAST_ACK) {
			if ((*cs).state.sndUna == (*cs).state.sndNxt){
			  cerr << "Entering the TIME_WAIT state (got FIN, sent ACK, got CLOSE, sent FIN, got ACK, now will wait for..?";
			  (*cs).state.state = TIME_WAIT;
			  //(*cs).timeout = TIMEWAIT_TIMEOUT;
			  Time current_time;
			  gettimeofday(&current_time,NULL);
			  (*cs).timeout = (double)current_time + TIMEWAIT_TIMEOUT;
			  cerr << "ENTERING TIMEWAIT, setting timer...\n";
			  //send out anything that's left in the rcv buffer		
			  //send out anything that's left in the rcv buffer
			  //writeSockResponse(c,WRITE,EOK,(*cs).state.rcvBuff,(*cs).state.rcvBuff.GetSize());
			  //cerr << "Sending out whatever's left in the buffer\n";	
			  
			}
			break;
		      }
		    }
		  }    
		}

		// PROCESS TEXT
		switch ((*cs).state.state)  {
		case ESTABLISHED:
		case FIN_WAIT_1:
		case FIN_WAIT_2:
		  {
		    unsigned int newBytes = len;
		    unsigned int oldseq;
		    tcph.GetSeqNum(oldseq);
		    Buffer newBuffer =  p.GetPayload().Extract(seq-oldseq,newBytes);
		    if (newBytes > 0) {
		      (*cs).state.rcvNxt = seq + newBytes;
		      
		      /* UPDATE RECEIVE WINDOW */
		      (*cs).state.rcvWnd -= newBytes;
		      
		      //TODO: piggyback our data onto the ACK. For now, just send the ACK
		      SET_ACK(replyF);
		      writePacket(&(*cs), (*cs).state.sndNxt, (*cs).state.rcvNxt,replyF,"",0);
		      
		      /* add data to our receive buffer */
		      (*cs).state.rcvBuff.AddBack(newBuffer);
		      
		      //cerr << "RCV BUFFER: " << (*cs).state.rcvBuff << endl;
		      
		      /* FORWARD TEXT TO APPLICATION */
		      if ((*cs).state.rcvBuff.GetSize() > 0 && (*cs).state.last_status==true) {
			writeSockResponse(c,WRITE,EOK,(*cs).state.rcvBuff,0);
			(*cs).state.last_status=false;
			//cerr << "FORWARDING TEXT TO THE APPLICATION\n";
		      }
		    }
		  }
		  //cerr << "breaking from text processing\n";
		  break;
		case CLOSE_WAIT:
		case CLOSING:
		case LAST_ACK:
		case TIME_WAIT:
		  {
		    //cerr << "WE ARE IN STATE " << printstates[(*cs).state.state] << endl;
		    //cerr << "We received a FIN - the other side promised not to sent us data. Ignoring the segment\n";
		  }
		  break;
		}
		
		// check FIN bit
		if (IS_FIN(flags))  {
		  //cerr << "FIN IS ON, processing...\n";
		  if ((*cs).state.state == CLOSED ||
		      (*cs).state.state == LISTEN ||
		      (*cs).state.state == SYN_SENT)  {
		    cerr << "Received a fin in state " << printstates[(*cs).state.state] << endl;
		    cerr << "Ignoring it because seq " << seq << " cannot be validated\n";
		    break;
		  } else {
		    (*cs).state.rcvNxt = seq + (len == 0? 1 : 0);
		    SET_ACK(replyF);
		    if (1) {
		      //len == 0) 
		      //cerr << "haven't acked text, acking fin...\n";
		      writePacket(&(*cs),(*cs).state.sndNxt,(*cs).state.rcvNxt,replyF,"",0);
		    }
		    switch ((*cs).state.state) {
		    case SYN_RECEIVED:
		    case ESTABLISHED:
		      {
			cerr << "TRANSFERRING TO CLOSE_WAIT\n";
			(*cs).state.state = CLOSE_WAIT;
			cerr << "writing a 0 byte to sock_module\n";
			writeSockResponse2(c,WRITE,EOK,"",0);
		      }
		      break;
		    case FIN_WAIT_1:
		      {
			if ((*cs).state.sndUna == (*cs).state.sndNxt) {
			  cerr << "transferring to TIME_WAIT";
			  (*cs).state.state = TIME_WAIT;
			  //turn on the time_wait timeout, turn off other timers 
			  Time current_time;
			  gettimeofday(&current_time,NULL);
			  cerr << "ENTERING TIMEWAIT, setting timer...\n";
			  (*cs).timeout = (double)current_time + TIMEWAIT_TIMEOUT;
			  (*cs).state.state = TIME_WAIT;
			} else {
			  cerr << "SIMULTANEOUS CLOSE\n";
			  (*cs).state.state = CLOSING; //simultaneous close
			}
		      }
		      break;
		    case FIN_WAIT_2:
		      {
			cerr << "Transferring to TIME_WAIT!\n";
			(*cs).state.state = TIME_WAIT;
			//TODO: start the time-wait timer
			Time current_time;
			gettimeofday(&current_time,NULL);
			(*cs).timeout = (double)current_time + TIMEWAIT_TIMEOUT;
			cerr << "ENTERING TIMEWAIT, setting timer...\n";
			(*cs).state.state = TIME_WAIT;
		      }
		      break;
		    case CLOSE_WAIT:
		      //nothing;
		      break;
		    case CLOSING:
		      //nothing
		      break;
		    case LAST_ACK:
		      //nothing
		      break;
		    case TIME_WAIT:
		      //restart the time-wait timer
		      break;
		    }
		    
		  }
		}				    				  
	      }	
	    }
	  } else {
	    //no bound connection matches, look for a listening connection
	    if (cs == clist.end()) { 
	      // IF THIS PACKET ISN'T FOR AN EXISTING CONNECTION
	      if (IS_RST(flags))  {
		cerr << "Received a reset for a non-existing connection, responding with a reset\n";
		if (!IS_ACK(flags))  {
		  SET_ACK(replyF);
		  SET_RST(replyF);
		  writePacket(&(*cs),0,seq+len,replyF,"",0);
		} else {
		  SET_RST(replyF);
		  cerr << "will writePacket with seq " << ack << endl;
		  writePacket(&(*cs),ack,0,replyF,"",0);
		}
		break;
	      }
	      cerr << "Received packet for a non-existing connection, ignoring \n";
	      break;
	    }

	    switch ((*cs).state.state) {
	    case LISTEN:
	      {
		if (IS_RST(flags)) {
		  break;
		}  else if (IS_ACK(flags)) {
		  SET_RST(replyF);
		  writePacket(&(*cs),ack,0,replyF,"",0);
		  break;
		} else if (IS_SYN(flags)) {
		  (*cs).state.rcvNxt = seq+1;
		  (*cs).state.rcvIrs = seq;
		  
		  SET_ACK(replyF);
		  SET_SYN(replyF);
		  
		  (*cs).state.sndIss = 0;  // TODO -- pick an ISS 
		  writePacket(&(*cs),(*cs).state.sndIss,(*cs).state.rcvNxt,replyF,"",0);
		  (*cs).state.sndNxt = (*cs).state.sndIss+1;
		  (*cs).state.sndUna = (*cs).state.sndIss;
		  (*cs).state.state = SYN_RECEIVED;
		  
		  //fill in unbound destination
		  (*cs).connection.dest = c.dest;
		  (*cs).connection.destport = c.destport;
		  
		  //start timer for our SYN
		  Time current_time;
		  gettimeofday(&current_time,NULL);
		  (*cs).timeout = (double)current_time+TIMEOUT;
		} else {
		  ;//cerr << "Ignoring a non-RST,non-ACK,non-SYN packet for a listening connection\n";
		  break;
		}
	      }
	      break;
	    default:
	      cerr << "Received a packet for a non-bound but not-listening connection\n";
	    }
	  }
	} else {
	  cerr << "Received a packet for a non-existing connection ";
	  if (IS_RST(flags)) {
	    cerr << "containing RST, ignoring\n";
	  } else  {
	    cerr << "not containing RST, sending RST\n";
	    if (!IS_ACK(flags)) {
	      SET_RST(replyF); SET_ACK(replyF);
	      writePacket(&(*cs),0,seq+len,replyF,"",0);
	    }  else {
	      SET_RST(replyF);
	      writePacket(&(*cs),ack,0,replyF,"",0);
	    }
	  }
	}
      }
      
      /* HANDLING SOCK_MODULE -> TCP_MODULE INTERACTION */
      if (event.handle==sock) {
	SockRequestResponse req;
	MinetReceive(sock,req);
	
	cerr << "\n\n\nreceived SockRequestResponse " << req << endl;
	
	ConnectionList<TCPState>::iterator cs;
	cs = clist.FindMatching(req.connection);
	
	unsigned char flags=0;
	switch (req.type) {
	  // case SockRequestResponse::CONNECT: 
	case CONNECT: 
	  {
	    SockRequestResponse repl;
	    writeSockResponse2(req.connection,STATUS,EOK,"",0);
	    
	    SET_SYN(flags);
	    struct ConnectionToStateMapping<TCPState> cst(req.connection,Time(),TCPState(SYN_SENT));
	    cst.state.sndNxt = 1;
	    cst.state.sndIss = 0;
	    cst.state.sndUna = 0;
	    // set timer
	    Time current_time;
	    gettimeofday(&current_time,0);
	    cst.timeout = (double)current_time+TIMEOUT; 
	    
	    writePacket(&cst,0,0,flags,"",0);
	    
	    clist.push_back(cst);
	  }
	  break;
	case ACCEPT: 
	  {
	    writeSockResponse2(req.connection,STATUS,EOK,"",0);
	    struct ConnectionToStateMapping<TCPState> cst(req.connection,Time(),TCPState(LISTEN));
	    clist.push_back(cst);
	    cst.state.state = LISTEN;
	    //cerr << "ADDED " << cst << endl;
	    
	  }
	  break;
	  // case SockRequestResponse::STATUS: 
	case STATUS: 
	  //cerr << "App accepted " << req.bytes << " bytes...\n";
	  (*cs).state.rcvBuff.ExtractFront(req.bytes);
	  (*cs).state.last_status = true;
	  
	  /* increase receive window */
	  (*cs).state.rcvWnd += req.bytes; 
	  
	  /* if we got something sinse our last WRITE (but before sock_module replied with STATUS), write it now) */
	  if ((*cs).state.rcvBuff.GetSize() > 0)   {
	      writeSockResponse(req.connection,WRITE,EOK,(*cs).state.rcvBuff,(*cs).state.rcvBuff.GetSize());
	      (*cs).state.last_status = false;
	  }
	  
	  break;
	case WRITE: 
	  {
	    unsigned bytes = MIN((*cs).state.sndWnd, req.data.GetSize());
	    
	    //cerr << "App wants us to write " << req.data << " which is " << req.data.GetSize() << " bytes long\n";
	    //cerr << "Our free space: " << (*cs).state.sndWnd << endl;
	    //cerr << "We're going to queue " << bytes << " bytes\n";
	    // create the payload of the packet
	    //Buffer buff(req.data.ExtractFront(bytes));
	    char *buff = (char *)malloc(bytes);
	    req.data.GetData(buff,bytes,0);
	    SET_ACK(flags);
	    writePacket(&(*cs),(*cs).state.sndNxt,(*cs).state.rcvNxt,flags,buff,bytes);
	    
	    free(buff);
	    
	    if ((*cs).state.sndNxt == (*cs).state.sndUna)   {
	      cerr << "setting timer...\n";
	      Time current_time;
	      gettimeofday(&current_time,0);
	      (*cs).timeout = (double)current_time + TIMEOUT;
	    }
	    (*cs).state.sndNxt += bytes;
	    //cerr << "sndNxt is now " << (*cs).state.sndNxt << endl; 
	    (*cs).state.sndBuff.AddBack(Buffer(buff,bytes));
	    
	    
	    SockRequestResponse repl;
	    // repl.type=SockRequestResponse::STATUS;
	    repl.type=STATUS;
	    repl.connection=req.connection;
	    repl.bytes=bytes;
	    repl.error=EOK;
	    MinetSend(sock,repl);
	  }
	  break;
	  
	  // case SockRequestResponse::FORWARD:
	case FORWARD:
	  {
	    SockRequestResponse repl;
	    // repl.type=SockRequestResponse::STATUS;
	    repl.type=STATUS;
	    repl.connection=req.connection;
	    repl.error=EOK;
	    repl.bytes=0;
	    MinetSend(sock,repl);
	  }
	  break;
	  
	  // case SockRequestResponse::CLOSE:
	case CLOSE:
	  {
	    //cerr << "\n\n\nHANDLING CLOSE REQUEST\n";
	    ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
	    SockRequestResponse repl;
	    repl.connection=req.connection;
	    repl.type=STATUS;
	    if (cs==clist.end()) {
	      repl.error=ENOMATCH;
	    } else {
	      repl.error=EOK;
	    }
	    MinetSend(sock,repl);
	    
	    //And send the fin
	    SET_FIN(flags);
	    SET_ACK(flags);
	    (*cs).state.state = ((*cs).state.state == ESTABLISHED? FIN_WAIT_1:LAST_ACK);
	    cerr << "TRANSFERRING TO " << printstates[(*cs).state.state] << endl;
	    writePacket(&(*cs),(*cs).state.sndNxt,(*cs).state.rcvNxt,flags,"",0);
	    if ((*cs).state.sndUna == (*cs).state.sndNxt)  {
	      Time current_time;
	      gettimeofday(&current_time,NULL);
	      cerr << "SETTING TIMER FOR FIN\n";
	      (*cs).timeout = (double)current_time + TIMEOUT;
	    }
	    (*cs).state.sndNxt++; //advance over FIN
	  }
	  break;
	default:
	  {
	    SockRequestResponse repl;
	    // repl.type=SockRequestResponse::STATUS;
	    repl.type=STATUS;
	    repl.error=EWHAT;
	    MinetSend(sock,repl);
	  }
	}
      }
    }
    
    /*      if (FD_ISSET(fromsock,&read_fds)) {
	    Packet p;
	    p.Unserialize(fromsock);
	    // add ip header here
	    if (CanWriteNow(toipmux)) { 
	    p.Serialize(toipmux);
	    } else {
	    cerr << "Discarded TCP packet because IP was not ready\n";
	    }
	    }
	    #endif*/
    
  }
  MinetDeinit();
  return 0;
}

void writeSockResponse2(Connection c, srrType type, int error, char *data, unsigned dataLen)
{
SockRequestResponse repl;
Buffer buff(data,dataLen);
repl.type=type;
repl.connection=c;
repl.data = buff;
repl.bytes=dataLen;
repl.error=error;
MinetSend(sock,repl);
}

void writeSockResponse(Connection c, srrType type, int error, const Buffer &data, unsigned dataLen)
{
SockRequestResponse repl;
repl.type=type;
repl.connection=c;
repl.data = data;
repl.bytes=dataLen;
repl.error=error;
MinetSend(sock,repl); 

}


void writePacket(ConnectionToStateMapping<TCPState> *cs,unsigned int seq,unsigned int ack,
  unsigned char flags,char *data,unsigned dataLen)
{
  Connection c = (*cs).connection;
  Packet p(data,dataLen);
  IPHeader ih;
  ih.SetProtocol(IP_PROTO_TCP);
  ih.SetSourceIP(c.src);
  ih.SetDestIP(c.dest);
  ih.SetHeaderLength(5);
  ih.SetTotalLength(dataLen+40);
  p.PushFrontHeader(ih);

  TCPHeader th;
  th.SetSourcePort(c.srcport,p);
  th.SetDestPort(c.destport,p);
  th.SetHeaderLen(5,p);
  th.SetSeqNum(seq,p);
  th.SetAckNum(ack,p);
  th.SetFlags(flags,p);
  th.SetWinSize((*cs).state.rcvWnd/4,p); //TODO: get correct window size
  p.PushBackHeader(th);

  unsigned short win;
  th.GetWinSize(win);
  //cerr << "the win that we're advertising is ... " << win << " (in words)" << endl;
  cerr << "SENDING PACKET: seq  " << seq << ", ack " << ack << endl;
  displayFlags(flags);
  if (dataLen > 0)
    cerr << "TEXT: " << data << endl;

  MinetSend(ipmux,p);
}

void writePacket2(ConnectionToStateMapping<TCPState> cs, unsigned seq, unsigned ack, unsigned char flags,
		  Buffer data, unsigned dataLen)
{
  Connection c = cs.connection;

  Packet p(data);
  IPHeader ih;
  ih.SetProtocol(IP_PROTO_TCP);
  ih.SetSourceIP(c.src);
  ih.SetDestIP(c.dest);
  ih.SetHeaderLength(5);
  ih.SetTotalLength(dataLen+40);
  p.PushFrontHeader(ih);

  TCPHeader th;
  th.SetSourcePort(c.srcport,p);
  th.SetDestPort(c.destport,p);
  th.SetHeaderLen(5,p);
  th.SetSeqNum(seq,p);
  th.SetAckNum(ack,p);
  th.SetFlags(flags,p);
  th.SetWinSize(cs.state.rcvWnd/4,p); //TODO: get correct window size
  p.PushBackHeader(th);

  unsigned short win;
  th.GetWinSize(win);
  //  cerr << "the win that we're advertising is ... " << win << endl;
  cerr << "SENDING PACKET: seq  " << seq << ", ack " << ack << endl;
  displayFlags(flags);
  if (dataLen > 0)
    cerr << "TEXT: " << data << endl;

  MinetSend(ipmux,p);
}


void displayFlags(unsigned char flags)
{
  cerr << "FLAGS ARE: ";
  if (IS_URG(flags))
    cerr << "URG, ";
  if (IS_ACK(flags))
    cerr << "ACK, ";
  if (IS_PSH(flags))
    cerr << "PSH, ";
  if (IS_RST(flags))
    cerr << "RST, ";
  if (IS_SYN(flags))
    cerr << "SYN, ";
  if (IS_FIN(flags))
    cerr << "FIN, ";
  cerr << "\n";
}

void handleTimeout(ConnectionToStateMapping<TCPState> &c)
{
  unsigned bytesToResend = c.state.sndNxt - c.state.sndUna;
  unsigned char flags=0;
  char *buf;
  unsigned sndUna = c.state.sndUna;

  if (c.state.state == SYN_SENT)
    {
      SET_SYN(flags); //will resend syn
      cerr << "RESENDING SYN\n";
      if (bytesToResend == 1)
	bytesToResend = 0;
    }
  else if (c.state.state == SYN_RECEIVED)
    {
      SET_SYN(flags);
      SET_ACK(flags);
      cerr << "RESENDING SYN,ACK\n";
      if (bytesToResend == 1)
	bytesToResend = 0;
    }
  else if (c.state.state == FIN_WAIT_1 || c.state.state == CLOSING || c.state.state == LAST_ACK) //didn't get acknowledgement for our FIN, resend it
    {
      SET_FIN(flags);
      SET_ACK(flags);
      cerr << "RESENDING FIN\n";
    }
  else
    SET_ACK(flags);


  buf = (char *)malloc(bytesToResend);
  c.state.sndBuff.GetData(buf,bytesToResend,0);

  //cerr << "RESENDING " << buf << endl;

  writePacket(&c,sndUna,c.state.rcvNxt,flags,buf,bytesToResend);

  //restart timeout
  //cerr << "restarting timeout...\n";
  Time current_time;
  gettimeofday(&current_time,NULL);
  c.timeout = (double)current_time+TIMEOUT;
}
