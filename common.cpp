#include "common.h"

#include <unistd.h>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;

string trim(const string& str) {
  int a = 0;
  int b = str.size();
  while(a < b && isspace(str[a])) a++;
  while(a < b && isspace(str[b - 1])) b--;
  return str.substr(a, b - a);
}

bool is_hex(char ch) {
  return '0' <= ch && ch <= '9' ||
         'a' <= ch && ch <= 'f' ||
         'A' <= ch && ch <= 'f';
}

int hextoi(char ch) {
  return '0' <= ch && ch <= '9' ? ch - '0' :
         ('a' <= ch && ch <= 'f' ? ch - 'a' + 10 : ch - 'A' + 10);
}

char itohex(int x) {
  return x < 10 ? '0' + x : 'a' + x - 10;
}

string url_encode(const string& url) {
  string ret;
  for(int i = 0; i < url.size(); i++) {
    if(url[i] == '%') {
      ret += "%25";
    } else if(isspace(url[i])) {
      ret += '%';
      ret += itohex(url[i] >> 4);
      ret += itohex(url[i] & 0xF);
    } else {
      ret += url[i];
    }
  }
  return ret;
}

string url_decode(const string& url) {
  string ret;
  for(int i = 0; i < url.size(); i++) {
    if(url[i] == '%') {
      if(i + 2 < url.size() && is_hex(url[i + 1]) && is_hex(url[i + 2])) {
        ret += (char)(hextoi(url[i + 1]) << 4 | hextoi(url[i + 2]));
        i += 2;
      } else {
        ret += '%';
      }
    } else {
      ret += url[i];
    }
  }
  return ret;
}

string mime_type(const string& file_path) {
/*
  int dot = file_path.find_last_of('.');
  if(dot == string::npos) return "test/html";
  string ext = file_path.substr(dot + 1);  
*/

  char temp_path[PATH_MAX] = "/tmp/XXXXXX";
  int fd = mkstemp(temp_path);
  if(fd == -1) {
    perror("mkstemp");
    return "";
  }
  if(!fork()) {
    FILE* jnk = freopen(temp_path, "a", stdout);
    execlp("file", "file", "-ib", file_path.c_str(), (char*)NULL);
    perror("mime_type execlp");
    exit(1);
  }
  int status;
  if(-1 == waitpid(-1, &status, 0)) {
    perror("mime_type waitpid");
    return "";
  }
  ssize_t amt = read(fd, temp_path, sizeof(temp_path));
  close(fd);
  if(amt <= 0) {
    return "";
  }
  while(amt > 0 && temp_path[amt - 1] == '\n') amt--;
  return string(temp_path, amt);
}

pair<string, string> parse_host(const string& host) {
  int pos = host.find_last_of(':');
  pair<string, string> ret;
  if(pos == string::npos) {
    ret.first = host;
    ret.second = "80";
  } else {
    ret.first = host.substr(0, pos);
    ret.second = host.substr(pos + 1);
  }
  return ret;
}

void write_string(string& data, const string& str) {
  data += str;
  data += '\x0';
}

void write_short(string& data, unsigned short x) {
  x = htons(x);
  data += ((char*)&x)[0];
  data += ((char*)&x)[1];
}

uint16_t read_short(string& data, int pos) {
  unsigned short ret;
  memcpy(&ret, data.data() + pos, sizeof(ret));
  return ntohs(ret);
}
