#! /bin/bash

case "$MINET_SHIMS" in
  *ip_mux+udp_module*) ./run_module.sh shim_ipmux_udp ;;
  *) ;;
esac
