#include <fcntl.h>

#include "minet_socket.h"
#include "config.h"
#include "sockint.h"
#include "ip.h"

#define UNINIT_SOCKS 0
#define KERNEL_SOCKS 1
#define MINET_SOCKS 2  
 

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif


int socket_type = 0;
int fromsock, tosock;
int minet_errno = EOK;


EXTERNC int minet_init (minet_socket_types type) {
  if (type == MINET_KERNEL) 
    socket_type = KERNEL_SOCKS;
  else {
    socket_type = MINET_SOCKS;
    fromsock = open(sock2app_fifo_name, O_RDONLY);
    tosock = open (app2sock_fifo_name, O_WRONLY);
  }
  return (socket_type);
}


EXTERNC int minet_deinit () {
  if (socket_type == MINET_SOCKS) {
    close(fromsock);
    close(tosock);
  }
  socket_type = UNINIT_SOCKS;
  return (socket_type);
}


EXTERNC int minet_error () {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (errno);
    break;
  case MINET_SOCKS:
    return (minet_errno);
    break;
  }
}


EXTERNC int minet_perror (char *s) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    perror (s);
    return (0);
    break;
  case MINET_SOCKS:
    cout << s;
    switch (minet_errno) {
    case EOK:
      cout << ": Ok" << endl;
      break;
    case ENOMATCH:
      cout << ": No match found" << endl;
      break;
    case EBUF_SPACE:
      cout << ": Buffer space error" << endl;
      break;
    case EUNKNOWN:
      cout << ": Unknown error" << endl;
      break;
    case ERESOURCE_UNAVAIL:
      cout << ": Resource unavailable" << endl;
      break;
    case EINVALID_OP:
      cout << ": Invalid operation" << endl;
      break;
    case ENOT_IMPLEMENTED:
      cout << ": Function not implemented" << endl;
      break;
    case ENOT_SUPPORTED:
      cout << ": Not supported" << endl;
      break;
    case EWOULD_BLOCK:
      cout << ": Would block" << endl;
      break;
    case ECONN_FAILED:
      cout << ": Connection failed" << endl;
      break;
    }
    return (0);
    break;
  }
}


EXTERNC int minet_socket (int type) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
  case KERNEL_SOCKS:
    if ((type == SOCK_STREAM) || (type == SOCK_DGRAM)) 
      return (socket (AF_INET, type, 0));
  case MINET_SOCKS:
    SockLibRequestResponse slrr;
    switch (type) {
    case SOCK_STREAM:
      slrr = SockLibRequestResponse(mSOCKET,
				    Connection(IP_ADDRESS_ANY,
					       IP_ADDRESS_ANY,
					       TCP_PORT_NONE,
					       TCP_PORT_NONE,
					       IP_PROTO_TCP),
				    UNSPECIFIED_SOCK,
				    Buffer(),
				    0, 0);
      break;
    case SOCK_DGRAM:
      slrr = SockLibRequestResponse(mSOCKET,
				    Connection(IP_ADDRESS_ANY,
					       IP_ADDRESS_ANY,
					       UDP_PORT_NONE,
					       UDP_PORT_NONE,
					       IP_PROTO_UDP),
				    UNSPECIFIED_SOCK,
				    Buffer(),
				    0, 0);
      break;
    }
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (slrr.sockfd);
  }
}


EXTERNC int minet_bind (int sockfd, struct sockaddr_in *myaddr) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
  case KERNEL_SOCKS:
    return (bind (sockfd, (sockaddr *) myaddr, sizeof(*myaddr)));
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mBIND,
				Connection(IPAddress(ntohl((unsigned)myaddr->sin_addr.s_addr)),
					   IP_ADDRESS_ANY,
					   (unsigned short)
					   ntohs(myaddr->sin_port),
					   PORT_NONE, 
					   0),
				sockfd,
				Buffer(),
				0, 0);
    //    cerr << slrr << endl;
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (0);
  }
}


EXTERNC int minet_listen (int sockfd, int backlog) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
  case KERNEL_SOCKS:
    return (listen (sockfd, backlog));
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mLISTEN,
				Connection(),
				sockfd,
				Buffer(),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (0);
  }
}


