#ifndef __SERVER_H
#define __SERVER_H

#include <vector>

#include "connection.h"

using namespace std;

class Server {
 public:
  Server(conn_create_func_t conn_creator, void* init_data);
  ~Server();

  void set_socket(int s) { this->s = s; }

  int bind_port(int port);

  int iterate(struct timeval* timeout);

 private:
  void accept_connection();

  int s;
  conn_create_func_t conn_creator;
  void* init_data;
  vector<pair<int, Connection*> > conns;
};

#endif
