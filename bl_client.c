#include "blather.h"
// The main thing to do in this file:
// read name of server and name of user from command line args
// create to-server and to-client FIFOs
// write a join_t request to the server FIFO
// start a user thread to read input
// start a server thread to listen to the server
// wait for threads to return
// restore standard terminal output

// globle variables
pthread_t user_thread;
pthread_t server_thread;
simpio_t simpio_actual;
int server_fd;
int to_client_fd;
int to_server_fd;
char user_name[MAXNAME];


// User thread:
// The loop repeat:
//     read input using simpio
//     when a line is ready
//     create a mesg_t with the line and write it to the to-server FIFO
// until end of input
// write a DEPARTED mesg_t into to-server
// cancel the server thread
void * user_thread_func() {
  while(1) {
    simpio_reset(&simpio_actual);
    iprintf(&simpio_actual, ""); // print prompt
    while(!simpio_actual.end_of_input && !simpio_actual.line_ready){
      simpio_get_char(&simpio_actual);
    }
    mesg_t message;
    memset(&message, 0, sizeof(mesg_t));
    strcpy(message.name, user_name);
    strcpy(message.body, simpio_actual.buf);

    // For Ctrl-D: the client will depart???
    if (simpio_actual.end_of_input) {
      message.kind = BL_DEPARTED;
    } else {
      // Otherwise it is just a normal message
      message.kind = BL_MESG;
    }
    // Write the message to the to-server fifo
    int nwrite = write(to_server_fd, &message, sizeof(mesg_t));
    check_fail(nwrite == -1, 1, "Error in write\n");

    // break the loop at the end of input, and cancel the thread
    if (simpio_actual.end_of_input) {
      pthread_cancel(server_thread);
      break;
    }
    simpio_reset(&simpio_actual);
  }

  // Write the depart message to the server through to-server fifo
  return NULL;
}

// server thread:
//   repeat:
//     read a mesg_t from to-client FIFO
//     print appropriate response to terminal with simpio
//   until a SHUTDOWN mesg_t is read
//   cancel the user thread
void* server_thread_func() {
  while(1) {
    mesg_t message;
    memset(&message, 0, sizeof(mesg_t));
    read(to_client_fd, &message, sizeof(mesg_t));
    // read a mesg_t from to-client FIFO
    if (message.kind == BL_MESG) {
      iprintf(&simpio_actual, "[%s] : %s\n", message.name, message.body);
    }
    if (message.kind == BL_JOINED) {
      iprintf(&simpio_actual, "-- %s JOINED --\n", message.name);
    } // a new client joined
    if (message.kind == BL_DEPARTED) {
      iprintf(&simpio_actual, "-- %s DEPARTED --\n", message.name);
    } // a client departed
    if (message.kind == BL_SHUTDOWN) {
      iprintf(&simpio_actual, "!!! server is shutting down !!!\n");
      pthread_cancel(user_thread);
      // a SHUTDOWN mesg_t is read, cancel the user thread
      break;
    }
  }
  return NULL;
}



int main(int argc, char *argv[]){
  memset(&simpio_actual, 0, sizeof(simpio_t));
  memset(&user_thread, 0, sizeof(pthread_t));
  memset(&server_thread, 0, sizeof(pthread_t));
  // Set handling functions for programs
  char prompt[MAXNAME];
  snprintf(prompt, MAXNAME, "%s>> ",argv[2]); // create a prompt string
  simpio_set_prompt(&simpio_actual, prompt);  // set the prompt
  simpio_reset(&simpio_actual); // initialize io
  simpio_noncanonical_terminal_mode();
  // set the terminal into a compatible mode

  char server_name[MAXNAME];
  strcpy(server_name, argv[1]);
  // The second argument from commands is the name of server
  strcpy(user_name, argv[2]);
  // The third argument from commands is the name of user

  // Read the second argument from commands and open the corresponding server fifo
  char server_fifo[MAXNAME];
  strcpy(server_fifo, argv[1]);
  strcat(server_fifo, ".fifo");
  server_fd = open(server_fifo, O_RDWR);
  check_fail(server_fd == -1, 1, "Cannot open server fifo\n");

  // Read the third argument from commands and create, open the corresponding to-server fifo
  char to_server_fifo[MAXNAME];
  strcpy(to_server_fifo, argv[2]);
  strcat(to_server_fifo, "_to_server.fifo");
  mkfifo(to_server_fifo, DEFAULT_PERMS);
  to_server_fd = open(to_server_fifo, O_RDWR);
  check_fail(to_server_fd == -1, 1, "Cannot open to_server fifo\n");

  // Read the third argument from commands and open the corresponding to-client fifo
  char to_client_fifo[MAXNAME];
  strcpy(to_client_fifo, argv[2]);
  strcat(to_client_fifo, "_to_client.fifo");
  mkfifo(to_client_fifo, DEFAULT_PERMS);
  to_client_fd = open(to_client_fifo, O_RDWR);
  check_fail(server_fd == -1, 1, "Cannot open to_server fifo\n");
  // read name of server and name of user from command line args
  // create to-server and to-client FIFOs

  join_t join;
  memset(&join, 0, sizeof(join_t));
  strcpy(join.name, user_name);
  strcpy(join.to_client_fname, to_client_fifo);
  strcpy(join.to_server_fname, to_server_fifo);
  int nwrite = write(server_fd, &join, sizeof(join_t));
  check_fail(nwrite == -1, 1, "Error in write\n");
  // Create and write a join_t request to the server FIFO

  // Create the threads and join them
  int user_thread_id = pthread_create(&user_thread,
  NULL, user_thread_func, NULL);
  check_fail(user_thread_id != 0, 1, "Failed to create the user thread!\n");
  int server_thread_id = pthread_create(&server_thread,
  NULL, server_thread_func, NULL);
  check_fail(server_thread_id != 0, 1, "Failed to create the server thread!\n");
  pthread_join(user_thread, NULL);
  pthread_join(server_thread, NULL);

  simpio_reset_terminal_mode();
  // restore standard terminal output
  printf("\n");
  // newline just to make returning to the terminal prettier

  close(server_fd);
  close(to_server_fd);
  close(to_client_fd);
  return 0;
}
