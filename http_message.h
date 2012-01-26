/*
  http_message.h

  c++ header for source/sink interface of GNY http
  */

#ifndef HTTP_MESSAGE_H
#define HTTP_MESSAGE_H

#include <iostream>
using namespace std;
#include "http.h"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

enum http_message_state_e {
    HTTP_MESSAGE_HEADER,
    HTTP_MESSAGE_ENTITY,
    HTTP_MESSAGE_FOOTER,
    HTTP_MESSAGE_DONE
};

// Used for host, header pairs
typedef std::pair<std::string, std::string> key_value_t;

class http_message {
    private:
        http_message();

        //true if request, false for response
        bool is_request;
        
        //relative url
        std::string url;
        enum http_request_verb_e verb;
        int status;
        std::vector<key_value_t> headers;

        std::vector<key_value_t> footers;

        std::string entity;
        enum http_message_state_e state;
    public:
        http_message(bool is_request):
            is_request(is_request), verb(HTTP_REQUEST_VERB_UNKNOWN),
            status(-1), state(HTTP_MESSAGE_HEADER) {}
        ~http_message();
        // Source interface
        
        // url_str should be relative
        // i.e. for bar.com/foo, url_str = "/foo"
        void set_url(const std::string& url_str) 
        { url = url_str; }
        
        void set_verb(enum http_request_verb_e verb_)
        { verb = verb_;}

        void set_status(int status_) 
        { status = status_; }

        void add_header(const std::string& key, const std::string& value);

        void append_entity(const std::string&);

        void add_footer(const std::string& key, const std::string& value);

        void end_header();
        void end_entity();
        void end_footer();
        
        // Sink interface

        bool get_is_request() { return is_request; }

        const std::string& get_url() const { return url; }
        
        void get_entity(std::string& entity_) {
            entity.swap(entity_);
            entity.clear();
        }

        const std::string& peek_entity() { return entity; }

        enum http_request_verb_e get_verb() const { return verb; }

        int get_status() const { return status; }

        /* Don't you think this should be plural? */
        void get_header(std::vector<key_value_t>& headers_) {
          get_headers(headers_);
        }
    
        void get_headers(std::vector<key_value_t>& headers_) {
            headers.swap(headers_);
            headers.clear();
        }

        const std::vector<key_value_t>& peek_headers() { return headers; }

        void get_footers(std::vector<key_value_t>& footers_) {
            footers.swap(footers_);
            footers.clear();
        }

        const std::vector<key_value_t>& peek_footers() { return footers; }

        enum http_message_state_e get_state() const { return state; }

        bool is_sent() {
          return state == HTTP_MESSAGE_DONE && headers.empty() &&
                          entity.empty() && footers.empty();
        }
};


#endif
