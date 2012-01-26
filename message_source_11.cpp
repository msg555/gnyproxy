#include "message_source_11.h"

#include <iostream>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <errno.h>
#include <string>
#include <vector>
#include <utility>
#include <sys/socket.h>

#include "common.h"
#include "http.h"

using namespace std;

#include <stdio.h>
ConnectionReader* message_source_11_creator(void* http_message_ptr) {
  return new message_source_11((http_message*)http_message_ptr);
}

#define FAIL_MESSAGE(msg, fail_text) cout << "FAILED: " << fail_text << endl;

enum {
  FIRSTLINE,
  HEADERS,
  ENTITY,
  DONE,
};

enum {
  ENTITY_CHUNKED = -2,
  ENTITY_UNKNOWN = -1,
  // >= 0 fixed size entity with that many bytes.
};

string get_token(const string& line, int& pos) {
  while(pos < line.size() && isspace(line[pos])) pos++;
  int beg = pos;
  while(pos < line.size() && !isspace(line[pos])) pos++;
  return line.substr(beg, pos - beg);
}

void message_source_11::parse_request_line(const string& line) {
  int pos = 0;
  msg->set_verb(atoverb(get_token(line, pos).c_str()));
  msg->set_url(url_decode(get_token(line, pos)));
  string protocol = get_token(line, pos);
  if(protocol != "HTTP/1.0" && protocol != "HTTP/1.1") {
    FAIL_MESSAGE(msg, "Unrecognized protocol");
  }
}

void message_source_11::parse_response_line(const string& line) {
  int pos = 0;
  string protocol = get_token(line, pos);
  if(protocol != "HTTP/1.0" && protocol != "HTTP/1.1") {
    FAIL_MESSAGE(msg, "Unrecognized protocol");
  }
  string response_code = get_token(line, pos);
  string response_str = get_token(line, pos);
  errno = 0;
  int code = strtol(response_code.c_str(), NULL, 10);
  if(errno == ERANGE) {
    FAIL_MESSAGE(msg, "Invalid response code");
  }
  msg->set_status(code);
}

void message_source_11::parse_header(const string& line) {
  int colon = line.find(':');
  if(colon == string::npos) {
    FAIL_MESSAGE(msg, "Invalid header");
    return;
  }
  string key = trim(line.substr(0, colon));
  string value = trim(line.substr(colon + 1));
  
  if(!strcasecmp(key.c_str(), "Transfer-Encoding")) {
    if(!strcasecmp(value.c_str(), "chunked")) {
      entity_mode = ENTITY_CHUNKED;
    } else {
      FAIL_MESSAGE(msg, "Unknown transfer encoding");
    }
  } else if(entity_mode == ENTITY_UNKNOWN &&
            !strcasecmp(key.c_str(), "Content-Length")) {
    errno = 0;
    entity_mode = strtoll(value.c_str(), NULL, 10);
    if(errno == ERANGE) {
      FAIL_MESSAGE(msg, "Invalid content length");
    }
    msg->add_header(key, value);
  } else if(!strcasecmp(key.c_str(), "Connection") ||
            !strcasecmp(key.c_str(), "Proxy-Connection") ||
            !strcasecmp(key.c_str(), "Keep-Alive") ||
            !strcasecmp(key.c_str(), "TE") ||
            !strcasecmp(key.c_str(), "Trailer")) {
    // We will ignore these headers
  } else {
    msg->add_header(key, value);
  }
}

void message_source_11::parse_footer(const string& line) {
  int colon = line.find(':');
  if(colon == string::npos) {
    FAIL_MESSAGE(msg, "Invalid header");
    return;
  }
  string key = trim(line.substr(0, colon));
  string value = trim(line.substr(colon + 1));
  msg->add_footer(key, value);
}

message_source_11::message_source_11(http_message* msg)
    : msg(msg), non_entity_bytes(0), last(0), state(FIRSTLINE),
      entity_mode(ENTITY_UNKNOWN) {
}

ssize_t message_source_11::read(int s, size_t max_read) {
  char rbuf[1024];
  ssize_t amt = recv(s, rbuf, min(sizeof(rbuf), max_read), MSG_PEEK);
  if(amt <= 0) return amt;
  buf += string(rbuf, amt);

  int i;
  int pos = 0;
  string entity;
  for(i = buf.size() - amt; i < buf.size() && state != DONE; i++) {
    char ch = buf[i];
    switch(state) {
      case FIRSTLINE:
      case HEADERS:
        if(ch == '\n') {
          string line = buf.substr(pos, i - pos - (last == '\r' ? 1 : 0));
          if(state == FIRSTLINE) {
            if(!line.empty()) {
              if(msg->get_is_request()) {
                parse_request_line(line);
              } else {
                parse_response_line(line);
              }
              state = HEADERS;
            }
          } else if(!line.empty()) {
            parse_header(line);
          } else {
            msg->end_header();
            if(entity_mode == 0 || entity_mode == ENTITY_UNKNOWN) {
              msg->end_entity();
              msg->end_footer();
              state = DONE;
            } else {
              state = ENTITY;
              entity_state1 = 0;
              entity_state2 = 0;
            }
          }
          pos = i + 1;
        }
        break;
      case ENTITY: {
        bool move_pos = true;
        if(entity_mode > 0) {
          entity += ch;
          if(--entity_mode == 0) {
            state = DONE;
            msg->append_entity(entity);
            entity.clear();
            msg->end_entity();
            msg->end_footer();
          }
        } else {
          if(entity_state1 == 0) {
            if(is_hex(ch)) {
              entity_state2 = entity_state2 << 4;
              entity_state2 |= hextoi(ch);
            } else if(ch == ';') {
              entity_state1 = 1;
            } else if(ch == '\n') {
              if(entity_state2) {
                entity_state1 = 2;
              } else {
                entity_state1 = 3;
                msg->append_entity(entity);
                entity.clear();
                msg->end_entity();
              }
            }
          } else if(entity_state1 == 1) {
            if(ch == '\n') {
              if(entity_state2) {
                entity_state1 = 2;
              } else {
                entity_state1 = 3;
                msg->append_entity(entity);
                entity.clear();
                msg->end_entity();
              }
            }
          } else if(entity_state1 == 2) {
            if(entity_state2) {
              entity += ch;
              entity_state2--;
            } else if(ch == '\n') {
              entity_state1 = 0;
            }
          } else if(entity_state1 == 3) {
            if(ch == '\n') {
              string line = buf.substr(pos, i - pos - (last == '\r' ? 1 : 0));
              if(line.empty()) {
                state = DONE;
                msg->end_footer();
              } else {
                parse_footer(line);
              }
            } else {
              move_pos = false;
            }
          }
        }
        if(move_pos) pos = i + 1;
        break;
      }
    }
    last = ch;
  }
  if(!entity.empty()) {
    msg->append_entity(entity);
  }
  /* Clear the bytes from the stream that were actually part of this
   * connection.
   */
  amt -= buf.size() - i;
  assert(amt == recv(s, rbuf, amt, 0));
  /* Clear data that we no longer need to keep around.  Note that this is ok to
   * do as a single byte will only be copied at most once making this run in
   * amortized linear time.
   */
  if(pos) {
    buf = buf.substr(pos);
  }
  return amt;
}
