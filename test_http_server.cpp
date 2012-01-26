#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "common.h"
#include "http_message.h"
#include "message_source_11.h"
#include "message_sink_11.h"
#include "message_source_gny.h"
#include "message_sink_gny.h"
#include "multiconn.h"
#include "server.h"
#include "http_server.h"

using namespace std;

struct server_data {
  string base_path;
};

int counter = 0;
class MyHttpServer : public EntityGenerator {
 public:
  MyHttpServer(http_message* in_msg, http_message* out_msg,
               server_data* data)
      : EntityGenerator(in_msg, out_msg), data(data), fd(-1) {
  }

  virtual ~MyHttpServer() {
    if(fd != -1) {
      close(fd);
    }
  }

  virtual void process() {
    string url = in_msg->get_url();
    size_t pos = url.find("?");
    if(pos != string::npos) url = url.substr(0, pos);

    if(!url.empty() && in_msg->get_state() > HTTP_MESSAGE_HEADER &&
       out_msg->get_state() == HTTP_MESSAGE_HEADER) {
      vector<pair<string, string> > headers;
      in_msg->get_headers(headers);
      for(int i = 0; i < headers.size(); i++) {
        for(int j = 0; j < headers[i].first.size(); j++) {
          headers[i].first[j] = tolower(headers[i].first[j]);
        }
        header_mp[headers[i].first] = headers[i].second;
      }
      out_msg->set_status(200);

      while(!url.empty() && url[url.size() - 1] == '/') {
        url.resize(url.size() - 1);
      }

      string entity;
      string content_type = "text/plain";
      char buf[PATH_MAX];
      char* canon_path = realpath((data->base_path + "/" + url).c_str(), buf);
      if(canon_path) {
        /* Make sure that the requested path is under the base path. */
        bool ok = true;
        for(int i = 0; ok && i < data->base_path.size(); i++) {
          ok = data->base_path[i] == canon_path[i];
        }
        if(!ok) {
          entity = "You do not have permission to access this resource.";
        } else {
          DIR* dir = opendir(canon_path);
          if(dir) {
            struct dirent* ent;
            while(ent = readdir(dir)) {
              if(ent->d_name[0] != '.') {
                content_type = "text/html";
                entity += string() + "<a href='http://" + header_mp["host"] +
                          url + "/" + ent->d_name + "'>" + ent->d_name +
                          "</a><br/>";
              }
            }
            closedir(dir);
          } else {
            fd = open(canon_path, O_RDONLY);
            if(fd == -1) {
              entity = "Could not find specified file or path";
            } else {
              content_type = mime_type(canon_path);
            }
          }
        }
      } else {
        entity = "Could not find specified file or path";
      }
      
      out_msg->add_header("Server", "Test Server");
      out_msg->add_header("Content-Type", content_type);
      out_msg->add_header("Cache-Control", "private");
      out_msg->end_header();
      if(fd == -1) {
        out_msg->append_entity(entity);
        out_msg->end_entity();
      }
    }
    while(out_msg->get_state() == HTTP_MESSAGE_ENTITY &&
          out_msg->peek_entity().size() < 1024) {
      ssize_t amt;
      char buf[1 << 16];
      amt = read(fd, buf, sizeof(buf));
      if(amt <= 0) {
        out_msg->end_entity();
        close(fd);
        fd = -1;
      } else {
        out_msg->append_entity(string(buf, amt));
      }
    }
    if(out_msg->get_state() == HTTP_MESSAGE_FOOTER) {
      out_msg->end_footer();
    }
    string edrain;
    vector<pair<string, string> > footer_drain;
    in_msg->get_entity(edrain);
    in_msg->get_footers(footer_drain);
  }
  
 private:
  int fd;
  server_data* data;
  map<string, string> header_mp;
};

EntityGenerator* my_http_server_gen_creator(http_message* in_msg,
                                            http_message* out_msg,
                                            void* data) {
  return new MyHttpServer(in_msg, out_msg, (server_data*)data);
}


int main(int argc, char** argv) {
  if(argc != 4) {
    cout << argv[0] << " (11|m11|gny|mgny) [PORT] [BASE_PATH]" << endl;
    return 1;
  }
  char buf[PATH_MAX];
  char* canon_path = realpath(argv[3], buf);
  if(!canon_path) {
    cout << "Error resolving base path" << endl;
    return 1;
  }

  struct server_data my_data;
  my_data.base_path = canon_path;

  conn_create_func_t conn_creator;
  void* init_data;
  
  struct multiconn_data multi_data;
  entity_linker_data link_data;
  if(!strcmp("11", argv[1])) {
    link_data.reader_creator = message_source_11_creator;
    link_data.writer_creator = message_sink_11_creator;
    link_data.entity_gen_creator = my_http_server_gen_creator;
    link_data.entity_gen_data = &my_data;

    conn_creator = entity_linker_conn_creator;
    init_data = &link_data;
  } else if(!strcmp("m11", argv[1])) {
    link_data.reader_creator = message_source_11_creator;
    link_data.writer_creator = message_sink_11_creator;
    link_data.entity_gen_creator = my_http_server_gen_creator;
    link_data.entity_gen_data = &my_data;

    multi_data.conn_creator = entity_linker_conn_creator;
    multi_data.init_data = &link_data;

    conn_creator = multiconn_conn_creator;
    init_data = &multi_data;
  } else if(!strcmp("gny", argv[1])) {
    link_data.reader_creator = message_source_gny_creator;
    link_data.writer_creator = message_sink_gny_creator;
    link_data.entity_gen_creator = my_http_server_gen_creator;
    link_data.entity_gen_data = &my_data;

    conn_creator = entity_linker_conn_creator;
    init_data = &link_data;
  } else if(!strcmp("mgny", argv[1])) {
    link_data.reader_creator = message_source_gny_creator;
    link_data.writer_creator = message_sink_gny_creator;
    link_data.entity_gen_creator = my_http_server_gen_creator;
    link_data.entity_gen_data = &my_data;

    multi_data.conn_creator = entity_linker_conn_creator;
    multi_data.init_data = &link_data;

    conn_creator = multiconn_conn_creator;
    init_data = &multi_data;
  } else {
    cout << "Could not understand protocol parameter" << endl;
    return 1;
  }

  Server svr(conn_creator, init_data);
  if(-1 == svr.bind_port(atoi(argv[2]))) {
    perror("svr.bind_port");
    return 1;
  }
  
  signal(SIGPIPE, SIG_IGN);
  struct timeval tv;
  do {
    tv.tv_sec = 0;
    tv.tv_usec = 10000;
  } while(svr.iterate(&tv) == 0);
  return 1;
}
