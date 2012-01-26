#include "message_sink_gny.h"

#include <sys/socket.h>

#include "common.h"

using namespace std;

ConnectionWriter* message_sink_gny_creator(void* http_message_ptr) {
  return new message_sink_gny((http_message*)http_message_ptr);
}

message_sink_gny::message_sink_gny(http_message* message)
               : http_msg_ptr(message), write_state(HTTP_MESSAGE_HEADER),
                 data_stream(""), init(false) {
}

size_t message_sink_gny::get_write_size(){
  check_for_update();
  return data_stream.size();
}

ssize_t message_sink_gny::write(int socket, size_t max_write){
  size_t to_write = min(get_write_size(), max_write);
  ssize_t amount_sent = send(socket, data_stream.data(), to_write, 0);
  if (amount_sent > 0) {
    data_stream = data_stream.substr(amount_sent);
  }
  return amount_sent;
}

//checks if http_msg_ptr has more information available
//  if so, we add it to data_stream
void message_sink_gny::check_for_update(){
  assert(write_state <= http_msg_ptr->get_state());

  if(write_state == HTTP_MESSAGE_HEADER) {
    if(!init) {
      if(http_msg_ptr->get_is_request()) {
        http_request_verb_e verb = http_msg_ptr->get_verb();
        const string& url = http_msg_ptr->get_url();
        if(url.empty() || verb == HTTP_REQUEST_VERB_UNKNOWN) {
          return;
        }
        data_stream += (char)verb;
        write_string(data_stream, url);
      } else {
        short status = http_msg_ptr->get_status();
        if(status == -1) {
          return;
        }
        write_short(data_stream, status);
      }
      init = true;
    }
    
    vector<pair<string, string> > header_vector;
    http_msg_ptr->get_headers(header_vector);
    for(int i = 0; i < header_vector.size(); i++) {
      add_key_value_pair(header_vector[i].first, header_vector[i].second);
    }
    if(http_msg_ptr->get_state() != HTTP_MESSAGE_HEADER) {
      data_stream += (char)HEADER_END;
      write_state = HTTP_MESSAGE_ENTITY;
    }
  }
  
  if(write_state == HTTP_MESSAGE_ENTITY){
    string more_entity;
    http_msg_ptr->get_entity(more_entity);
    for(int i = 0; i < more_entity.size(); ) {
      int sz = min((int)more_entity.size() - i, (1 << 16) - 1);
      write_short(data_stream, sz);
      data_stream += more_entity.substr(i, sz);
      i += sz;
    }
    if(http_msg_ptr->get_state() != HTTP_MESSAGE_ENTITY) {
      write_state = HTTP_MESSAGE_FOOTER;
      write_short(data_stream, 0);
    }
  }

  if(write_state == HTTP_MESSAGE_FOOTER){
    vector<pair<string, string> > footer_vector;
    http_msg_ptr->get_footers(footer_vector);

    for(int i = 0; i < footer_vector.size(); i++) {
      add_key_value_pair(footer_vector[i].first, footer_vector[i].second);
    }
    if(http_msg_ptr->get_state() != HTTP_MESSAGE_FOOTER) {
      data_stream += (char)HEADER_END;
      write_state = HTTP_MESSAGE_DONE;
    }
  }
}

void message_sink_gny::add_key_value_pair(const string& key,
                                          const string& value){
  enum http_headers_e red_key = atoheader(key.c_str());
  data_stream += char(red_key);
  if(red_key == HEADER_UNKNOWN) {
    write_string(data_stream, key);
  }
  write_string(data_stream, value);
}
