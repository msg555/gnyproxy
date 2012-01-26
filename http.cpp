#include "http.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>

int string_bin_search(const char * key, const char ** strings, int size);

enum http_request_verb_e atoverb(const char * key) {
    int result = string_bin_search(key, http_request_verb_strings,
                                   HTTP_REQUEST_VERB_UNKNOWN);
    return result != -1 ? (http_request_verb_e)result :
                          HTTP_REQUEST_VERB_UNKNOWN; } 

enum http_headers_e atoheader(const char * key) {
    int result = string_bin_search(key, http_header_strings,
                                   HEADER_UNKNOWN);
    return result != -1 ? (http_headers_e)result :
                          HEADER_UNKNOWN;
}

const char* verbtoa(enum http_request_verb_e key) {
    return http_request_verb_strings[key];
}

const char* headertoa(enum http_headers_e key) {
    return http_header_strings[key];
}

int string_bin_search(const char *key, const char ** strings, int size) {
    int low, high, mid, cond;
    low = 0;
    high = size;
    while(low < high) {
        mid = (high + low) / 2;
        if((cond = strcasecmp(key, strings[mid])) < 0) high = mid;
        else if(cond > 0)
            low = mid + 1;
        else 
            return mid;
        
    }
    return -1;
}

const char* http_request_verb_strings[] = {
    "CONNECT",
    "DELETE",
    "GET",
    "HEAD",
    "PATCH",
    "POST",
    "PUT",
    "OPTIONS",
    "TRACE"
};

const char* http_header_strings [] = {
    "accept",
    "accept-charset",
    "accept-encoding",
    "accept-language",
    "accept-ranges",
    "age",
    "allow",
    "authorization",
    "cache-control",
    "connection",
    "content-disposition",
    "content-encoding",
    "content-language",
    "content-length",
    "content-location",
    "content-md5",
    "content-range",
    "content-type",
    "cookie",
    "date",
    "etag",
    "expect",
    "expires",
    "from",
    "host",
    "if-match",
    "if-modified-since",
    "if-none-match",
    "if-range",
    "if-unmodified-since",
    "keep-alive",
    "last-modified",
    "location",
    "max-forwards",
    "pragma",
    "proxy-authenticate",
    "proxy-authorization",
    "range",
    "referer",
    "refresh",
    "retry-after",
    "server",
    "set-cookie",
    "te",
    "trailer",
    "transfer-encoding",
    "upgrade",
    "user-agent",
    "vary",
    "via",
    "warn",
    "warning",
    "www-authenticate",
};

