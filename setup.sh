#! /bin/bash


if [ -e fifos ]; then
    rm -rf fifos
fi

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
export MINET_IPADDR="10.10.10.11"
export MINET_ETHERNETDEVICE="eth0"
export MINET_ETHERNETADDR=`./get_addr.pl $MINET_ETHERNET_DEVICE`
export MINET_READER="/home/Minet/execs/reader"
export MINET_WRITER="/home/Minet/execs/writer"
export MINET_READERBUFFER="100"
export MINET_WRITERBUFFER="100"
export MINET_DEBUGLEVEL="0"
# log xterm gdb
export MINET_DISPLAY=xterm
#export MINET_SHIMS="ip_mux+udp_module;udp_module+sock_module;ip_mux+tcp_module;tcp_module+sock_module"
export MINET_MONITOR=icmp
export MINET_MSS=256
export MINET_MTU=500

echo "Done!"
