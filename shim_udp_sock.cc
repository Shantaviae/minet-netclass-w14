#include "config.h"
#include "shim.h"
#include "sockint.h"
#include <string>

MAKESHIM(SockRequestResponse,SockRequestResponse);

int main()
{
  return RunShim("sock_module",
		 "udp_module",
		 udp2sock_fifo_name,
		 sock2udp_fifo_name,
		 (string(sock2udp_fifo_name)+string("_shim")).c_str(),
		 (string(udp2sock_fifo_name)+string("_shim")).c_str());
}
