#ifndef __CLIENT_H
#define __CLIENT_H

#include <utility>
#include <vector>

#include "connection.h"

class Client {
 public:
  Client(conn_create_func_t conn_creator);
  ~Client();

  Connection* add_socket(int s, void* init_data);

  Connection* connect(const char* host, const char* service, void* init_data);

  int iterate(struct timeval* timeout);

 private:
  conn_create_func_t conn_creator;
  std::vector<std::pair<int, Connection*> > conns;
};

#endif
