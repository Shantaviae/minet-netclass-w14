#include "route.h"


#define	NET		0
#define GATEWAY		1
#define MASK		2
#define FLAGS		3
#define METRIC		4
#define REF		5
#define USE		6
#define INTERFACE	7

// Initializing a route table
route_table_t *make_route_table(void) 
{
  route_table_t *route_q = (route_table_t *)malloc(sizeof(route_table_t));
  route_q->first = NULL;
  route_q->last = NULL;
  
  return route_q;
}



// Making a route
route_t *make_route(char *net, char *mask, char *interface, char *gateway, \
		    char *flags, int metric, int ref, int use) 
{
  route_t *route_entry = (route_t *)malloc(sizeof(route_t));

  route_entry->net = net;
  route_entry->mask = mask;
  route_entry->interface = interface;
  route_entry->gateway = gateway;
  route_entry->flags = flags;
  route_entry->metric = metric;
  route_entry->ref = ref;
  route_entry->use = use;
  route_entry->previous = NULL;
  route_entry->next = NULL;
 
  return route_entry;
}



// Initial loading of route table from a given file
void load_routes(route_table_t *table, const char *filename) 
{
  FILE *f;
  char  temp[256];
  int   type = 0;
  int   not_entry = 1;

  if((f = fopen(filename, "r")) == NULL)
    cout << filename << " cannot be opened" << endl;

  fseek(f, 0, SEEK_SET);
  route_t *route = (route_t *)malloc(sizeof(route_t));

  while(!feof(f)) {
    if(not_entry) {
      fscanf(f, "%s", &temp);
      if((strchr(temp, '.') != NULL) || (strcmp(temp, "default") == 0)) {
        not_entry = 0;
        route->net = temp;
        type++;
      }
    }

    switch(type) {			// Once the pointer gets into the table
      case NET: {
        fscanf(f, "%s", &(route->net));
      } break;
      case GATEWAY: {
        fscanf(f, "%s", &(route->gateway));
      } break;
      case MASK: {
        fscanf(f, "%s", &(route->mask));
      } break;
      case FLAGS: {
        fscanf(f, "%s", &(route->flags));
      } break;
      case METRIC: {
        fscanf(f, "%d", &(route->metric));
      } break;
      case REF: {
        fscanf(f, "%d", &(route->ref));
      } break;
      case USE: {
        fscanf(f, "%d", &(route->use));
      } break;
      case INTERFACE: {
        fscanf(f, "%s", &(route->interface));
      } break;
      default: {}
    }

    type++;
    if(type == 8) {  			// Finish reading one row
      type = 0;
      add_route(table, route);
    }
  }
  
  free(route); 
  fclose(f);
  return;
}  
  


// Checking if a route_table is empty
// if empty return 1
// if not empty return 0
int is_empty(route_table_t *table) 
{
  if(table->first == NULL)
    return 1;

  return 0;
}



// Adding a route to the route table
void add_route(route_table_t *table, route_t *new_route) 
{
  if(is_empty(table)) {
    table->first = new_route;
    table->last = new_route;
  }
  else {
    table->last->next = new_route;
    new_route->previous = table->last;
    table->last = new_route;
  }
}

  

// Looking for a match
route_t *match_route(route_table_t *route, const char *net_addr) 
{
}



// Deleting an entry from the route table 
void del_route(route_table_t *table, const char *net_addr)
{
  route_t *current = (route_t *)malloc(sizeof(route_t));
  
  current = table->first;
  
  while(current != NULL) {
    if(strcmp(net_addr, current->net) == 0) {
      if(current == table->first) {
	table->first = table->first->next;
        table->first->previous = NULL;
      }
      else if (current == table->last) {
        table->last = table->last->previous;
        table->last->next = NULL;
      }
      else {
        current->previous->next = current->next;
	current->next->previous = current->previous;
      }
      free(current);
      return;
    }
    
    current = current->next;
  }
  cout << "Network address " << net_addr << " not in the routing table" << endl;
  return;
}
          
