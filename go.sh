#!/bin/sh

rm -f pids
#./monitor.sh
./device_driver.sh 
./ethernet_mux.sh 
./arp_module.sh 
./ip_module.sh 
./other_module.sh 
./ip_mux.sh 
./shim_ipmux_udp.sh
./shim_ipmux_tcp.sh
./icmp_module.sh
./udp_module.sh 
./shim_udp_sock.sh
./tcp_module.sh 
./shim_tcp_sock.sh
./sock_module.sh 
case "foo$*" in
  foo) ./run_module.sh app;;
  foo?*) ./run_module.sh $* ;;
esac




