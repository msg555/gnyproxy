#include "message_sink_11.h"

#include <iostream>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <algorithm>
#include <sys/socket.h>

#include "common.h"

using namespace std;

ConnectionWriter* message_sink_11_creator(void* http_message_ptr) {
  return new message_sink_11((http_message*)http_message_ptr);
}

enum {
  ENTITY_CHUNKED = -2,
  ENTITY_UNKNOWN = -1,
  // >= 0 fixed size entity with that many bytes.
};

message_sink_11::message_sink_11(http_message* ptr) :
            http_msg_ptr(ptr), is_init(false),
            write_state(HTTP_MESSAGE_HEADER), entity_mode(ENTITY_UNKNOWN) {}

size_t message_sink_11::get_write_size() {
    check_for_update();
    return packet_str.size();
}

ssize_t message_sink_11::write(int sock, size_t max_write) {
    size_t to_write = get_write_size();
    to_write = min(to_write, max_write);

    ssize_t amount_sent = send(sock, packet_str.data(), to_write, 0);

    if(amount_sent > 0) {
        packet_str = packet_str.substr(amount_sent);
    }
    return amount_sent;
}

void message_sink_11::check_for_update() {
    if (write_state == HTTP_MESSAGE_HEADER){
        if(!is_init) {
            if(http_msg_ptr->get_is_request()) {
              if (http_msg_ptr->get_url().empty() ||
                  http_msg_ptr->get_verb() == HTTP_REQUEST_VERB_UNKNOWN) {
                  return;
              }
              packet_str += (verbtoa(http_msg_ptr->get_verb()));
              packet_str += ' ';
              packet_str += url_encode(http_msg_ptr->get_url());
              packet_str += " HTTP/1.1\r\n";
            } else if(http_msg_ptr->get_status() != -1) {
              char numbuf[20];
              sprintf(numbuf, "%d", http_msg_ptr->get_status());
              packet_str += "HTTP/1.1 ";
              packet_str += numbuf;
              /* Need code to convert status code to status text. */
              packet_str += " FIXME\r\n";
            } else {
              return;
            }
            is_init = true;
        }
        vector<key_value_t> headers;
        http_msg_ptr->get_headers(headers);
        for(vector<key_value_t>::const_iterator itr = headers.begin();
                itr != headers.end(); ++itr) {
            if(!strcasecmp(itr->first.c_str(), "Transfer-Encoding")) {
                if(!strcasecmp(itr->second.c_str(), "chunked")) {
                    entity_mode = ENTITY_CHUNKED;
                } else {
                    cerr << "message_sink_11::check_for_update: " <<
                            "unknown transfer encoding " << itr->second << endl;
                }
            } else if(entity_mode == ENTITY_UNKNOWN &&
                      !strcasecmp(itr->first.c_str(), "Content-Length")) {
                errno = 0;
                entity_mode = strtoll(itr->second.c_str(), NULL, 10);
                if(errno == ERANGE) {
                    entity_mode = ENTITY_UNKNOWN;
                    cerr << "message_sink_11::check_for_update: " <<
                            "failed to parse content length: " <<
                            itr->second << endl;
                }
            }
            packet_str += itr->first;
            packet_str += ": ";
            packet_str += itr->second;
            packet_str += "\r\n"; 
        }
        if(http_msg_ptr->get_state() != HTTP_MESSAGE_HEADER) {
          if(entity_mode != ENTITY_UNKNOWN) {
            packet_str += "\r\n";
          }
          write_state = HTTP_MESSAGE_ENTITY;
        }
    }
    if (write_state == HTTP_MESSAGE_ENTITY){
        string entity;
        http_msg_ptr->get_entity(entity);
        if(!entity.empty()) {
            /* Default to chunked transfer encoding if no content length or
             * transfer encoding given and an entity exists.
             */
            if(entity_mode == ENTITY_UNKNOWN) {
              packet_str += "Transfer-Encoding: chunked\r\n\r\n";
              entity_mode = ENTITY_CHUNKED;
            }
            if(entity_mode == ENTITY_CHUNKED) {
              char numbuf[20];
              sprintf(numbuf, "%x", entity.size());
              packet_str += numbuf;
              packet_str += "\r\n";
              packet_str += entity;
              packet_str += "\r\n";
            } else if(entity_mode >= 0) {
              if(entity.size() > entity_mode) {
                cerr << "Warning: entity size larger than advertised" << endl;
                entity_mode = 0;
              } else {
                entity_mode -= entity.size();
              }
              packet_str += entity;
            }
        }
        if(http_msg_ptr->get_state() != HTTP_MESSAGE_ENTITY) {
          if(entity_mode == ENTITY_UNKNOWN) {
            packet_str += "\r\n";
          } else if(entity_mode == ENTITY_CHUNKED) {
            packet_str += "0\r\n";
          }
          write_state = HTTP_MESSAGE_FOOTER;
        }
    }
    if (write_state == HTTP_MESSAGE_FOOTER){
        vector<key_value_t> footers;
        http_msg_ptr->get_footers(footers);
        if(!footers.empty()) {
            if(entity_mode != ENTITY_CHUNKED) {
                cerr << "message_sink_11::check_for_update: " <<
                        "cannot send footers" << endl;

            } else {
                for(int i = 0; i < footers.size(); i++) {
                    packet_str += footers[i].first;
                    packet_str += ": ";
                    packet_str += footers[i].second;
                    packet_str += "\r\n";
                }
            }
        }
        if(http_msg_ptr->get_state() == HTTP_MESSAGE_DONE) {
            if(entity_mode == ENTITY_CHUNKED) {
                packet_str += "\r\n";
            }
            write_state = HTTP_MESSAGE_DONE;
        }
    }
}