EXTERNC int minet_accept (int sockfd, struct sockaddr_in *addr) {
  socklen_t length = sizeof(*addr);
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (accept (sockfd, (sockaddr *) addr, &length));
    break;
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mACCEPT,
				Connection(IP_ADDRESS_ANY,
					   IPAddress((unsigned)
						     addr->sin_addr.s_addr),
					   TCP_PORT_NONE,
					   (unsigned short)
					   addr->sin_port,
					   0),
				sockfd,
				Buffer(),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (slrr.sockfd);
  }
}


EXTERNC int minet_connect (int sockfd, struct sockaddr_in *addr) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (connect (sockfd, (sockaddr *) addr, sizeof(*addr)));
    break;
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mCONNECT,
				Connection(IP_ADDRESS_ANY,
					   IPAddress(ntohl((unsigned)
						     addr->sin_addr.s_addr)),
					   TCP_PORT_NONE,
					   ntohs((unsigned short)
						 addr->sin_port),
					   0),
				sockfd,
				Buffer(),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK) {
      return (-1);
    }
    return (0);
  }
}


EXTERNC int minet_read (int fd, char *buf, int len) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (read (fd, buf, len));
    break;
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mREAD,
				Connection(),
				fd,
				Buffer(buf, len),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    Buffer b;
    b = slrr.data;
    b.GetData(buf, b.GetSize(), 0);
    return (b.GetSize());
  }
}


EXTERNC int minet_write (int fd, char *buf, int len) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (write (fd, buf, len));
    break;
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mWRITE,
				Connection(),
				fd,
				Buffer(buf, len),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (slrr.bytes);
  }
}


EXTERNC int minet_recvfrom (int fd, char *buf, int len, 
			    struct sockaddr_in *from) {
  socklen_t length = sizeof(*from);
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (recvfrom (fd, buf, len, 0, (sockaddr *) from, &length));
    break;
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mRECVFROM,
				Connection(IP_ADDRESS_ANY,
					   IPAddress((unsigned)
						     from->sin_addr.s_addr),
					   UDP_PORT_NONE,
					   (unsigned short)
					   from->sin_port,
					   0),
				fd,
				Buffer(buf, len),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (0);
  }
}


EXTERNC int minet_sendto (int fd, char *buf, int len, 
			  struct sockaddr_in *to) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (sendto (fd, buf, len, 0, (sockaddr *) to, sizeof(*to)));
    break;
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mSENDTO,
				Connection(IP_ADDRESS_ANY,
					   IPAddress(ntohl((unsigned)
							   to->sin_addr.s_addr)),
					   UDP_PORT_NONE,
					   ntohs((unsigned short)
						 to->sin_port),
					   0),
				fd,
				Buffer(buf, len),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (0);
  }
}


EXTERNC int minet_close (int sockfd) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (close (sockfd));
    break;
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mCLOSE,
				Connection(),
				sockfd,
				Buffer(),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (0);
  }
}


EXTERNC int minet_select (int             minet_maxfd,
			  fd_set         *minet_read_fds,
			  fd_set         *minet_write_fds,
			  fd_set         *minet_except_fds,
			  struct timeval *timeout) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (select (minet_maxfd, minet_read_fds, minet_write_fds, 
		    minet_except_fds, timeout));
    break;
  case MINET_SOCKS:
    fd_set minet_read_fifos, minet_write_fifos, minet_except_fifos;
    FD_ZERO(&minet_read_fifos); 
    FD_ZERO(&minet_write_fifos); 
    FD_ZERO(&minet_except_fifos);
    for (int fd=0; fd<minet_maxfd+1; fd++)
      if(FD_ISSET(fd, minet_read_fds)) {
	fd = open(sock2app_fifo_name, O_RDONLY);
	FD_SET(fd, &minet_read_fifos);
      }
      else if(FD_ISSET(fd, minet_write_fds)) {
	fd = open(app2sock_fifo_name, O_WRONLY);
	FD_SET(fd, &minet_write_fifos);
      }
      else if(FD_ISSET(fd, minet_except_fds)) {
	fd = open(app2sock_fifo_name, O_WRONLY);
	FD_SET(fd, &minet_except_fifos);
      };
    int numfds = select(minet_maxfd, &minet_read_fifos, &minet_write_fifos, 
			&minet_except_fifos, timeout);
    SockLibRequestResponse slrr;
    if (numfds) {
      slrr = SockLibRequestResponse(mSELECT,
				    Connection(),
				    UNSPECIFIED_SOCK,
				    Buffer(),
				    0,
				    ESET_SELECT,
				    minet_read_fifos,
				    minet_write_fifos,
				    minet_except_fifos);
      
      slrr.Serialize (tosock);
      slrr.Unserialize (fromsock);
    } else {
      slrr = SockLibRequestResponse(mSELECT,
				    Connection(),
				    UNSPECIFIED_SOCK,
				    Buffer(),
				    0,
				    ECLEAR_SELECT,
				    minet_read_fifos,
				    minet_write_fifos,
				    minet_except_fifos);
      
      slrr.Serialize (tosock);
      slrr.Unserialize (fromsock);
    }
    int ctr=0;
    for(int j=0; j<minet_maxfd+1; j++)
      if(FD_ISSET(j, &minet_read_fifos)) ctr++;
      else if(FD_ISSET(j, &minet_write_fifos)) ctr++;
      else if(FD_ISSET(j, &minet_except_fifos)) ctr++;
    return (ctr);
    break;
  }
}


