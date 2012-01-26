#include <iostream>
#include <vector>
#include <cstring>

#include "multiconn.h"
#include "identity.h"
#include "http_proxy.h"
#include "message_source_11.h"
#include "message_sink_11.h"
#include "message_source_gny.h"
#include "message_sink_gny.h"

using namespace std;

int main(int argc, char** argv) {
  if(argc != 4 && argc != 6) {
    cout << "Usage:" << endl;
    cout << argv[0] << " [SOURCE PROTOCOL] [DEST PROTOCOL] [PORT]" << endl;
    cout << argv[0] << " [SOURCE PROTOCOL] [DEST PROTOCOL] [PORT]" <<
                       " [FORWARD HOST] [FORWARD PORT]" << endl;
    cout << endl;
    cout << "  PROTOCOL = 11|m11|gny|mgny" << endl;
    return 1;
  }
  reader_create_func_t a_reader_creator;
  writer_create_func_t a_writer_creator;
  wrapper_create_func_t a_wrapper;
  reader_create_func_t b_reader_creator;
  writer_create_func_t b_writer_creator;
  wrapper_create_func_t b_wrapper;
  connection_maker_func_t connector;

  if(!strcmp(argv[1], "11")) {
    a_reader_creator = message_source_11_creator;
    a_writer_creator = message_sink_11_creator;
    a_wrapper = identity_wrap_creator;
  } else if(!strcmp(argv[1], "m11")) {
    a_reader_creator = message_source_11_creator;
    a_writer_creator = message_sink_11_creator;
    a_wrapper = multiconn_wrap_creator;
  } else if(!strcmp(argv[1], "gny")) {
    a_reader_creator = message_source_gny_creator;
    a_writer_creator = message_sink_gny_creator;
    a_wrapper = identity_wrap_creator;
  } else if(!strcmp(argv[1], "mgny")) {
    a_reader_creator = message_source_gny_creator;
    a_writer_creator = message_sink_gny_creator;
    a_wrapper = multiconn_wrap_creator;
  } else {
    cout << "Unrecognized client protocol" << endl;
    return 1;
  }

  if(!strcmp(argv[2], "11")) {
    b_reader_creator = message_source_11_creator;
    b_writer_creator = message_sink_11_creator;
    b_wrapper = identity_wrap_creator;
    connector = default_connector;
  } else if(!strcmp(argv[2], "m11")) {
    b_reader_creator = message_source_11_creator;
    b_writer_creator = message_sink_11_creator;
    b_wrapper = multiconn_wrap_creator;
    connector = multiconn_connector;
  } else if(!strcmp(argv[2], "gny")) {
    b_reader_creator = message_source_gny_creator;
    b_writer_creator = message_sink_gny_creator;
    b_wrapper = identity_wrap_creator;
    connector = default_connector;
  } else if(!strcmp(argv[2], "mgny")) {
    b_reader_creator = message_source_gny_creator;
    b_writer_creator = message_sink_gny_creator;
    b_wrapper = multiconn_wrap_creator;
    connector = multiconn_connector;
  } else {
    cout << "Unrecognized client protocol" << endl;
    return 1;
  }

  HttpProxy proxy(a_reader_creator, a_writer_creator, a_wrapper,
                  b_reader_creator, b_writer_creator, b_wrapper,
                  connector, argc == 6 ? argv[4] : NULL,
                  argc == 6 ? argv[5] : NULL);
  proxy.get_server()->bind_port(atoi(argv[3]));

  struct timeval tv;
  while(1) {
    tv.tv_sec = 0; tv.tv_usec = 0;
    if(proxy.get_server()->iterate(&tv)) break;
    tv.tv_sec = 0; tv.tv_usec = 0;
    if(proxy.get_client()->iterate(&tv)) break;
    tv.tv_sec = 0; tv.tv_usec = 10;
    select(0, NULL, NULL, NULL, &tv);
  }
  return 1;
}

