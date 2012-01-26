#include "http_proxy.h"

#include <string.h>
#include <vector>
#include <string>
#include <utility>
#include <queue>
#include <sys/types.h>
#include <sys/socket.h>

#include "common.h"

using namespace std;

static void generate_message(http_message* msg, int status,
                             const string& info) {
  msg->set_status(status);
  msg->add_header("Proxy-Agent", "gny-proxy");
  msg->end_header();
  msg->append_entity(info);
  msg->end_entity();
  msg->end_footer();
}

class MessageCopier : public RequestGenerator {
 public:
  MessageCopier(http_message* msg_req, http_message* msg_res)
    : msg_req(msg_req), msg_res(msg_res) {
  }

  virtual ~MessageCopier() {
  }

  virtual void generate_request(http_message* msg) {
    dump_message(msg_req, msg);
  }

  virtual void read_response(http_message* msg) {
    dump_message(msg, msg_res);
  }

 private:
  void dump_message(http_message* src, http_message* dst) {
    vector<pair<string, string> > headers;
    if(dst->get_is_request()) {
      dst->set_verb(src->get_verb());
      if(dst->get_url().empty()) {
        dst->set_url(src->get_url());
      }
    } else {
      dst->set_status(src->get_status());
    }
    if(dst->get_state() == HTTP_MESSAGE_HEADER) {
      src->get_headers(headers);
      for(int i = 0; i < headers.size(); i++) {
        dst->add_header(headers[i].first, headers[i].second);
      }
      if(src->get_state() > HTTP_MESSAGE_HEADER) {
        dst->end_header();
      }
    }
    if(dst->get_state() == HTTP_MESSAGE_ENTITY) {
      string entity;
      src->get_entity(entity);
      dst->append_entity(entity);
      if(src->get_state() > HTTP_MESSAGE_ENTITY) {
        dst->end_entity();
      }
    }
    if(dst->get_state() == HTTP_MESSAGE_FOOTER) {
      src->get_footers(headers);
      for(int i = 0; i < headers.size(); i++) {
        dst->add_footer(headers[i].first, headers[i].second);
      }
      if(src->get_state() > HTTP_MESSAGE_FOOTER) {
        dst->end_footer();
      }
    }
  }

  http_message* msg_req;
  http_message* msg_res;
};

class Tunnel : public Connection {
 public:
  Tunnel(string* data_req, string* data_res, int* linked_count)
    : data_req(data_req), data_res(data_res), linked_count(linked_count) {
  }

  virtual ~Tunnel() {
    if(--(*linked_count) == 0) {
      delete linked_count;
    }
  }

  virtual size_t get_write_size() {
    if(*linked_count != 2) {
      return 0;
    }
    return data_req->size();
  }

  virtual ssize_t write(int s, size_t max_write) {
    if(*linked_count != 2) {
      return 0;
    }
    ssize_t amt = send(s, data_req->data(),
                       min(max_write, data_req->size()), 0);
    if(amt > 0) {
      *data_req = data_req->substr(amt);
    }
    return amt;
  }

  virtual ssize_t read(int s, size_t max_read) {
    if(*linked_count != 2) {
      return 0;
    }
    char buf[1024];
    ssize_t amt = recv(s, buf, min(sizeof(buf), max_read), 0);
    if(amt > 0) {
      *data_res += string(buf, amt);
    }
    return amt;
  }

  void stop_tunnel() {
    data_req = data_res = NULL;
  }

 private:
  string* data_req;
  string* data_res;
  int* linked_count;
};

class ALinker : public Connection {
 public:
  ALinker(HttpProxy* proxy)
    : proxy(proxy), connected(false), writer(NULL), tunneler(NULL),
      linked_count(NULL) {
    reader_msg = new http_message(true);
    reader = proxy->a_reader_creator(reader_msg);
  }

  virtual ~ALinker() {
    if(linked_count && --(*linked_count) == 0) {
      delete linked_count;
    }
  }

  virtual size_t get_write_size() {
    if(response_queue.empty()) {
      return tunneler ? data_res.size() : 0;
    }

    /* Get the reqeust and response messages for this transaction. */
    http_message* msg_req = response_queue.front().first;
    http_message* msg_res = response_queue.front().second;

    /* Create a writer for the current message if it doesn't already exist. */
    if(!writer) {
      writer = proxy->a_writer_creator(msg_res);
    }

    /* Figure out how many bytes the writer has to write. */
    size_t ret = writer->get_write_size();

    /* If there are no bytes to write and both the request and response are
       in a completed state then move on to the next transaction. */
    if(ret == 0 && (tunneler || msg_req->is_sent()) && msg_res->is_sent()) {
      delete writer;
      delete msg_req;
      delete msg_res;
      response_queue.pop();
      writer = NULL;
      return get_write_size();
    }

    return ret;
  }

  virtual ssize_t write(int s, size_t max_write) {
    if(writer == NULL) {
      /* Tunnel mode is active.  Send back data as received. */
      assert(tunneler != NULL);
      ssize_t amt = send(s, data_res.data(),
                         min(max_write, data_res.size()), 0);
      if(amt > 0) {
        data_res = data_res.substr(amt);
      }
      return amt;
    }
    return writer->write(s, max_write);
  }

