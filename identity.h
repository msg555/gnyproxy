#ifndef __IDENTITY_H
#define __IDENTITY_H

#include "connection.h"

class Identity : public Connection {
 public:
  Identity(void* init_data, conn_create_func_t conn_creator) {
    conn = conn_creator(init_data);
  }

  explicit Identity(Connection* conn) : conn(conn) {
  }

  virtual ~Identity() {
    delete conn;
  }

  virtual size_t get_write_size() {
    return conn->get_write_size();
  }

  virtual ssize_t write(int s, size_t max_write) {
    return conn->write(s, max_write);
  }

  virtual ssize_t read(int s, size_t max_read) {
    return conn->read(s, max_read);
  }
 
 private:
  Connection* conn;
};

extern
bool default_connector(Connection*, void*);

extern
Connection* identity_wrap_creator(void* init_data,
                                  conn_create_func_t conn_creator);

#endif
