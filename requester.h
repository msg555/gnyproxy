#ifndef __REQUESTER_H
#define __REQUESTER_H

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <set>

#include "connection.h"
#include "client.h"
#include "http_message.h"

const int REQUESTER_NO_CONNECTION_LIMIT = 0x7FFFFFFF;

class ReqLinker;
class WrapperWrapper;
class Connector;

class RequestGenerator {
 public:
  RequestGenerator() {}
  virtual ~RequestGenerator() {}

  virtual void generate_request(http_message* msg) = 0;

  virtual void read_response(http_message* msg) = 0;
};

class Requester {
 friend class ReqLinker;
 friend class WrapperWrapper;
 friend class Connector;

 public:
  Requester(reader_create_func_t reader_creator,
            writer_create_func_t writer_creator,
            wrapper_create_func_t wrapper_creator,
            connection_maker_func_t connection_maker,
            int max_connections);
  ~Requester();

  bool add_request(const char* host, const char* service,
                   RequestGenerator* gen);

  /* Always establishes a new virtual connection to the endpoint. */
  bool add_connection(const char* host, const char* service,
                      http_message* connect_msg, Connection* cn);

  Client* get_client() { return &cli; }

 private:
  /* Note that it's important these maps are destructed after cli so that
   * WrapperWrapper can remove itself from conn_mp.
   */
  std::map<std::pair<std::string, std::string>,
                     std::set<WrapperWrapper*> > conn_mp;
  std::map<std::pair<std::string, std::string>,
                     std::set<ReqLinker*> > req_mp;
  Client cli;

  reader_create_func_t reader_creator;
  writer_create_func_t writer_creator;
  wrapper_create_func_t wrapper_creator;
  connection_maker_func_t connection_maker;
  int max_connections;
};

#endif
