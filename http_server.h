#ifndef __HTTP_SERVER_H
#define __HTTP_SERVER_H

#include <queue>
#include <utility>
#include <sys/types.h>
#include <sys/time.h>

#include "connection.h"

class http_message;

class EntityGenerator {
 public:
  EntityGenerator(http_message* in_msg, http_message* out_msg)
      : in_msg(in_msg), out_msg(out_msg) {}

  virtual ~EntityGenerator() {
    delete in_msg;
    delete out_msg;
  }

  virtual void process() = 0;

  bool is_done() {
    return in_msg->is_sent() && out_msg->is_sent();
  }

 protected:
  http_message* in_msg;
  http_message* out_msg;
};

typedef EntityGenerator*(*entity_gen_create_func_t)(http_message* in_msg,
                                                    http_message* out_msg,
                                                    void* data);

struct entity_linker_data {
  reader_create_func_t reader_creator;
  writer_create_func_t writer_creator;
  entity_gen_create_func_t entity_gen_creator;
  void* entity_gen_data;
};

extern
Connection* entity_linker_conn_creator(void* entity_linker_data_ptr);

class EntityLinker : public Connection {
 public:
  EntityLinker(reader_create_func_t reader_creator,
               writer_create_func_t writer_creator,
               entity_gen_create_func_t entity_gen_creator,
               void* entity_gen_data);
  virtual ~EntityLinker();

  virtual size_t get_write_size();

  virtual ssize_t write(int s, size_t max_write);

  virtual ssize_t read(int s, size_t max_read);
 
 private:
  ConnectionReader* reader;
  ConnectionWriter* writer;
  EntityGenerator* reader_processor;
  void* entity_gen_data;

  reader_create_func_t reader_creator;
  writer_create_func_t writer_creator;
  entity_gen_create_func_t entity_gen_creator;

  http_message* reader_msg;
  std::queue<std::pair<http_message*, EntityGenerator*> > writer_msgs;
};

#endif
