#include "message_source_gny.h"

#include <cstring>
#include <string>
#include <vector>
#include <sys/socket.h>

#include "common.h"

using namespace std;

enum {
  INITVERB,
  INITURL,
  INITSTATUS,
  HEADERSTART,
  HEADERKEY,
  HEADERVALUE,
  ENTITYSTART,
  ENTITY,
  DONE,
};

ConnectionReader* message_source_gny_creator(void* http_message_ptr) {
  return new message_source_gny((http_message*)http_message_ptr);
}

message_source_gny::message_source_gny(http_message* http_msg_ptr)
  : http_msg_ptr(http_msg_ptr), data_stream(""), substate(0), pos(0),
    key(HEADER_UNKNOWN) {
  state = http_msg_ptr->get_is_request() ? INITVERB : INITSTATUS;
}

message_source_gny::~message_source_gny() {
}

ssize_t message_source_gny::read(int s, size_t max_read) {
  char rbuf[MAX_READ_SIZE];
  ssize_t recv_amt = recv(s, rbuf, min(max_read, MAX_READ_SIZE), MSG_PEEK);

  if(recv_amt <= 0) {
    return recv_amt;
  }
  data_stream += string(rbuf, recv_amt);

  int i;
  for(i = data_stream.size() - recv_amt; state != DONE &&
                                         i < data_stream.size(); i++) {
    char ch = data_stream[i];
    switch(state) {
      case INITVERB:
        http_msg_ptr->set_verb((enum http_request_verb_e)(unsigned char)ch);
        state = INITURL;
        pos = i + 1;
        break;
      case INITURL:
        if(!ch) {
          http_msg_ptr->set_url(data_stream.data() + pos);
          state = HEADERSTART;
          pos = i + 1;
        } 
        break;
      case INITSTATUS:
        if(pos + 1 == i) {
          uint16_t status;
          memcpy(&status, data_stream.data() + pos, 2);
          status = ntohs(status);
          http_msg_ptr->set_status(status);
          state = HEADERSTART;
          pos = i + 1;
        }
        break;
      case HEADERSTART:
        key = (enum http_headers_e)(unsigned char)ch;
        if(key == HEADER_END) {
          if(http_msg_ptr->get_state() == HTTP_MESSAGE_HEADER) {
            http_msg_ptr->end_header();
            state = ENTITYSTART;
          } else {
            http_msg_ptr->end_footer();
            state = DONE;
          }
        } else if(key == HEADER_UNKNOWN) {
          state = HEADERKEY;
        } else {
          state = HEADERVALUE;
        }
        pos = i + 1;
        break;
      case HEADERKEY:
        if(!ch) {
          state = HEADERVALUE;
          substate = i + 1;
        }
        break;
      case HEADERVALUE:
        if(!ch) {
          if(key == HEADER_UNKNOWN) {
            if(http_msg_ptr->get_state() == HTTP_MESSAGE_HEADER) {
              http_msg_ptr->add_header(data_stream.data() + pos,
                                       data_stream.data() + substate);
            } else {
              http_msg_ptr->add_footer(data_stream.data() + pos,
                                       data_stream.data() + substate);
            }
          } else {
            if(http_msg_ptr->get_state() == HTTP_MESSAGE_HEADER) {
              http_msg_ptr->add_header(headertoa(key),
                                       data_stream.data() + pos);
            } else {
              http_msg_ptr->add_footer(headertoa(key),
                                       data_stream.data() + pos);
            }
          }
          pos = i + 1;
          state = HEADERSTART;
        }
        break;
      case ENTITYSTART:
        if(pos + 1 == i) {
          uint16_t size;
          memcpy(&size, data_stream.data() + pos, 2);
          size = ntohs(size);
          substate = size;
          pos = i + 1;
          if(size) {
            state = ENTITY;
          } else {
            state = HEADERSTART;
            http_msg_ptr->end_entity();
          }
        }
        break;
      case ENTITY:
        if(!--substate) {
          http_msg_ptr->append_entity(data_stream.substr(pos, i - pos + 1));
          pos = i + 1;
          state = ENTITYSTART;
        }
        break;
    }
  }
  if(state == ENTITY) {
    http_msg_ptr->append_entity(data_stream.substr(pos));
    pos = data_stream.size();
  }

  recv_amt -= data_stream.size() - i;
  assert(recv_amt == recv(s, rbuf, recv_amt, 0));

  if(pos) {
    data_stream = data_stream.substr(pos);
    pos = 0;
  }
  return recv_amt;
}

