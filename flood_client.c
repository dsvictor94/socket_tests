/*
 * adapted from: https://github.com/uchicago-cs/cmsc23300/blob/master/samples/sockets/client.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

struct workerArgs
{
  int socket;
  int countwrite;
  int countread;
  long long int round_trip;
};

void *read_data(void *args);
void *write_data(void *args);
void *log_rate(void *args);

int data_size = 0;

int main(int argc, char *argv[])
{
  /* A socket is just a file descriptor, i.e., an int */
  int clientSocket;

  struct addrinfo hints, // Used to provide hints to getaddrinfo()
                   *res, // Used to return the list of addrinfo's
                     *p; // Used to iterate over this list


  /* Host and port */
  char *host = NULL, *port = NULL;

  /* Used by getopt */
  int opt;

  pthread_t read_thread, write_thread, log_thread;
  /* Used by write, read and log worker */
  struct workerArgs *wa;

  /* Use getopt to fetch the host and port */
  while ((opt = getopt(argc, argv, "h:p:s:")) != -1){
    switch (opt)
    {
      case 'h':
        host = strdup(optarg);
        break;
      case 'p':
        port = strdup(optarg);
        break;
      case 's':
        data_size = atoi(strdup(optarg));
        break;
      default:
        printf("Unknown option\n"); exit(1);
    }
  }
  if( host == NULL || port == NULL || data_size == 0){
    printf("host (-h), port (-p) and size (-s) required\n"); exit(1);
  }


  /* We start by creating the "hints" addrinfo that is used to aid
     getaddrinfo in returning addresses that we'll actually be able
     to use */

  /* We want to leave all unused fields of hints to zero*/
  memset(&hints, 0, sizeof(hints));

  /* We leave the family unspecified. Based on the hostname (or IP address),
     getaddrinfo should be able to determine what family we want to use.
     However, we can set this to AF_INET or AF_INET6 to force a specific version */
  hints.ai_family = AF_UNSPEC;

  /* We want a reliable, full-duplex socket */
  hints.ai_socktype = SOCK_STREAM;

  /* Call getaddrinfo with the host and port specified in the command line */
  if (getaddrinfo(host, port, &hints, &res) != 0)
  {
    perror("getaddrinfo() failed");
    exit(-1);
  }

  /* Iterate through the list */
  for(p = res;p != NULL; p = p->ai_next)
  {
    /* The list could potentially include multiple entries (e.g., if a
       hostname resolves to multiple IP addresses). Here we just pick
       the first address we can connect to, although we could do
       additional filtering (e.g., we may prefer IPv6 addresses to IPv4
       addresses */

    /* Try to open a socket */
    if ((clientSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      perror("Could not open socket");
      continue;
    }

    /* Try to connect.
       Note: Like many other socket functions, this function expects a sockaddr and
             its length, both of which are conveniently provided in addrinfo */
    if (connect(clientSocket, p->ai_addr, p->ai_addrlen) == -1)
    {
      close(clientSocket);
      perror("Could not connect to socket");
      continue;
    }

    /* If we make it this far, we've connected succesfully. Don't check any more entries */
    break;
  }

  /* We don't need the linked list anymore. Free it. */
  freeaddrinfo(res);

  /* wrap args on struct */
  wa = malloc(sizeof(struct workerArgs));
  wa->socket = clientSocket;
  wa->countread = 0;
  wa->countwrite = 0;

  if (pthread_create(&read_thread, NULL, read_data, wa) != 0)
  {
    perror("Could not create a read_thread thread");
    free(wa);
    close(clientSocket);
    exit(-1);
  }

  if (pthread_create(&write_thread, NULL, write_data, wa) != 0)
  {
    perror("Could not create a write_thread thread");
    free(wa);
    close(clientSocket);
    exit(-1);
  }

  if (pthread_create(&log_thread, NULL, log_rate, wa) != 0)
  {
    perror("Could not create a write_thread thread");
    free(wa);
    close(clientSocket);
    exit(-1);
  }

  pthread_join(write_thread, NULL);
  printf("write_thread joined\n");
  pthread_join(read_thread, NULL);
  printf("read_thread joined\n");
  pthread_join(log_thread, NULL);
  printf("log_thread joined\n");
  return 0;
}


void* write_data(void *args){
  int socket, nbytes;
  struct workerArgs *wa;

  char *buffer = malloc(data_size);

  struct timespec send_at;
  long int package_number;

  wa = (struct workerArgs*) args;
  socket = wa->socket;

  while(1){
    clock_gettime(CLOCK_MONOTONIC_RAW, &send_at);
    wa->countwrite++;
    package_number++;

    nbytes = sprintf(buffer, ":::%ld;%ld;%ld:::", send_at.tv_sec, send_at.tv_nsec, package_number);
    //printf("encoded %d bytes: %s \n", nbytes, buffer);
    nbytes = send(socket, buffer, data_size, 0);
    if (nbytes == -1 && (errno == ECONNRESET || errno == EPIPE))
    {
      fprintf(stderr, "Socket disconnected\n");
      close(socket);
      free(wa);
      pthread_exit(NULL);
    }
    else if (nbytes == -1)
    {
      fprintf(stderr, "Unexpected error in send(): %d;", errno);
      free(wa);
      pthread_exit(NULL);
    }
  }
  pthread_exit(NULL);
}

void* read_data(void *args){
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  long long int running = 0;
  long int last_countread = 0;

  int socket;
  int nbytes = 0, total = 0;
  struct timespec end;
  struct workerArgs *wa;

  char *buffer = malloc(data_size);

  struct timespec send_at;
  long int package_number;

  wa = (struct workerArgs*) args;
  socket = wa->socket;


  /* Read from the socket */
  while((nbytes = recv(socket, buffer+total, data_size - total, 0)) > 0){
    total += nbytes;
    buffer[data_size - 1] = '\0';
    if(total < data_size)
      continue;
    total = 0;

    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    nbytes = sscanf(buffer, ":::%ld;%ld;%ld:::", &(send_at.tv_sec), &(send_at.tv_nsec), &package_number);

    wa->round_trip += (end.tv_sec - send_at.tv_sec) * 1e9 + (end.tv_nsec - send_at.tv_nsec);
    wa->countread++;

    if(wa->countread % 50000 == 0) {
      running = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
      printf("avg_round_trip, %d, %lld, %lld\n", data_size, running, (wa->round_trip / (wa->countread - last_countread)));
      wa->round_trip = 0;
      last_countread = wa->countread;
    }
  }

  if (nbytes == 0)
    perror("Server closed the connection");
  if (nbytes == -1)
    perror("Socket recv() failed");

  close(socket);
  pthread_exit(NULL);
}


void* log_rate(void *args){
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  long long int running = 0;
  long long int last_run = 0;

  long int last_countread = 0;
  long int last_countwrite = 0;
  struct timespec last = start;

  struct workerArgs *wa;
  wa = (struct workerArgs*) args;

  while(1) {
    sleep(2);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    running = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    last_run = (end.tv_sec - last.tv_sec) * 1e9 + (end.tv_nsec - last.tv_nsec);
    printf("output_flow_rate, %d, %lld, %f\n", data_size, running, 1e9 * (wa->countwrite - last_countwrite) / last_run);
    printf("input_flow_rate, %d, %lld, %f\n", data_size, running, 1e9 * (wa->countread - last_countread) / last_run);

    last_countwrite = wa->countwrite;
    last_countread = wa->countread;
    last = end;
  }
}
