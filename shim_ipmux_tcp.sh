#! /bin/bash

case "$MINET_SHIMS" in
  *ip_mux+tcp_module*) ./run_module.sh shim_ipmux_tcp ;;
  *) ;;
esac