EXTERNC int minet_select_ex (int             minet_maxfd,
			     fd_set         *minet_read_fds,
			     fd_set         *minet_write_fds,
			     fd_set         *minet_except_fds,
			     int             unix_maxfd,
			     fd_set         *unix_read_fds,
			     fd_set         *unix_write_fds,
			     fd_set         *unix_except_fds,
			     struct timeval *timeout) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (select(minet_maxfd, minet_read_fds, minet_write_fds, 
		   minet_except_fds, timeout) ||
	    select(unix_maxfd, unix_read_fds, unix_write_fds,
		   unix_except_fds, timeout));
    break;
  case MINET_SOCKS:
    fd_set minet_read_fifos, minet_write_fifos, minet_except_fifos;
    FD_ZERO(&minet_read_fifos); 
    FD_ZERO(&minet_write_fifos); 
    FD_ZERO(&minet_except_fifos);
    for (int fd=0; fd<minet_maxfd+1; fd++)
      if(FD_ISSET(fd, minet_read_fds)) {
	fd = open(sock2app_fifo_name, O_RDONLY);
	FD_SET(fd, &minet_read_fifos);
      }
      else if(FD_ISSET(fd, minet_write_fds)) {
	fd = open(app2sock_fifo_name, O_WRONLY);
	FD_SET(fd, &minet_write_fifos);
      }
      else if(FD_ISSET(fd, minet_except_fds)) {
	fd = open(app2sock_fifo_name, O_WRONLY);
	FD_SET(fd, &minet_except_fifos);
      };
    int numfds = select(minet_maxfd, &minet_read_fifos, &minet_write_fifos, 
			&minet_except_fifos, timeout);
    SockLibRequestResponse slrr;
    if (numfds) {
      slrr = SockLibRequestResponse(mSELECT,
				    Connection(),
				    UNSPECIFIED_SOCK,
				    Buffer(),
				    0,
				    ESET_SELECT,
				    minet_read_fifos,
				    minet_write_fifos,
				    minet_except_fifos);
      
      slrr.Serialize (tosock);
      slrr.Unserialize (fromsock);
    } else {
      slrr = SockLibRequestResponse(mSELECT,
				    Connection(),
				    UNSPECIFIED_SOCK,
				    Buffer(),
				    0,
				    ECLEAR_SELECT,
				    minet_read_fifos,
				    minet_write_fifos,
				    minet_except_fifos);
      
      slrr.Serialize (tosock);
      slrr.Unserialize (fromsock);
    }
    int ctr=0;
    for(int j=0; j<minet_maxfd+1; j++)
      if(FD_ISSET(j, &minet_read_fifos)) ctr++;
      else if(FD_ISSET(j, &minet_write_fifos)) ctr++;
      else if(FD_ISSET(j, &minet_except_fifos)) ctr++;
    return (ctr ||
	    select(unix_maxfd, unix_read_fds, unix_write_fds,
		   unix_except_fds, timeout));
    break;
  }
}


