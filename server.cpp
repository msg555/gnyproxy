#include "server.h"

#include <iostream>
#include <cstdio>
#include <cstring>
#include <utility>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

static const int INFINITY = 0x7FFFFFFF;

Server::Server(conn_create_func_t conn_creator, void* init_data)
    : conn_creator(conn_creator), init_data(init_data), s(-1) {
}

Server::~Server() {
  if(s != -1) {
    close(s);
  }
  for(int i = 0; i < conns.size(); i++) {
    close(conns[i].first);
    delete conns[i].second;
  }
}

int Server::bind_port(int port) {
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  s = socket(AF_INET, SOCK_STREAM, 0);
  if(s == -1) {
    return -1;
  }
  int optval = 1;
  if(-1 == setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) ||
     -1 == bind(s, (struct sockaddr*)&addr, sizeof(addr)) ||
     -1 == listen(s, 1024)) {
    return -1;
  }
  return 0;
}

int Server::iterate(struct timeval* timeout) {
  int nfds = s + 1;
  // Initialize file descriptor masks.
  fd_set read_fds;
  fd_set write_fds;
  fd_set excep_fds;
  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  // Add the listening socket to the read file descriptor list.
  // A listening socket is given a read event when a connection comes in.
  FD_SET(s, &read_fds);
  // Add all existing connection's socket if needed.
  for(int i = 0; i < conns.size(); i++) {
    conns[i].second->update();
    nfds = max(nfds, conns[i].first + 1);
    FD_SET(conns[i].first, &read_fds);
    if(conns[i].second->get_write_size()) {
      nfds = max(nfds, conns[i].first + 1);
      FD_SET(conns[i].first, &write_fds);
    }
  }
  // Wait for at least one of our file descriptors to be ready.
  if(-1 == select(nfds, &read_fds, &write_fds, NULL, timeout)) {
    return -1;
  }
  // If we have a new connection on s accept it.
  if(FD_ISSET(s, &read_fds)) {
    accept_connection();
  }
  // Allow connections to receive and send data.
  for(int i = 0; i < conns.size(); i++) {
    if(FD_ISSET(conns[i].first, &read_fds) &&
       conns[i].second->read(conns[i].first, INFINITY) <= 0 ||
       FD_ISSET(conns[i].first, &write_fds) &&
       conns[i].second->write(conns[i].first, INFINITY) <= 0) {
      close(conns[i].first);
      delete conns[i].second;
      swap(conns[i], conns.back());
      conns.resize(conns.size() - 1);
      i--;
    }
  }
  return 0;
}

int xconns = 0;
void Server::accept_connection() {
  int s_acp = accept(s, NULL, NULL);
  if(s_acp == -1) {
    perror("accept_connection");
  } else {
    conns.push_back(make_pair(s_acp, conn_creator(init_data)));
  }
}
