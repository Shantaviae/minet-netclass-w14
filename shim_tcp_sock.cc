#include "config.h"
#include "shim.h"
#include "sockint.h"
#include <string>

MAKESHIM(SockRequestResponse,SockRequestResponse)

int main()
{
  return RunShim("sock_module",
		 "tcp_module",
		 tcp2sock_fifo_name,
		 sock2tcp_fifo_name,
		 (string(sock2tcp_fifo_name)+string("_shim")).c_str(),
		 (string(tcp2sock_fifo_name)+string("_shim")).c_str());
}
