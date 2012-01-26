#include "requester.h"

#include <assert.h>
#include <iostream>
#include <map>

using namespace std;

struct req_linker_data {
  bool is_direct;
  Connection* conn;
  http_message* connect_msg;
  RequestGenerator* gen;
  Requester* requester;
  pair<string, string> endpoint;
};

struct wrap_data {
  wrapper_create_func_t wrapper_creator;
  req_linker_data* link_data;
};

class ReqLinker : public Connection {
 public:
  ReqLinker(RequestGenerator* gen, Requester* requester,
            const pair<string, string>& endpoint)
    : gen(NULL), requester(requester), endpoint(endpoint) {
    assert(add_request(gen));
  }

  virtual ~ReqLinker() {
    if(gen) {
      delete gen; 
      delete writer;
      delete reader;
      delete in_msg;
      delete out_msg;
    } else {
      requester->req_mp[endpoint].erase(this);
    }
  }

  bool add_request(RequestGenerator* gen) {
    if(this->gen) {
      return false;
    }
    this->gen = gen;
    in_msg = new http_message(false);
    out_msg = new http_message(true);
    reader = requester->reader_creator(in_msg);
    writer = requester->writer_creator(out_msg);
    return true;
  }

  virtual size_t get_write_size() {
    if(!gen) {
      return 0;
    }
    gen->generate_request(out_msg);
    ssize_t ret = writer->get_write_size();
    check_done();
    return ret;
  }
  
  virtual ssize_t write(int s, size_t max_write) {
    if(!gen) {
      return 0;
    }
    ssize_t ret = writer->write(s, max_write);
    check_done();
    return ret;
  }

  virtual ssize_t read(int s, size_t max_read) {
    if(!gen) {
      return 0;
    }
    ssize_t ret = reader->read(s, max_read);
    gen->read_response(in_msg);
    check_done();
    return ret;
  }

  void check_done() {
    if(gen && writer->get_write_size() == 0 &&
       in_msg->is_sent() && out_msg->is_sent()) {
      delete gen;
      delete writer;
      delete reader;
      delete in_msg;
      delete out_msg;
      gen = NULL; writer = NULL; reader = NULL; in_msg = NULL; out_msg = NULL;
      requester->req_mp[endpoint].insert(this);
    }
  }
  
  Requester* requester;
  RequestGenerator* gen;
  http_message* in_msg;
  http_message* out_msg;
  ConnectionWriter* writer;
  ConnectionReader* reader;

  pair<string, string> endpoint;
};

class WrapperWrapper : public Connection {
 public:
  WrapperWrapper(Requester* req, pair<string, string> endpoint,
                 Connection* conn)
    : req(req), endpoint(endpoint), conn(conn) {
  }
  virtual ~WrapperWrapper() {
    req->conn_mp[endpoint].erase(this);
    delete conn;
  }
  virtual size_t get_write_size() { return conn->get_write_size(); }
  virtual ssize_t write(int s, size_t max_write) {
    return conn->write(s, max_write);
  }
  virtual ssize_t read(int s, size_t max_read) {
    return conn->read(s, max_read);
  }
  virtual void update() { conn->update(); }

  Connection* conn;
  Requester* req;
  pair<string, string> endpoint;
};

class Connector : public Connection {
 public:
  Connector(Requester* req, http_message* connect_msg, Connection* conn)
    : req(req), connect_msg(connect_msg), conn(conn), writer(NULL) {
    if(connect_msg) {
      writer = req->writer_creator(connect_msg);
    }
  }

  virtual ~Connector() {
    if(writer) {
      delete writer;
      delete connect_msg;
    }
    delete conn;
  }

  virtual size_t get_write_size() {
    if(writer) {
      size_t amt = writer->get_write_size();
      if(!amt && connect_msg->is_sent()) {
        delete writer;
        delete connect_msg;
        writer = NULL;
        connect_msg = NULL;
      } else {
        return amt;
      }
    }
    return conn->get_write_size();
  }

