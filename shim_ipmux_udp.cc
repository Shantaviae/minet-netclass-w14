#include "config.h"
#include "shim.h"
#include "sockint.h"
#include <string>

MAKESHIM(Packet,Packet);

int main()
{
  return RunShim("udp_module",
		 "ip_mux",
		 ipmux2udp_fifo_name,
		 udp2ipmux_fifo_name,
		 (string(udp2ipmux_fifo_name)+string("_shim")).c_str(),
		 (string(ipmux2udp_fifo_name)+string("_shim")).c_str());
}
