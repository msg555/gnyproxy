#include "http_message.h"

#include <cstdio>

#include "http_server.h"

using namespace std;

Connection* entity_linker_conn_creator(void* entity_linker_data_ptr) {
  entity_linker_data* data = (entity_linker_data*)entity_linker_data_ptr;
  return new EntityLinker(data->reader_creator, data->writer_creator,
                          data->entity_gen_creator, data->entity_gen_data);
}

EntityLinker::EntityLinker(reader_create_func_t reader_creator, 
                           writer_create_func_t writer_creator,
                           entity_gen_create_func_t entity_gen_creator,
                           void* entity_gen_data)
    : reader_creator(reader_creator), writer_creator(writer_creator),
      entity_gen_creator(entity_gen_creator),
      entity_gen_data(entity_gen_data) {
  reader_msg = new http_message(true);
  reader = reader_creator(reader_msg);
  http_message* writer_msg = new http_message(false);
  reader_processor = entity_gen_creator(reader_msg, writer_msg,
                                        entity_gen_data);
  writer_msgs.push(make_pair(writer_msg, reader_processor));
  writer = writer_creator(writer_msg);
}

EntityLinker::~EntityLinker() {
  delete reader;
  delete writer;
  while(!writer_msgs.empty()) {
    delete writer_msgs.front().second;
    writer_msgs.pop();
  }
}

int countx= 0;
int county = 0;
size_t EntityLinker::get_write_size() {
  if(writer && writer->get_write_size() == 0 &&
     writer_msgs.front().second->is_done()) {
    delete writer;
    delete writer_msgs.front().second;
    writer_msgs.pop();
    writer = NULL;
  }
  if(!writer && !writer_msgs.empty()) {
    writer = writer_creator(writer_msgs.front().first);
  }
  if(writer) {
    writer_msgs.front().second->process();
  }
  return writer ? writer->get_write_size() : 0;
}

ssize_t EntityLinker::write(int s, size_t max_write) {
  ssize_t ret = writer->write(s, max_write);
  writer_msgs.front().second->process();
  return ret;
}

ssize_t EntityLinker::read(int s, size_t max_read) {
  ssize_t ret = reader->read(s, max_read);
  reader_processor->process();
  if(reader_msg->get_state() == HTTP_MESSAGE_DONE) {
    delete reader;
    reader_msg = new http_message(true);
    http_message* writer_msg = new http_message(false);
    reader = reader_creator(reader_msg);
    reader_processor = entity_gen_creator(reader_msg, writer_msg,
                                          entity_gen_data);
    writer_msgs.push(make_pair(writer_msg, reader_processor));
  }
  return ret;
}