  virtual ssize_t write(int s, size_t max_write) {
    if(writer) {
      writer->write(s, max_write);
    } else {
      conn->write(s, max_write);
    }
  }

  virtual ssize_t read(int s, size_t max_read) {
    conn->read(s, max_read);
  }

 private:
  Requester* req;
  Connection* conn;
  http_message* connect_msg;
  ConnectionWriter* writer;
};

static Connection* req_linker_creator(void* req_linker_data_ptr) {
  req_linker_data* data = (req_linker_data*)req_linker_data_ptr;
  if(data->is_direct) {
    return new Connector(data->requester, data->connect_msg, data->conn);
  } else {
    return new ReqLinker(data->gen, data->requester, data->endpoint);
  }
}

static Connection* create_wrapper(void* wrap_data_ptr) {
  wrap_data* data = (wrap_data*)wrap_data_ptr;
  return new WrapperWrapper(data->link_data->requester,
                            data->link_data->endpoint,
                  data->wrapper_creator(data->link_data, req_linker_creator));
}

Requester::Requester(reader_create_func_t reader_creator,
                     writer_create_func_t writer_creator,
                     wrapper_create_func_t wrapper_creator,
                     connection_maker_func_t connection_maker,
                     int max_connections)
  : reader_creator(reader_creator), writer_creator(writer_creator),
    wrapper_creator(wrapper_creator), connection_maker(connection_maker),
    max_connections(max_connections), cli(create_wrapper) {
}

Requester::~Requester() {
}

bool Requester::add_request(const char* host, const char* service,
                            RequestGenerator* gen) {
  pair<string, string> endpoint = make_pair(string(host), string(service));

  req_linker_data data;
  data.is_direct = false;
  data.gen = gen;
  data.requester = this;
  data.endpoint = endpoint;
  wrap_data wdata;
  wdata.wrapper_creator = wrapper_creator;
  wdata.link_data = &data;

  // Try to re use an existing virtual connection that has finished.
  set<ReqLinker*>& reqs = req_mp[endpoint];
  if(!reqs.empty()) {
    assert((ReqLinker*)(*reqs.begin())->add_request(gen));
    reqs.erase(reqs.begin());
    return true;
  }

  // Try to establish a new virtual connection on an existing connection.
  set<WrapperWrapper*>& cns = conn_mp[endpoint];
  for(set<WrapperWrapper*>::iterator it = cns.begin(); it != cns.end(); it++) {
    if(connection_maker((Connection*)(*it)->conn, &data)) {
      return true;
    }
  }

  // Try establish a new connection.
  if(cns.size() < max_connections) {
    WrapperWrapper* wconn = (WrapperWrapper*)cli.connect(host, service, &wdata);
    if(wconn) {
      Connection* conn = wconn->conn;
      connection_maker(conn, &data);
      cns.insert(wconn);
      return true;
    }
  }
  return false;
}

bool Requester::add_connection(const char* host, const char* service,
                               http_message* connect_msg, Connection* cn) {
  pair<string, string> endpoint = make_pair(string(host), string(service));
  
  req_linker_data data;
  data.is_direct = true;
  data.connect_msg = connect_msg;
  data.conn = cn;
  data.requester = this;
  data.endpoint = endpoint;
  wrap_data wdata;
  wdata.wrapper_creator = wrapper_creator;
  wdata.link_data = &data;

  // Try to establish a new virtual connection on an existing connection.
  set<WrapperWrapper*>& cns = conn_mp[endpoint];
  for(set<WrapperWrapper*>::iterator it = cns.begin(); it != cns.end(); it++) {
    if(connection_maker((Connection*)(*it)->conn, &data)) {
      return true;
    }
  }
  
  // Try establish a new connection.
  if(cns.size() < max_connections) {
    WrapperWrapper* wconn = (WrapperWrapper*)cli.connect(host, service, &wdata);
    if(wconn) {
      Connection* conn = wconn->conn;
      connection_maker(conn, &data);
      cns.insert(wconn);
      return true;
    }
  }
  return false;
}
