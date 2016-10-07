/*
 * adapted from: https://github.com/uchicago-cs/cmsc23300/blob/master/samples/sockets/server-pthreads.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

int data_size = 0;
char *port = NULL;

long int counter = 0;
pthread_mutex_t lock;

/* We will use this struct to pass parameters to one of the threads */
struct workerArgs
{
  int socket;
};

void *log_rate(void *args);
void *server_handler(void *args);

int main(int argc, char *argv[])
{
  /* The pthread_t type is a struct representing a single thread. */
  pthread_t server_thread, log_thread;

  int opt;
  while ((opt = getopt(argc, argv, "p:s:")) != -1){
    switch (opt)
    {
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
  if(port == NULL || data_size == 0){
    printf("port (-p) and size (-s) required\n"); exit(1);
  }

  /* If a client closes a connection, this will generally produce a SIGPIPE
           signal that will kill the process. We want to ignore this signal, so
     send() just returns -1 when this happens. */
  sigset_t new;
  sigemptyset (&new);
  sigaddset(&new, SIGPIPE);
  if (pthread_sigmask(SIG_BLOCK, &new, NULL) != 0)
  {
    perror("Unable to mask SIGPIPE");
    exit(-1);
  }

	if (pthread_mutex_init(&lock, NULL) != 0)
  {
      perror("mutex init failed");
      exit(-1);
  }

  if (pthread_create(&server_thread, NULL, server_handler, NULL) < 0)
  {
    perror("Could not create server thread");
    exit(-1);
  }

	if (pthread_create(&log_thread, NULL, log_rate, NULL) < 0)
	{
		perror("Could not create server thread");
		exit(-1);
	}


  pthread_join(server_thread, NULL);
  pthread_join(log_thread, NULL);

  pthread_exit(NULL);
}


void *server_handler(void *args)
{
  int serverSocket;
  struct addrinfo hints, *res, *p;
  struct sockaddr_storage *clientAddr;
  socklen_t sinSize = sizeof(struct sockaddr_storage);
  struct workerArgs *wa;
  int yes = 1;
  int nbytes;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE; // Return my address, so I can bind() to it

  /* Note how we call getaddrinfo with the host parameter set to NULL */
  if (getaddrinfo(NULL, port, &hints, &res) != 0)
  {
    perror("getaddrinfo() failed");
    pthread_exit(NULL);
  }

  for(p = res;p != NULL; p = p->ai_next)
  {
    if ((serverSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      perror("Could not open socket");
      continue;
    }

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
      perror("Socket setsockopt() failed");
      close(serverSocket);
      continue;
    }

    if (bind(serverSocket, p->ai_addr, p->ai_addrlen) == -1)
    {
      perror("Socket bind() failed");
      close(serverSocket);
      continue;
    }

    break;
  }

  freeaddrinfo(res);

  if (p == NULL)
  {
    fprintf(stderr, "Could not find a socket to bind to.\n");
    pthread_exit(NULL);
  }

  char* tosend = malloc(data_size);

  /* Loop and wait for connections */
  while (1)
  {
    if ((nbytes = recvfrom(serverSocket, tosend, data_size, 0, (struct sockaddr*) &clientAddr, &sinSize)) <= 0){
     perror("Could not recv data");
     free(clientAddr);
     free(tosend);
     close(serverSocket);
     pthread_exit(NULL);
    }
    tosend[nbytes-1] = '\0';

    if (sendto(serverSocket, tosend, nbytes, 0, (struct sockaddr*) &clientAddr, sinSize) == -1)
    {
      perror("Could not recv data");
      free(clientAddr);
      free(tosend);
      close(serverSocket);
      pthread_exit(NULL);
    }

    pthread_mutex_lock(&lock);
    counter++;
    pthread_mutex_unlock(&lock);

  }

  pthread_exit(NULL);
}

void* log_rate(void *args){
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  long long int running = 0;
  long long int last_run = 0;

  struct timespec last = start;

  struct workerArgs *wa;
  wa = (struct workerArgs*) args;

  while(1) {
    sleep(2);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    running = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    last_run = (end.tv_sec - last.tv_sec) * 1e9 + (end.tv_nsec - last.tv_nsec);
    last = end;
    printf("flow_rate, %d, %lld, %ld, %f\n", data_size, running, counter, 1e9*counter/last_run);
    pthread_mutex_lock(&lock);
    counter = 0;
    pthread_mutex_unlock(&lock);
  }
}
