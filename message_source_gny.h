#ifndef __MESSAGE_SOURCE_GNY
#define __MESSAGE_SOURCE_GNY

#include <arpa/inet.h>
#include <sys/types.h>

#include "connection.h"
#include "http_message.h"

//const size_t MAX_NON_ENTITY_BYTES = (1 << 17); // 128KB
const size_t MAX_READ_SIZE = (1 << 16);

extern
ConnectionReader* message_source_gny_creator(void* http_message_ptr);

class message_source_gny : public ConnectionReader {
 public:
  message_source_gny(http_message* msg);
  virtual ~message_source_gny();

  virtual ssize_t read(int s, size_t max_read);

 private:
  http_message* http_msg_ptr;
  std::string data_stream;

  int pos;
  int state;
  int substate;
  enum http_headers_e key;
};

#endif
