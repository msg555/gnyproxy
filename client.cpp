#include "client.h"

#include <cstring>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

using namespace std;

static const int INFINITY = 0x7FFFFFFF;

Client::Client(conn_create_func_t conn_creator)
    : conn_creator(conn_creator) {
}

Client::~Client() {
  for(int i = 0; i < conns.size(); i++) {
    close(conns[i].first);
    delete conns[i].second;
  }
}

Connection* Client::add_socket(int s, void* init_data) {
  Connection* ret = conn_creator(init_data);
  conns.push_back(make_pair(s, ret));
  return ret;
}

Connection* Client::connect(const char* host, const char* service,
                            void* init_data) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if(s == -1) {
    return NULL;
  }

  /* Connect to remote endpoint. */
  struct addrinfo hints;
  struct addrinfo* res;
  struct addrinfo* rp;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;
  if(getaddrinfo(host, service, &hints, &res)) {
    close(s);
    return NULL;
  }
  for(rp = res; rp; rp = rp->ai_next) {
    if(-1 != ::connect(s, rp->ai_addr, rp->ai_addrlen)) {
      break;
    }
  }
  freeaddrinfo(res);
  if(!rp) {
    close(s);
    return NULL;
  }
  return add_socket(s, init_data);
} 

int Client::iterate(struct timeval* timeout) {
  int nfds = 0;
  // Initialize file descriptor masks.
  fd_set read_fds;
  fd_set write_fds;
  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);

  // Add all existing connection's socket if needed.
  for(int i = 0; i < conns.size(); i++) {
    conns[i].second->update();
    nfds = max(nfds, conns[i].first + 1);
    FD_SET(conns[i].first, &read_fds);
    if(conns[i].second->get_write_size()) {
      FD_SET(conns[i].first, &write_fds);
    }
  }
  // Wait for at least one of our file descriptors to be ready.
  if(-1 == select(nfds, &read_fds, &write_fds, NULL, timeout)) {
    return -1;
  }
  // Allow connections to receive and send data.
  for(int i = 0; i < conns.size(); i++) {
    if(FD_ISSET(conns[i].first, &read_fds) &&
       conns[i].second->read(conns[i].first, INFINITY) <= 0 ||
       FD_ISSET(conns[i].first, &write_fds) &&
       conns[i].second->write(conns[i].first, INFINITY) <= 0) {
      close(conns[i].first);
      delete conns[i].second;
      conns[i] = conns.back();
      conns.resize(conns.size() - 1);
      i--;
    }
  }
  return 0;
}
