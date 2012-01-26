#ifndef __MESSAGE_SOURCE_11
#define __MESSAGE_SOURCE_11

#include <string>
#include <sys/types.h>

#include "connection.h"
#include "http_message.h"

const size_t MAX_NON_ENTITY_BYTES = (1 << 17); // 128KB

extern
ConnectionReader* message_source_11_creator(void* http_message_ptr);

class message_source_11 : public ConnectionReader {
 public:
  message_source_11(http_message* msg);
  virtual ~message_source_11() {}

  virtual ssize_t read(int s, size_t max_read);

 private:
  void parse_request_line(const std::string& line);
  void parse_response_line(const std::string& line);
  void parse_header(const std::string& line);
  void parse_footer(const std::string& line);

  int state;
  char last;
  size_t non_entity_bytes;
  http_message* msg;
  std::string buf;
  long long entity_mode;
  long long entity_state1;
  long long entity_state2;
};

#endif