EXTERNC int minet_poll (struct pollfd *minet_fds,
			int            num_minet_fds,
			int            timeout) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (poll (minet_fds, num_minet_fds, timeout));
    break;
  case MINET_SOCKS:
    struct pollfd *minet_fifos = (struct pollfd*)malloc(num_minet_fds * 
							sizeof(struct pollfd));
    for(int i=0; i<num_minet_fds; i++) {
      minet_fifos[i].fd = open(sock2app_fifo_name, O_RDWR);
      minet_fifos[i].events = minet_fds[i].events;
      minet_fifos[i].revents = minet_fds[i].revents;
    }
    int revnts = poll(minet_fifos, num_minet_fds, timeout);
    if(revnts) {
      SockLibRequestResponse slrr = SockLibRequestResponse(mPOLL,
							   Connection(),
							   UNSPECIFIED_SOCK,
							   Buffer(),
							   0, 0,
							   num_minet_fds,
							   *minet_fifos);
      slrr.Serialize (tosock);
      slrr.Unserialize (fromsock);
    } else {
      int ctr=0;
      for(int j=0; j<num_minet_fds; j++)
	if(minet_fds[j].revents) ctr++;
      return (ctr);
    }
    return (revnts);
    break;
  }
}
  

EXTERNC int minet_poll_ex (struct pollfd *minet_fds,
			   int            num_minet_fds,
			   struct pollfd *unix_fds,
			   int            num_unix_fds,
			   int            timeout) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    return (poll (minet_fds, num_minet_fds, timeout));
    break;
  case MINET_SOCKS:
    struct pollfd *minet_fifos = (struct pollfd*)malloc(num_minet_fds * 
							sizeof(struct pollfd));
    for(int i=0; i<num_minet_fds; i++) {
      minet_fifos[i].fd = open(sock2app_fifo_name, O_RDWR);
      minet_fifos[i].events = minet_fds[i].events;
      minet_fifos[i].revents = minet_fds[i].revents;
    }
    int revnts = poll(minet_fifos, num_minet_fds, timeout);
    if(revnts) {
      SockLibRequestResponse slrr = SockLibRequestResponse(mPOLL,
							   Connection(),
							   UNSPECIFIED_SOCK,
							   Buffer(),
							   0, 0,
							   num_minet_fds,
							   *minet_fifos);
      slrr.Serialize (tosock);
      slrr.Unserialize (fromsock);
    } else {
      int ctr=0;
      for(int j=0; j<num_minet_fds; j++)
	if(minet_fds[j].revents) ctr++;
      return (ctr ||
	      poll(unix_fds, num_unix_fds, timeout));
    }
    return (revnts ||
	    poll(unix_fds, num_unix_fds, timeout));
    break;
  }
}


EXTERNC int minet_set_nonblocking (int sockfd) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS: {
    int val = 1;
    int x = ioctl (sockfd, FIONBIO, &val);
    if (x)
      return (-1);
    return 0;
  }
    break;
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mSET_NONBLOCKING,
				Connection(),
				sockfd,
				Buffer(),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (0);
  }
}


EXTERNC int minet_set_blocking (int sockfd) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS: {
    int val = 0;
    int x = ioctl (sockfd, FIONBIO, &val);
    if (x)
      return (-1);
    return 0;
  }
    break;
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mSET_BLOCKING,
				Connection(),
				sockfd,
				Buffer(),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (0);
  }
}


EXTERNC int minet_can_write_now (int sockfd) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    fd_set *write_fds;
    FD_ZERO(write_fds);
    FD_SET(sockfd, write_fds);
    return (select(sockfd+1, NULL, write_fds, NULL, 0));
    break;
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mCAN_WRITE_NOW,
				Connection(),
				sockfd,
				Buffer(),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (0);
  }
}


EXTERNC int minet_can_read_now (int sockfd) {
  switch (socket_type) {
  case UNINIT_SOCKS:
    errno = ENODEV;            // "No such device" error
    return (-1);
    break;
  case KERNEL_SOCKS:
    fd_set *read_fds;
    FD_ZERO(read_fds);
    FD_SET(sockfd, read_fds);
    return (select(sockfd+1, read_fds, NULL, NULL, 0));
    break;
  case MINET_SOCKS:
    SockLibRequestResponse slrr(mCAN_READ_NOW,
				Connection(),
				sockfd,
				Buffer(),
				0, 0);
    slrr.Serialize (tosock);
    slrr.Unserialize (fromsock);
    minet_errno = slrr.error;
    if (minet_errno != EOK)
      return (-1);
    return (0);
  }
}
