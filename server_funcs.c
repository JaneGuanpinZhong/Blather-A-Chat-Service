#include "blather.h"

client_t *server_get_client(server_t *server, int idx) {
  check_fail(idx > server->n_clients, 1, "Beyond size\n");
  return &(server->client[idx]);
}
// Gets a pointer to the client_t struct at the given index. If the
// index is beyond n_clients, the behavior of the function is
// unspecified and may cause a program crash.

void server_start(server_t *server, char *server_name, int perms) {
  printf("server_start()\n");
  strcpy(server->server_name, server_name); // Get the server name
  char fifo_name[MAXNAME];
  strcpy(fifo_name, server_name);
  strcat(fifo_name, ".fifo");
  remove(fifo_name);
  // Removes any existing file of that name prior to creation.

  mkfifo(fifo_name, perms);
  // create requests FIFO for client requests using given permissions

  server->join_fd = open(fifo_name, O_RDWR);
  check_fail(server->join_fd == -1, 1, "Cannot open fifo%s\n", fifo_name);
  // open FIFO read/write to avoid blocking, and stores its
  // file descriptor in join_fd. And check for errors.

  printf("server_start(): end\n");
}
// Initializes and starts the server with the given name. A join fifo
// called "server_name.fifo" should be created. Removes any existing
// file of that name prior to creation. Opens the FIFO and stores its
// file descriptor in join_fd.
//
// ADVANCED: create the log file "server_name.log" and write the
// initial empty who_t contents to its beginning. Ensure that the
// log_fd is position for appending to the end of the file. Create the
// POSIX semaphore "/server_name.sem" and initialize it to 1 to
// control access to the who_t portion of the log.

void server_shutdown(server_t *server) {
  char *fifo_name = strcat(server->server_name, ".fifo");
  close(server->join_fd);
  remove(fifo_name);

  // Create and send a BL_SHUTDOWN message to all clients
  mesg_t mesg;
  memset(&mesg, 0, sizeof(mesg_t));
  mesg.kind = BL_SHUTDOWN;
  server_broadcast(server, &mesg);

  // Remove all clients.
  for (int i = 0; i < server->n_clients; i++) {
    server_remove_client(server, i);
  }

  // ADVANCED: Close the log file. Close the log semaphore and unlink
  // it.
  close(server->log_fd);
  sem_close(server->log_sem);
  sem_unlink(server->log_sem);
}
// Shut down the server. Close the join FIFO and unlink (remove) it so
// that no further clients can join. Send a BL_SHUTDOWN message to all
// clients and proceed to remove all clients in any order.
//
// ADVANCED: Close the log file. Close the log semaphore and unlink
// it.

int server_add_client(server_t *server, join_t *join) {
  if (server->n_clients == MAXCLIENTS) {
    return 1;
  } // return 1 if the server as no space for clients

  client_t client;
  memset(&client, 0, sizeof(client_t));
  mesg_t join_message;
  memset(&join_message, 0, sizeof(mesg_t));
  strcpy(client.name, join->name);
  strcpy(client.to_client_fname, join->to_client_fname);

  // Open both fifos and check if open successfully
  client.to_client_fd = open(client.to_client_fname, O_RDWR);
  check_fail(client.to_client_fd == -1, 1, "Cannot open fifo %s\n", join->to_client_fname);
  client.to_server_fd = open(join->to_server_fname, O_RDWR);
  check_fail(client.to_server_fd == -1, 1, "Cannot open fifo %s\n", join->to_server_fname);

  // Complete a join message
  strcpy(join_message.name, client.name);
  join_message.kind = BL_JOINED;

  client.data_ready = 0;

  // Add the client to the server
  server->client[server->n_clients] = client;
  server->n_clients++;
  server_broadcast(server, &join_message);
  // Broadcast the join message to the server after the client is added
  return 0;
}
// Adds a client to the server according to the parameter join which
// should have fileds such as name filed in.  The client data is
// copied into the client[] array and file descriptors are opened for
// its to-server and to-client FIFOs. Initializes the data_ready field
// for the client to 0. Returns 0 on success and non-zero if the
// server as no space for clients (n_clients == MAXCLIENTS).

int server_remove_client(server_t *server, int idx) {
  client_t client = *(server_get_client(server, idx)); // get the client

  close(client.to_client_fd);
  remove(client.to_client_fname);
  close(client.to_server_fd);
  remove(client.to_server_fname);
  // Close fifos associated with the client and remove them.

  for (int i = idx; i < server->n_clients; i++) {
    server->client[i] = *server_get_client(server, i+1);
  }
  server->n_clients -= 1;
  // Shift the remaining clients to lower indices of the client[]
  // array and decrease n_clients.
  return 0;
}
// Remove the given client likely due to its having departed or
// disconnected. Close fifos associated with the client and remove
// them.  Shift the remaining clients to lower indices of the client[]
// array and decrease n_clients.

int server_broadcast(server_t *server, mesg_t *mesg) {
  // Write the message to all the to-client fifos
  for (int i = 0; i < server->n_clients; i++) {
    int nwrite = write(server_get_client(server, i)->to_client_fd,
                    mesg, sizeof(mesg_t));
    check_fail(nwrite == -1, 1, "Error in write\n");
  }
   printf("broadcast %s\n", mesg->body);
  return 0;
}
// Send the given message to all clients connected to the server by
// writing it to the file descriptors associated with them.
//
// ADVANCED: Log the broadcast message unless it is a PING which
// should not be written to the log.


