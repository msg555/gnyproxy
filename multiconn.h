#ifndef __MULTICONN_H
#define __MULTICONN_H

#include <queue>
#include <sys/types.h>

#include "connection.h"

const int MULTICONN_MAX_ID = 0xFF;
const size_t MULTICONN_MAX_SZ = 0x10000;

struct multiconn_data {
  void* init_data;
  conn_create_func_t conn_creator;
};

/* Creates a new Multiconn object.  Please pass a multiconn_data pointer
 * as the void* parameter.
 */
extern
Connection* multiconn_conn_creator(void* multiconn_data_ptr);
extern
Connection* multiconn_wrap_creator(void* init_data,
                                   conn_create_func_t conn_creator);

extern
bool multiconn_connector(Connection* conn, void* data);

/* A layer that supports multiple virtual connections over one socket.
 */
class Multiconn : public Connection {
 public:
  /* Constructs a new Multiconn object that will pass init_data to
   * new virtual connection objects.
   */
  Multiconn(void* init_data, conn_create_func_t conn_creator);

  virtual ~Multiconn();

  /* Select loop interface. */

  virtual size_t get_write_size();

  /* Should be called when there is data to be read on the socket
   * underlying this multiplexed connection.
   *
   * Returns the result from the call to recv.
   */
  virtual ssize_t read(int s, size_t max_read);

  /* Should be called when data can be written to the socket underlying
   * this multiplexed connection.
   *
   * Returns the result from the call to send.
   */
  virtual ssize_t write(int s, size_t max_write);

  /* Adds a new virtual connection to this multiplexed connection.
   *
   * Returns true if the virtual connection was added successfully.  A
   * new virtual connection may fail to add if there are no more
   * available virtual IDs to be assigned.  Multiconn takes ownership
   * of the vc object whether it was added successfully or not.  Please allocate
   * vc on the heap using "new".
   */
  bool add_virtual_connection(Connection* vc); 

  bool create_virtual_connection(void* data);

  virtual void update();
 private:
  Multiconn() {}
  Connection* get_virtual_connection(int id);

  void* init_data; /* Data passed to a virtual connection on creation. */
  conn_create_func_t conn_creator;

  /* Read state. */
  int r_id;
  size_t r_sz;
  size_t r_state;

  /* Write state. */
  int w_id;
  size_t w_sz;
  size_t w_state;
  std::queue<int> w_queue; /* Queue of virtual connections waiting to write. */

  Connection* vconn[MULTICONN_MAX_ID + 1];
  bool is_queued[MULTICONN_MAX_ID + 1];
};

#endif
