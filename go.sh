#!/bin/sh

rm -f pids
./monitor.sh
./device_driver.sh 
./ethernet_mux.sh 
./arp_module.sh 
./ip_module.sh 
./other_module.sh 
./ip_mux.sh 
./icmp_module.sh
./udp_module.sh 
./tcp_module.sh 
./ipother_module.sh
./sock_module.sh 
case "foo$*" in
  foo) ./run_module.sh app;;
  foo?*) ./run_module.sh $* ;;
esac




