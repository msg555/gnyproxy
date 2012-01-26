#include "identity.h"

bool default_connector(Connection*, void*) {
  return false;
}

Connection* identity_wrap_creator(void* init_data,
                                  conn_create_func_t conn_creator) {
  return new Identity(init_data, conn_creator);
}

