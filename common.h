#ifndef __COMMON_H
#define __COMMON_H

#include <string>
#include <utility>
#include <arpa/inet.h>

std::string trim(const std::string& str);
bool is_hex(char ch);
int hextoi(char ch);
char itohex(int x);
std::string url_encode(const std::string& url);
std::string url_decode(const std::string& url);
std::string mime_type(const std::string& file_path);
std::pair<std::string, std::string> parse_host(const std::string& host);

void write_string(std::string& data, const std::string& str);

void write_short(std::string& data, unsigned short x);
uint16_t read_short(std::string& data, int pos = 0);

#endif
