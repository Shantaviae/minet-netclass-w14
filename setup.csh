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


    mkfifo fifos/ether2mux_shim
    mkfifo fifos/mux2ether_shim
    
    mkfifo fifos/mux2arp_shim
    mkfifo fifos/arp2mux_shim
    
    mkfifo fifos/mux2ip_shim
    mkfifo fifos/ip2mux_shim
    
    mkfifo fifos/mux2other_shim
    mkfifo fifos/other2mux_shim
    
    mkfifo fifos/ip2arp_shim
    mkfifo fifos/arp2ip_shim
    
    mkfifo fifos/ip2ipmux_shim
    mkfifo fifos/ipmux2ip_shim
    
    mkfifo fifos/udp2ipmux_shim
    mkfifo fifos/ipmux2udp_shim

    mkfifo fifos/tcp2ipmux_shim
    mkfifo fifos/ipmux2tcp_shim

    mkfifo fifos/icmp2ipmux_shim
    mkfifo fifos/ipmux2icmp_shim

    mkfifo fifos/other2ipmux_shim
    mkfifo fifos/ipmux2other_shim

    mkfifo fifos/udp2sock_shim
    mkfifo fifos/sock2udp_shim

    mkfifo fifos/tcp2sock_shim
    mkfifo fifos/sock2tcp_shim

    mkfifo fifos/icmp2sock_shim
    mkfifo fifos/sock2icmp_shim

    mkfifo fifos/app2sock_shim
    mkfifo fifos/sock2app_shim

    echo "Done!"

echo "Setting MINET environment vars"
setenv MINET_IPADDR "10.10.10.11"
setenv MINET_ETHERNETDEVICE "eth0"
setenv MINET_ETHERNETADDR `./get_addr.pl $MINET_ETHERNETDEVICE`
setenv MINET_READER "/home/pdinda/netclass/rootexecs/reader"
setenv MINET_WRITER "/home/pdinda/netclass/rootexecs/writer"
setenv MINET_READERBUFFER "100"
setenv MINET_WRITERBUFFER "100"
setenv MINET_DEBUGLEVEL "0"
# log xterm gdb
setenv MINET_DISPLAY xterm
setenv MINET_SHIMS  "ip_mux+udp_module;udp_module+sock_module;ip_mux+tcp_module;tcp_module+sock_module"

setenv MINET_MSS 256
setenv MINET_MTU 500

echo "Done!"
