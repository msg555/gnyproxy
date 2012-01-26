#include "multiconn.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <queue>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

Connection* multiconn_conn_creator(void* multiconn_data_ptr) {
  multiconn_data* data = (multiconn_data*)multiconn_data_ptr;
  return multiconn_wrap_creator(data->init_data, data->conn_creator);
}

Connection* multiconn_wrap_creator(void* init_data,
                                   conn_create_func_t conn_creator) {
  return new Multiconn(init_data, conn_creator);
}

bool multiconn_connector(Connection* conn, void* data) {
  Multiconn* multiconn = (Multiconn*)conn;
  return multiconn->create_virtual_connection(data);
}

inline
bool can_read(int s) {
  struct timeval zerowait;
  zerowait.tv_sec = zerowait.tv_usec = 0;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(s, &fds);
  return select(s + 1, &fds, NULL, NULL, &zerowait) == 1;
}

inline
bool can_write(int s) {
  struct timeval zerowait;
  zerowait.tv_sec = zerowait.tv_usec = 0;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(s, &fds);
  return select(s + 1, NULL, &fds, NULL, &zerowait) == 1;
}

Multiconn::Multiconn(void* init_data, conn_create_func_t conn_creator)
    : init_data(init_data), conn_creator(conn_creator),
      r_id(0), r_sz(0), r_state(0),
      w_id(0), w_sz(0), w_state(0)  {
  memset(vconn, 0, sizeof(vconn));
  memset(is_queued, 0, sizeof(is_queued));
}

Multiconn::~Multiconn() {
  for(int i = 0; i <= MULTICONN_MAX_ID; i++) {
    if(vconn[i]) {
      delete vconn[i];
    }
  }
}
 
size_t Multiconn::get_write_size() {
  if(!w_sz && !w_queue.empty()) {
    w_id = w_queue.front();
    w_queue.pop();
    w_sz = min(MULTICONN_MAX_SZ,
               get_virtual_connection(w_id)->get_write_size());
    if(!w_sz) {
      fprintf(stderr, "Warning, virtual connection lowered write size "
                      "unexpectedly\n");
    }
    w_state = 0;
  }
  return w_sz ? w_sz + 3 - w_state : 0;
}

ssize_t Multiconn::read(int s, size_t max_read) {
  ssize_t amt = 0;
  ssize_t read = 0;
  if(r_state == 0) {
    unsigned char id;
    amt = recv(s, &id, 1, 0);
    if(amt <= 0) return amt;
    r_id = id;
    read += amt; r_state += amt; max_read -= amt;
    if(!max_read || !can_read(s)) return read;
  }
  if(r_state < 3) {
    amt = recv(s, (char *)&r_sz + r_state - 1, min(max_read, 3 - r_state), 0);
    if(amt <= 0) return amt;
    read += amt; r_state += amt; max_read -= amt;
    if(r_state == 3) r_sz = r_sz ? ntohs(*(uint16_t *)&r_sz) : MULTICONN_MAX_SZ;
    if(!max_read || !can_read(s)) return read;
  }
  amt = get_virtual_connection(r_id)->read(s, min(r_sz, max_read));
  
  if(amt <= 0) return amt;
  if(r_sz < amt) {
    fprintf(stderr, "multiconn::write: client read too many bytes\n");
  }
  r_sz -= amt;
  if(!is_queued[r_id] && get_virtual_connection(r_id)->get_write_size()) {
    w_queue.push(r_id);
    is_queued[r_id] = true;
  }
  if(!r_sz) {
    r_id = r_sz = r_state = 0;
  }
  return amt + read;
}

ssize_t Multiconn::write(int s, size_t max_write) {
  ssize_t amt = 0;
  ssize_t wrote = 0;
  if(!w_sz) {
    fprintf(stderr, "Unexpected call to write\n");
    return -1;
  }
  if(w_state == 0) {
    unsigned char id = w_id;
    amt = send(s, &id, 1, 0);
    if(amt <= 0) return amt;
    wrote += amt; w_state += amt; max_write -= amt;
    if(!max_write || !can_write(s)) return wrote;
  }
  if(w_state < 3) {
    unsigned short sz = w_sz == MULTICONN_MAX_SZ ? 0 : htons(w_sz);
    amt = send(s, (char *)&sz + w_state - 1, min(max_write, 3 - w_state), 0);
    if(amt <= 0) return amt;
    wrote += amt; w_state += amt; max_write -= amt;
    if(!max_write || !can_write(s)) return wrote;
  }
  amt = get_virtual_connection(w_id)->write(s, min(w_sz, max_write));
  if(amt <= 0) return amt;
  if(w_sz < amt) {
    fprintf(stderr, "multiconn::write: client wrote too many bytes\n");
  }
  w_sz -= amt;
  if(!w_sz) {
    if(get_virtual_connection(w_id)->get_write_size()) {
      w_queue.push(w_id);
    } else {
      is_queued[w_id] = false;
    }
    w_id = w_sz = w_state = 0;
  }
  return wrote + amt;
}

Connection* Multiconn::get_virtual_connection(int id) {
  if(!vconn[id]) {
    vconn[id] = conn_creator(init_data);
  }
  return vconn[id];
}

bool Multiconn::add_virtual_connection(Connection* vc) {
  for(int i = 0; i <= MULTICONN_MAX_ID; i++) {
    if(!vconn[i]) {
      vconn[i] = vc;
      if(vc->get_write_size()) {
        is_queued[i] = true;
        w_queue.push(i);
      }
      return true;
    }
  }
  delete vc;
  return false;
}

bool Multiconn::create_virtual_connection(void* data) {
  for(int i = 0; i <= MULTICONN_MAX_ID; i++) {
    if(!vconn[i]) {
      vconn[i] = conn_creator(data);
      if(vconn[i]->get_write_size()) {
        is_queued[i] = true;
        w_queue.push(i);
      }
      return true;
    }
  }
  return false;
}

void Multiconn::update() {
  for(int i = 0; i <= MULTICONN_MAX_ID; i++) {
    if(!is_queued[i] && vconn[i] && vconn[i]->get_write_size()) {
      is_queued[i] = true;
      w_queue.push(i);
    }
  }
}
