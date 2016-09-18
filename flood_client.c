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

#define data_size 100000

struct workerArgs
{
	int socket;
  int countwrite;
  int countread;
  struct timespec latency;
};

struct timespec diff(struct timespec start, struct timespec end);

void *read_data(void *args);
void *write_data(void *args);

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

  pthread_t read_thread, write_thread;
  /* Used by write, read and log worker */
  struct workerArgs *wa;

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

  pthread_join(write_thread, NULL);
  printf("write_thread joined\n");
  pthread_join(read_thread, NULL);
  printf("read_thread joined\n");
	return 0;
}


void* write_data(void *args){
  int socket, nbytes;
  struct workerArgs *wa;

  char buffer[data_size];

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
    nbytes = send(socket, buffer, sizeof(buffer), 0);
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
  int socket;
  int nbytes = 0, total = 0;
  struct timespec end;
  struct workerArgs *wa;

  char buffer[data_size];

  struct timespec send_at;
  long int package_number;

  wa = (struct workerArgs*) args;
  socket = wa->socket;


  /* Read from the socket */
	while((nbytes = recv(socket, buffer+total, sizeof(buffer) - total, 0)) > 0){
    total += nbytes;
    buffer[sizeof(buffer) - 1] = '\0';
    if(total < sizeof(buffer))
      continue;
    total = 0;

    //fprintf(stderr, "%s --- \n", buffer);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    wa->countread++;

    perror
    nbytes = sscanf(buffer, ":::%ld;%ld;%ld:::", &(send_at.tv_sec), &(send_at.tv_nsec), &package_number);
    //if(package_number % 500 == 0){
      wa->latency = diff(send_at, end);
      fprintf(stdin, "decoded :::%ld;%ld;%ld::: -- latency: %ld;%ld",
          send_at.tv_sec, send_at.tv_nsec, package_number,
          wa->latency.tv_sec, wa->latency.tv_nsec);
    //}


    if(wa->countread % 50000 == 0){
      printf("Read Package %d: latency: %ld.%ld, send at: %ld.%ld\n",
        wa->countread,
        wa->latency.tv_sec, wa->latency.tv_nsec,
        send_at.tv_sec, send_at.tv_nsec);
    }
  }

	if (nbytes == 0)
	  perror("Server closed the connection");
  if (nbytes == -1)
    perror("Socket recv() failed");

	close(socket);
	pthread_exit(NULL);
}


struct timespec diff(struct timespec start, struct timespec end)
{
	struct timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}
