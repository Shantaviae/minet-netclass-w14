#! /bin/csh


if ( -e fifos ) then
    rm -rf fifos
endif

    echo "Making fifos for communication between stack components in ./fifos"
    mkdir fifos
    
    mkfifo fifos/ether2mux
    mkfifo fifos/mux2ether
    
    mkfifo fifos/mux2arp
    mkfifo fifos/arp2mux
    
    mkfifo fifos/mux2ip
    mkfifo fifos/ip2mux
    
    mkfifo fifos/mux2other
    mkfifo fifos/other2mux
    
    mkfifo fifos/ip2arp
    mkfifo fifos/arp2ip
    
    mkfifo fifos/ip2ipmux
    mkfifo fifos/ipmux2ip
    
    mkfifo fifos/udp2ipmux
    mkfifo fifos/ipmux2udp

    mkfifo fifos/tcp2ipmux
    mkfifo fifos/ipmux2tcp

    mkfifo fifos/icmp2ipmux
    mkfifo fifos/ipmux2icmp

    mkfifo fifos/other2ipmux
    mkfifo fifos/ipmux2other

    mkfifo fifos/udp2sock
    mkfifo fifos/sock2udp

    mkfifo fifos/tcp2sock
    mkfifo fifos/sock2tcp

    mkfifo fifos/icmp2sock
    mkfifo fifos/sock2icmp

    mkfifo fifos/app2sock
    mkfifo fifos/sock2app

    mkfifo fifos/reader2mon
    mkfifo fifos/writer2mon
    mkfifo fifos/ether2mon
    mkfifo fifos/ethermux2mon
    mkfifo fifos/arp2mon
    mkfifo fifos/ip2mon
    mkfifo fifos/other2mon
    mkfifo fifos/ipmux2mon
    mkfifo fifos/udp2mon
    mkfifo fifos/tcp2mon
    mkfifo fifos/icmp2mon
    mkfifo fifos/sock2mon
    mkfifo fifos/socklib2mon
    mkfifo fifos/app2mon

    echo "Done!"

echo "Setting MINET environment vars"
setenv MINET_IPADDR "10.10.1.5"
setenv MINET_ETHERNETDEVICE "eth0"
setenv MINET_ETHERNETADDR `./get_addr.pl $MINET_ETHERNETDEVICE`
setenv MINET_READER "/home1/pdinda/netclass-execs/reader"
setenv MINET_WRITER "/home1/pdinda/netclass-execs/writer"
setenv MINET_READERBUFFER "100"
setenv MINET_WRITERBUFFER "100"
setenv MINET_DEBUGLEVEL "0"
# log xterm gdb
setenv MINET_DISPLAY xterm
# setenv MINET_SHIMS  "ip_mux+udp_module;udp_module+sock_module;ip_mux+tcp_module;tcp_module+sock_module"

setenv MINET_MSS 256
setenv MINET_MTU 500

echo "Done!"
