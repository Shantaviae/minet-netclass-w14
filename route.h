#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct route_t 
{
  char*   net;
  char*   mask;
  char*   interface;
  char*   gateway;
  char*   flags;
  int     metric;
  int     ref;
  int     use;
  route_t *previous;
  route_t *next;
};

struct route_table_t 
{
  route_t *first;
  route_t *last;
};

route_table_t *make_route_table(void);
route_t	      *make_route(char *net, char *mask, char *interface, char *gateway, \
			  char *flags, int metric, int ref, int use);
void	       load_routes(route_table_t *table, const char *filename);
void           add_route(route_table_t *table, route_t *new_route);
void           del_route(route_table_t *table, const char *net_addr);
route_t       *match_route(route_table_t *route, const char *net_addr);
int            is_empty(route_table_t *table);

