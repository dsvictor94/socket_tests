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

#define data_size 10485760
#define num_of_packages 5000

int main(int argc, char *argv[])
{

  struct timespec start, end, send_at, recv_at;
  int nbytes, read_count = 0;
  long long int round_trip = 0;
  char *buffer = malloc(data_size);

  /* A socket is just a file descriptor, i.e., an int */
  int clientSocket;

  struct addrinfo hints, // Used to provide hints to getaddrinfo()
                   *res, // Used to return the list of addrinfo's
                     *p; // Used to iterate over this list

  /* Host and port */
  char *host = NULL, *port = NULL;

  /* Used by getopt */
  int opt;

  /* Use getopt to fetch the host and port */
  while ((opt = getopt(argc, argv, "h:p:")) != -1){
    switch (opt)
    {
      case 'h':
        host = strdup(optarg);
        break;
      case 'p':
        port = strdup(optarg);
        break;
      default:
        printf("Unknown option\n"); exit(1);
    }
  }
  if( host == NULL || port == NULL){
    printf("host and port required\n"); exit(1);
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

  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  /* Iterate through the list */
  for(p = res;p != NULL; p = p->ai_next)
  {
    /* Try to open a socket */
    if ((clientSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      perror("Could not open socket");
      continue;
    }

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
  for(int i=0; i<num_of_packages; i++){
    //SEND
    clock_gettime(CLOCK_MONOTONIC_RAW, &send_at);

    nbytes = sprintf(buffer, ":::%d:::", i);
    //printf("encoded %d bytes: %s \n", nbytes, buffer);
    nbytes = send(clientSocket, buffer, data_size, 0);
    if (nbytes == -1 && (errno == ECONNRESET || errno == EPIPE))
    {
      fprintf(stderr, "Socket disconnected\n");
      close(clientSocket);
      exit(-1);
    }
    else if (nbytes == -1)
    {
      fprintf(stderr, "Unexpected error in send(): %d;", errno);
      exit(-1);
    }

    // RECV
    int total=0;
    while((total < data_size-1) && (nbytes = recv(clientSocket, buffer+total, data_size - total, 0)) > 0){
      total += nbytes;
      buffer[data_size - 1] = '\0';
      read_count++;
    }

    if (nbytes == 0)
      perror("Server closed the connection");
    if (nbytes == -1)
      perror("Socket recv() failed");

    clock_gettime(CLOCK_MONOTONIC_RAW, &recv_at);

    //nbytes = sscanf(buffer, ":::%d:::", &package_number);

    round_trip += (recv_at.tv_sec - send_at.tv_sec) * 1e9 + (recv_at.tv_nsec - send_at.tv_nsec);
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);

  long long total_time = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);

  printf("avg_round_trip, %d, %d, %f\n", data_size, num_of_packages, round_trip/(num_of_packages*1e9));
  printf("efficiency, %d, %d, %lld, %lld, %f\n", data_size, num_of_packages, total_time, round_trip, 1.0*round_trip/total_time);
  printf("fragmentation_rate, %d, %d, %d, %f\n", data_size, num_of_packages, read_count, 1.0*read_count/num_of_packages);
  return 0;
}