void server_check_sources(server_t *server) {
  fd_set check_set; // set of file descriptors for select()
  FD_ZERO(&check_set); // init the set
  int maxfd = -1;
  int new_fd = server->join_fd;
  FD_SET(new_fd, &check_set);
  // Put a join request
  if (new_fd > maxfd) {
    maxfd = new_fd;
  }

  for (int i = 0; i < server->n_clients; i++) {
    new_fd = server_get_client(server, i)->to_server_fd;
    FD_SET(new_fd, &check_set);
    // Put each client into the set
    if (new_fd > maxfd) {
      maxfd = new_fd;
    }
  }

  int res = select(maxfd+1, &check_set, NULL, NULL, NULL);
  check_fail(res == -1, 1, "Select failed!\n");
  // sleep, wake up if any client ready for reading

  if (FD_ISSET(server->join_fd, &check_set)) {
    server->join_ready = 1;
  } // check if there is a new join request

  for (int i = 0; i < server->n_clients; i++) {
    int new_fd = server_get_client(server, i)->to_server_fd;
    if(FD_ISSET(new_fd, &check_set)){
      // check if each client has anything
      server_get_client(server, i)->data_ready = 1;
    }
  }
    printf("check source done\n");
}
// Checks all sources of data for the server to determine if any are
// ready for reading. Sets the servers join_ready flag and the
// data_ready flags of each of client if data is ready for them.
// Makes use of the select() system call to efficiently determine
// which sources are ready.

int server_join_ready(server_t *server) {
  return server->join_ready;
}
// Return the join_ready flag from the server which indicates whether
// a call to server_handle_join() is safe.

int server_handle_join(server_t *server) {
  join_t join;
  memset(&join, 0, sizeof(join_t));
  int nread = read(server->join_fd, &join, sizeof(join_t));
  check_fail(nread == -1, 1, "Error in read\n");
  server_add_client(server, &join);
  server->join_ready = 0;
  printf("server_handle_join(): done\n");
  return 0;
}
// Call this function only if server_join_ready() returns true. Read a
// join request and add the new client to the server. After finishing,
// set the servers join_ready flag to 0.

int server_client_ready(server_t *server, int idx) {
  return server->client[idx].data_ready;
}
// Return the data_ready field of the given client which indicates
// whether the client has data ready to be read from it.

int server_handle_client(server_t *server, int idx) {
  mesg_t message;
  memset(&message, 0, sizeof(mesg_t));
  int nread = read(server_get_client(server, idx)->to_server_fd, &message, sizeof(mesg_t));
  check_fail(nread == -1, 1, "Error in read\n");
  check_fail(idx > server->n_clients, 1, "Beyond size");
  // Check if the idx is legal
  mesg_kind_t kind = message.kind;
  printf("%s\n", message.body);

  // Departure and Message types should be broadcast to all other clients.
  if (kind == BL_DEPARTED){
    server_remove_client(server, idx);
    server_broadcast(server, &message);
  } else if (kind == BL_MESG) {
    server_broadcast(server, &message);
  } else if (kind == BL_PING) {
    server_get_client(server, idx)->last_contact_time = server->time_sec;
  }
  server_get_client(server, idx)->data_ready = 0;
  printf("Things SHOULD work\n");

  // ADVANCED: Update the last_contact_time of the client to the current
  // server time_sec.
  server_get_client(server, idx)->last_contact_time = server->time_sec;
  return 0;
}
// Process a message from the specified client. This function should
// only be called if server_client_ready() returns true. Read a
// message from to_server_fd and analyze the message kind. Departure
// and Message types should be broadcast to all other clients.  Ping
// responses should only change the last_contact_time below. Behavior
// for other message types is not specified. Clear the client's
// data_ready flag so it has value 0.
//
// ADVANCED: Update the last_contact_time of the client to the current
// server time_sec.

void server_tick(server_t *server) {
  server->time_sec += 1;
}
// ADVANCED: Increment the time for the server

void server_ping_clients(server_t *server) {
  mesg_t message;
  memset(&message, 0, sizeof(mesg_t));
  message.kind = BL_PING;
  server_broadcast(server, &message);
}
// ADVANCED: Ping all clients in the server by broadcasting a ping.

void server_remove_disconnected(server_t *server, int disconnect_secs);
// ADVANCED: Check all clients to see if they have contacted the
// server recently. Any client with a last_contact_time field equal to
// or greater than the parameter disconnect_secs should be
// removed. Broadcast that the client was disconnected to remaining
// clients.  Process clients from lowest to highest and take care of
// loop indexing as clients may be removed during the loop
// necessitating index adjustments.

void server_write_who(server_t *server);
// ADVANCED: Write the current set of clients logged into the server
// to the BEGINNING the log_fd. Ensure that the write is protected by
// locking the semaphore associated with the log file. Since it may
// take some time to complete this operation (acquire semaphore then
// write) it should likely be done in its own thread to preven the
// main server operations from stalling.  For threaded I/O, consider
// using the pwrite() function to write to a specific location in an
// open file descriptor which will not alter the position of log_fd so
// that appends continue to write to the end of the file.

void server_log_message(server_t *server, mesg_t *mesg) {
  int nwrite =  write(server->log_fd, mesg, sizeof(mesg_t));
  check_fail(nwrite == -1, 1, "Error in writing the message to the end of log file");
}
// ADVANCED: Write the given message to the end of log file associated
// with the server.
