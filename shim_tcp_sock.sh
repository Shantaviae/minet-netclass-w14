#! /bin/bash

case "$MINET_SHIMS" in
  *tcp_module+sock_module*) ./run_module.sh shim_tcp_sock ;;
  *) ;;
esac
