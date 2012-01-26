#include "http_message.h"
#include <iostream>

using namespace std;

http_message::~http_message() {

}

void http_message::add_header(const string& key, const string& value) {
    assert(state == HTTP_MESSAGE_HEADER);
    headers.push_back(make_pair(key, value));
}

void http_message::append_entity(const string& entity_) {
    assert(state == HTTP_MESSAGE_ENTITY);
    entity.append(entity_);
}

void http_message::add_footer(const string& key, const string& value) {
    assert(state == HTTP_MESSAGE_FOOTER);
    footers.push_back(make_pair(key, value));
}

void http_message::end_header() {
    assert(state <= HTTP_MESSAGE_HEADER);
    state = HTTP_MESSAGE_ENTITY;
}

void http_message::end_entity() {
    assert(state <= HTTP_MESSAGE_ENTITY);
    state = HTTP_MESSAGE_FOOTER;
}

void http_message::end_footer() {
    assert(state <= HTTP_MESSAGE_FOOTER);
    state = HTTP_MESSAGE_DONE;
}
