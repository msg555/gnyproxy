#ifndef __CONNECTION_H
#define __CONNECTION_H

#include <sys/types.h>

/* Abstract class that defines all of the functionality of a connection.
 *
 * size_t get_write_size()
 *   Should return the number of bytes that the virtual connection can write
 *   right now without blocking.
 *
 * ssize_t write(int s, int max_write)
 *   Should write data to the socket s with one call to send.  No more than
 *   max_write bytes should be written.  max_write will always be less than or
 *   equal to get_write_size().  Should return the value from send.
 *
 * ssize_t read(int s, int max_read)
 *   This function is called when data is received on the virtual connection.
 *   No more than max_read bytes should be read from the socket.  Should return
 *   the value from recv.  Virtual connections that fail to read in data as
 *   quickly as possible will tie up the underlying connection.
 */

class ConnectionReader {
 public:
  ConnectionReader() {}
  virtual ~ConnectionReader() {}

  virtual ssize_t read(int s, size_t max_read) = 0;
};

class ConnectionWriter {
 public:
  ConnectionWriter() {}
  virtual ~ConnectionWriter() {}

  virtual size_t get_write_size() = 0;
  virtual ssize_t write(int s, size_t max_write) = 0;
  virtual void update() {}
};

class Connection : public ConnectionReader, public ConnectionWriter {
 public:
  Connection() {}
  virtual ~Connection() {}

  virtual ssize_t read(int s, size_t max_read) = 0;
  virtual size_t get_write_size() = 0;
  virtual ssize_t write(int s, size_t max_write) = 0;
};

/* Connections will be deleted using "delete".  Threfore please use "new" to
 * allocate connections created by connection creator functions.
 */
typedef ConnectionReader*(*reader_create_func_t)(void*);
typedef ConnectionWriter*(*writer_create_func_t)(void*);
typedef Connection*(*conn_create_func_t)(void*);
typedef Connection*(*wrapper_create_func_t)(void*, conn_create_func_t);
typedef bool(*connection_maker_func_t)(Connection*, void*);


#endif
