LIBMINET_SOCK = libminet_socket.a

LIBMINET = libminet.a

LIBMINET_SOCK_OBJS = minet_socket.o

LIBMINET_OBJS  = \
           Minet.o                              \
           Monitor.o                            \
           config.o 				\
           buffer.o 				\
           error.o 				\
           ethernet.o 				\
           headertrailer.o 			\
           packet.o 				\
           packet_queue.o 			\
           raw_ethernet_packet.o 		\
           raw_ethernet_packet_buffer.o  	\
           util.o                               \
           debug.o                              \
           arp.o                                \
           ip.o                                 \
           icmp.o                               \
           udp.o                                \
           tcp.o                                \
           sockint.o                            \
	   sock_mod_structs.o                   \
           constate.o				\
           tcpstate.o                           \
	   route.o                              \
           bitsource.o 

EXECOBJS_EXCEPT_READER_WRITER =                 \
           monitor.o                            \
           device_driver.o                      \
           ethernet_mux.o			\
           arp_module.o                         \
           ip_module_diffusion.o		\
           ip_module_routing.o                  \
           ip_module.o                          \
           other_module.o                       \
           ip_mux.o                             \
           icmp_module.o                        \
           udp_module.o                         \
           tcp_module.o                         \
           ipother_module.o                     \
           app.o				\
           sock_module.o                        \
           udp_client.o                         \
           udp_server.o                         \
           sock_test_tcp.o                      \
           sock_test_app.o                      \
           tcp_client.o                         \
           tcp_server.o                         \
           sock_test_app.o                      \
           sock_test_tcp.o                      \
           test_reader.o                        \
           test_writer.o                        \
           test_arp.o                           \
           test_raw_ethernet_packet_buffer.o    \
           icmp_app.o                           \
           test_bitsource.o                     \
           http_client.o                        \
           http_server1.o                       \
           http_server2.o                       \
           http_server3.o                       \


READER_WRITER_EXECOBJS=reader.o writer.o

EXECOBJS = $(EXECOBJS_EXCEPT_READER_WRITER) $(READER_WRITER_EXECOBJS)

OBJS     = $(LIBMINET_OBJS) $(LIBMINET_SOCK_OBJS) $(EXECOBJS)

LIBNETCFLAGS  = `libnet-config --defines`
LIBNETLDFLAGS = `libnet-config --libs`

PCAPCFLAGS  = -I/usr/include/pcap
PCAPLDFLAGS = -lpcap

CXX=g++ 
CC=gcc 
AR=ar
RANLAB=ranlib

CXXFLAGS = -g -ggdb -gstabs+ 
READERCXXFLAGS = -g $(PCAPCFLAGS)  
WRITERCXXFLAGS = -g $(CXXFLAGS) $(LIBNETCFLAGS)
LDFLAGS= $(LIBMINET) $(LIBMINET_SOCK) $(LIBMINET) 
READERLDFLAGS= $(LIBMINET) $(PCAPLDFLAGS)
WRITERLDFLAGS= $(LIBMINET) $(LIBNETLDFLAGS) 

all:  all_except_rw

all_all: all_except_rw reader_writer

all_except_rw: $(LIBMINET) $(LIBMINET_SOCK) $(EXECOBJS_EXCEPT_READER_WRITER:.o=)

reader_writer: $(READER_WRITER_EXECOBJS:.o=)

$(LIBMINET) : $(LIBMINET_OBJS)
	$(AR) ruv $(LIBMINET) $(LIBMINET_OBJS)

$(LIBMINET_SOCK) : $(LIBMINET_SOCK_OBJS)
	$(AR) ruv $(LIBMINET_SOCK) $(LIBMINET_SOCK_OBJS)

% : %.o $(LIBMINET) $(LIBMINET_SOCK)
	$(CXX) $< $(LDFLAGS) -o $(@F)

reader: reader.o $(LIBMINET)
	$(CXX)  reader.o $(READERLDFLAGS)  -o reader
	-./fixup.sh

writer: writer.o $(LIBMINET)
	$(CXX) writer.o $(WRITERLDFLAGS) -o writer
	-./fixup.sh

%.o : %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $(@F)

device_driver.o : device_driver.cc
	$(CXX) $(READERCXXFLAGS) $(WRITERCXXFLAGS) -c $< -o $(@F)

ethernet.o : ethernet.cc
	$(CXX) $(READERCXXFLAGS) $(WRITERCXXFLAGS) -c $< -o $(@F)

reader.o : reader.cc
	$(CXX) $(READERCXXFLAGS) -c $< -o $(@F)

writer.o : writer.cc
	$(CXX) $(WRITERCXXFLAGS) -c $< -o $(@F)

depend:
	$(CXX) $(CXXFLAGS) $(READERCXXFLAGS) $(WRITERCXXFLAGS)  -MM $(OBJS:.o=.cc) > .dependencies

clean: 
	rm -f $(OBJS) $(EXECOBJS:.o=) $(LIBMINET) $(LIBMINET_SOCK) 

include .dependencies



