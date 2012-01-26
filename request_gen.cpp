#include <iostream>
#include <iomanip>
#include <vector>
#include <fstream>
#include <string>
#include <utility>
#include <string.h>
#include <sys/time.h>

#include "connection.h"
#include "client.h"
#include "http.h"
#include "http_message.h"
#include "message_source_11.h"
#include "message_source_gny.h"
#include "message_sink_11.h"
#include "message_sink_gny.h"
#include "multiconn.h"
#include "requester.h"
#include "identity.h"

using namespace std;

int active = 0;
int processed = 0;

class MyRequester : public RequestGenerator {
 public:
  MyRequester(const string& url) : url(url), marked(false) {
    active++;
  }

  virtual ~MyRequester() {
    if(!marked) {
      active--;
      processed++;
    }
  }

  virtual void generate_request(http_message* msg) {
    if(msg->is_sent()) {
      return;
    }
    msg->set_url(url);
    msg->set_verb(HTTP_REQUEST_GET);
    msg->add_header("User-Agent", "My Agent!");
    msg->end_header();
    msg->end_entity();
    msg->end_footer();
  }

  virtual void read_response(http_message* msg) {
    string entity;
    vector<pair<string, string> > headers;
    msg->get_headers(headers);
    msg->get_footers(headers);
    msg->get_entity(entity);
  }

 private:
  bool marked;
  string url;
};

int main(int argc, char** argv) {
  if(argc != 5) {
    cout << argv[0] << " PROTOCOL DEST_HOST DEST_PORT MAX_CONNECTIONS" << endl;
    return 1;
  }
  reader_create_func_t reader_creator;
  writer_create_func_t writer_creator;
  wrapper_create_func_t wrapper_creator;
  connection_maker_func_t connection_maker;
  int max_connections = atoi(argv[4]);

  if(!strcmp("11", argv[1])) {
    reader_creator = message_source_11_creator;
    writer_creator = message_sink_11_creator;
    wrapper_creator = identity_wrap_creator;
    connection_maker = default_connector;
  } else if(!strcmp("gny", argv[1])) {
    reader_creator = message_source_gny_creator;
    writer_creator = message_sink_gny_creator;
    wrapper_creator = identity_wrap_creator;
    connection_maker = default_connector;
  } else if(!strcmp("m11", argv[1])) {
    reader_creator = message_source_11_creator;
    writer_creator = message_sink_11_creator;
    wrapper_creator = multiconn_wrap_creator;
    connection_maker = multiconn_connector;
  } else if(!strcmp("mgny", argv[1])) {
    reader_creator = message_source_gny_creator;
    writer_creator = message_sink_gny_creator;
    wrapper_creator = multiconn_wrap_creator;
    connection_maker = multiconn_connector;
  } else {
    cout << "Unrecognized protocol" << endl;
    return 1;
  }

  struct timeval start;
  gettimeofday(&start, NULL);

  Requester requester(reader_creator, writer_creator, wrapper_creator,
                      connection_maker, max_connections);

  string url;
  while(active || cin) {
    cout << "\rActive: " << setw(4) << active <<
            " Processed: " << setw(4) << processed << flush;
    while(!url.empty() || cin) {
      if(!url.empty()) {
        MyRequester* my_req = new MyRequester(url);
        if(!requester.add_request(argv[2], argv[3], my_req)) {
          delete my_req;
          processed--;
          break;
        }
      }
      url.clear();
      getline(cin, url);
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;
    if(requester.get_client()->iterate(&timeout)) {
      break;
    }
  }

  struct timeval endt;
  gettimeofday(&endt, NULL);
  cout << "\nTime expired: " <<
          (endt.tv_sec * 1000 + endt.tv_usec / 1000) -
          (start.tv_sec * 1000 + start.tv_usec / 1000) << " ms" << endl;

  cout << "Exiting" << endl;
  return 0;
}
