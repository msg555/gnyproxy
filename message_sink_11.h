/*
    message_sink_11.h

    C++ header to interface the http_message and send http1.1 packet
    to designated socket
*/
#ifndef MESSAGE_SINK_11_H
#define MESSAGE_SINK_11_H

#include <sys/types.h>

#include "connection.h"
#include "http_message.h"

extern
ConnectionWriter* message_sink_11_creator(void* http_message_ptr);

class message_sink_11 : public ConnectionWriter {
    private:
        message_sink_11() {}

        // pointer to http_message interface
        http_message *http_msg_ptr;
        // store packet to be sent out in a C++ std string
        std::string packet_str;

        http_message_state_e write_state;

        void check_for_update();

        bool is_init;
        long long entity_mode;

    public:
        message_sink_11(http_message *ptr);
        virtual ~message_sink_11() {}

        virtual size_t get_write_size();
        virtual ssize_t write(int sock, size_t max_write); 
};

#endif
