#include "blather.h"
// This file does things below:
// REPEAT:
//   check all sources
//   handle a join request if on is ready
//   for each client{
//     if the client is ready handle data from it
//   }

server_t server; // globle server

// handler for some signals for shutdown
void handle_signals(int signo){
  server_shutdown(&server);
}

int main(int argc, char *argv[]){
  // Set handling functions for programs
  struct sigaction my_sa = {};
  // portable signal handling setup with sigaction()
  sigemptyset(&my_sa.sa_mask);
  // don't block any other signals during handling
  my_sa.sa_flags = SA_RESTART;
  // always restart system calls on signals possible

  my_sa.sa_handler = handle_signals;
  // run function handle_signals
  sigaction(SIGTERM, &my_sa, NULL);
  // register SIGTERM with given action

  my_sa.sa_handler = handle_signals;
  // run function handle_signals
  sigaction(SIGINT,  &my_sa, NULL);
  // register SIGINT with given action

  memset(&server, 0, sizeof(server_t));
  char name[256];
  // The second argument of commands is the name of the fifo
  strcpy(name, argv[1]);
  server_start(&server, name, DEFAULT_PERMS);

  while (1) {
    server_check_sources(&server);
    // If there is a ready join request, then handle it
    if (server_join_ready(&server)) {
      server_handle_join(&server);
    }

    // for each client:
    // if the client is ready handle data from it
    for (int i = 0; i < server.n_clients; i++) {
      if (server_client_ready(&server, i)) {
        server_handle_client(&server, i);
      }
    }
  }
  return 0;
}
