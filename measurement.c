#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define PORT 8080
#define DATA_SIZE  500 //(1024 * 1024) // 1MB
#define NUM_MESSAGES 100


int main(int argc, char** argv) {

  // creat data to send to the server
  char *data = malloc(DATA_SIZE);
  memset(data, 'A', DATA_SIZE); // fill in the data with something

  /* our client socket */
  char* hostname;
  // The port on which the server is running (on CLEAR, the usable range is 18000 <= port <= 18200)
  short port;
  // The size in bytes of each message to send
  unsigned short sock;
  // The number of message exchanges to perform
  int loops = 100;

  struct timeval start, end;
  long long msg_start_time, msg_end_time;

  /* variables for identifying the server */
  unsigned int server_addr; // ip for the server
  //struct sockaddr_in sin;
  struct sockaddr_in server;
  struct addrinfo *getaddrinfo_result, hints;

  /* convert server domain name to IP address */
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET; /* indicates we want IPv4 */

  // get the IP address
  if (getaddrinfo(argv[1], NULL, &hints, &getaddrinfo_result) == 0) {
    server_addr = (unsigned int) ((struct sockaddr_in *) (getaddrinfo_result->ai_addr))->sin_addr.s_addr;
    freeaddrinfo(getaddrinfo_result);
  }

  /* server port number */
  unsigned short server_port = atoi (argv[2]);

  /* create a socket */
  if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
      perror ("opening TCP socket");
      abort ();
    }

  /* fill in the server's address */
  
  memset (&sin, 0, sizeof (sin)); // set to all zeros
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = server_addr;
  server.sin_port = htons(server_port);

  /* connect to the server */
  if (connect(sock, (struct sockaddr *) &server, sizeof (server)) < 0)
    {
      perror("connect to server failed");
      abort();
    }

    // for the bandwidth and latency measurement 
    double total_bandwidth_independent_time = 0.0;

    
    for(int i = 0; i < NUMS_MESSAGE; i++){
        struct timeval sent, receive;
        gettimeofday(&sent, NULL);

        // send the msg
        printf("Sending the message %d" %i);
        if(send(sock, data,DATA_SIZE,0)){

        // receiving
        char pong[500]; // i assume 500 is enough ?
        count = recv(sock, pong, sizeof(pong), 0);

        if (count < 0)
        {
        perror("receive failure");
        abort();
        }

        // record the time we receive the data
        gettimeofday(&receive, NULL); 

        // count round trip time 
        long rtt = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        printf("Message %d: RTT: %ld microseconds\n", i + 1, rtt);

        // bytes per second
        // we have to convert rtt unit (microseconds) into seconds
        double bandwidth = (double)DATA_SIZE / (rtt / 1000000.0); 
      
        double bandwidth_independent_time = (rtt / 1000000.0) - 2 * bandwidth_dependent_time;
       
        total_bandwidth_independent_time+=bandwidth_independent_time;

        }

        // calculate the avg
        printf("Average Bandwidth independent transmission time: %f seconds\n", total_bandwidth_independent_time / NUM_MESSAGES);

        return 0;
    }
}
