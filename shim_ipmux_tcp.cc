#include "config.h"
#include "shim.h"
#include "sockint.h"
#include <string>

MAKESHIM(Packet,Packet);

int main()
{
  return RunShim("tcp_module",
		 "ip_mux",
		 ipmux2tcp_fifo_name,
		 tcp2ipmux_fifo_name,
		 (string(tcp2ipmux_fifo_name)+string("_shim")).c_str(),
		 (string(ipmux2tcp_fifo_name)+string("_shim")).c_str());
}
