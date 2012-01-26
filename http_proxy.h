#ifndef __HTTP_PROXY_H
#define __HTTP_PROXY_H

#include <map>
#include <sys/select.h>

#include "connection.h"
#include "client.h"
#include "http_message.h"
#include "requester.h"
#include "server.h"

class ALinker;
class HttpProxy;

struct create_wrapper_data {
  wrapper_create_func_t wrapper_creator;
  HttpProxy* proxy;
};

class HttpProxy {
  friend class ALinker;

 public:
  HttpProxy(reader_create_func_t a_reader_creator,
            writer_create_func_t a_writer_creator,
            wrapper_create_func_t a_wrapper,
            reader_create_func_t b_reader_creator,
            writer_create_func_t b_writer_creator,
            wrapper_create_func_t b_wrapper,
            connection_maker_func_t connection_maker,
            const char* forward_host, const char* forward_service);
  ~HttpProxy();

  Server* get_server() { return &svr; }

  Client* get_client() { return req.get_client(); }
  
  const char* get_forward_host() const { return forward_host; }
  
  const char* get_forward_service() const { return forward_service; }

 private:
  reader_create_func_t a_reader_creator;
  writer_create_func_t a_writer_creator;
  create_wrapper_data wrap_data;

  Server svr;
  Requester req;

  const char* forward_host;
  const char* forward_service;
};

#endif
