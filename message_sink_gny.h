#ifndef GYN_SINK_6
#define GYN_SINK_6

#include <vector>
#include <string>
#include <sys/types.h>

#include "connection.h"
#include "http_message.h"

extern
ConnectionWriter* message_sink_gny_creator(void* http_message_ptr);

class message_sink_gny : public ConnectionWriter {
 public:
  message_sink_gny(http_message*);
  virtual ~message_sink_gny() {}
  virtual size_t get_write_size();
  virtual ssize_t write(int socket, size_t max_write);

 private:
  void add_key_value_pair(const std::string&, const std::string&);
  void check_for_update();

  http_message* http_msg_ptr;
  http_message_state_e write_state;
  std::string data_stream;
  bool init;
};

#endif