  virtual ssize_t read(int s, size_t max_read) {
    if(reader == NULL) {
      /* We are in tunnel mode.  Just send out data exactly as received. */
      assert(tunneler != NULL);
      char buf[1024];
      ssize_t amt = recv(s, buf, min(sizeof(buf), max_read), 0);
      if(amt > 0) {
        data_req += string(buf, amt);
      }
      return amt;
    }

    ssize_t ret = reader->read(s, max_read);

    if(!connected && reader_msg->get_verb() != HTTP_REQUEST_VERB_UNKNOWN &&
                     !reader_msg->get_url().empty()) {
      /* Figure out where the request wants us to connect to. */
      pair<string, string> endpoint;
      if(proxy->get_forward_host()) {
        endpoint = make_pair(proxy->get_forward_host(),
                             proxy->get_forward_service());
      } else if(reader_msg->get_verb() == HTTP_REQUEST_CONNECT) {
        endpoint = parse_host(reader_msg->get_url());
      } else {
        const vector<pair<string, string> >& headers =
            reader_msg->peek_headers();
        for(int i = 0; i < headers.size(); i++) {
          if(!strcasecmp("host", headers[i].first.c_str())) {
            endpoint = parse_host(headers[i].second);
            break;
          }
        }
      }
      /* Patch the connection through to the remote endpoint if found. */
      if(!endpoint.first.empty()) {
        if(reader_msg->get_verb() != HTTP_REQUEST_CONNECT) {
          /* This is a normal http request. */
          http_message* msg_res = new http_message(false);
          MessageCopier* copier = new MessageCopier(reader_msg, msg_res);
          if(!proxy->req.add_request(endpoint.first.c_str(),
                                     endpoint.second.c_str(), copier)) {
            delete copier;
            generate_message(msg_res, 504,
                             "Failed to connect to upstream server");
          }
          append_response(reader_msg, msg_res);
          connected = true;
        } else {
          /* The client is asking us to create a tunnel to the endpoint. */
          linked_count = new int;
          *linked_count = 2;
          tunneler = new Tunnel(&data_req, &data_res, linked_count);

          /* Try to connect to the remote server or upstream proxy.  Note that
           * if there is an upstream proxy we need to forward the CONNECT
           * message up to it.
           */
          if(!proxy->req.add_connection(endpoint.first.c_str(),
                             endpoint.second.c_str(),
                             proxy->get_forward_host() ? reader_msg : NULL,
                             tunneler)) {
            /* We failed to connect to the next hop.  Clean up and report the
             * error back to the client.
             */
            delete tunneler;
            delete linked_count;
            tunneler = NULL;
            linked_count = NULL;
            http_message* msg_res = new http_message(false);
            generate_message(msg_res, 504,
                             "Failed to connect to upstream server");
            append_response(reader_msg, msg_res);
          } else if(!proxy->get_forward_host()) {
            /* If there is no upstream proxy we need to send back to the client
             * that the tunnel has been completed to the remote server.
             */
            http_message* msg_res = new http_message(false);
            generate_message(msg_res, 200, "");
            append_response(reader_msg, msg_res);
          }
          connected = true;
        }
      }
    }

    /* When the request finishes we need to move on to the next request. */
    if(reader_msg->get_state() == HTTP_MESSAGE_DONE) {
      if(!connected) {
        http_message* msg_res = new http_message(false);
        generate_message(msg_res, 400, "no host header found");
        append_response(reader_msg, msg_res);
      }
      delete reader;
      connected = false;
      if(tunneler == NULL) {
        reader_msg = new http_message(true);
        reader = proxy->a_reader_creator(reader_msg);
      } else {
        reader = NULL;
        reader_msg = NULL;
      }
    }
    return ret;
  }

  void append_response(http_message* msg_req, http_message* msg_res) {
    response_queue.push(make_pair(msg_req, msg_res));
  }

 private:
  HttpProxy* proxy;

  ConnectionReader* reader;
  ConnectionWriter* writer;
  http_message* reader_msg;
  bool connected;

  Tunnel* tunneler;
  string data_req;
  string data_res;
  int* linked_count;

  queue<pair<http_message*, http_message*> > response_queue;
};

static Connection* linker_creator(void* http_proxy_ptr) {
  return new ALinker((HttpProxy*)http_proxy_ptr);
}

static Connection* create_wrapper(void* create_wrapper_data_ptr) {
  create_wrapper_data* data = (create_wrapper_data*)create_wrapper_data_ptr;
  return data->wrapper_creator(data->proxy, linker_creator);
}

HttpProxy::HttpProxy(reader_create_func_t a_reader_creator,
                     writer_create_func_t a_writer_creator,
                     wrapper_create_func_t a_wrapper,
                     reader_create_func_t b_reader_creator,
                     writer_create_func_t b_writer_creator,
                     wrapper_create_func_t b_wrapper,
                     connection_maker_func_t connection_maker,
                     const char* forward_host, const char* forward_service)
  : a_reader_creator(a_reader_creator), a_writer_creator(a_writer_creator),
    forward_host(forward_host), forward_service(forward_service),
    svr(create_wrapper, &wrap_data),
    req(b_reader_creator, b_writer_creator, b_wrapper, connection_maker,
        REQUESTER_NO_CONNECTION_LIMIT) {
  wrap_data.wrapper_creator = a_wrapper;
  wrap_data.proxy = this;
}

HttpProxy::~HttpProxy() {
}
