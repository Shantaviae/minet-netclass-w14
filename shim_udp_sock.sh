#! /bin/bash

case "$MINET_SHIMS" in
  *udp_module+sock_module*) ./run_module.sh shim_udp_sock ;;
  *) ;;
esac
