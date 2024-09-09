#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

/* Simple program that exercies
   getaddrinfo(), getnameinfo(), inet_aton(), inet_ntoa()

   Input parameter: domain name (e.g. www.rice.edu)
*/

#define MAX_LEN 100

int main(int argc, char** argv) {

  char *hostname;
  char hostname2[MAX_LEN];
  char *ipv4addr_in_ascii; /* i.e. a.b.c.d format */
  struct addrinfo *getaddrinfo_result, hints;
  struct sockaddr_in *socketaddr_ipv4_structure;
  struct sockaddr_in socketaddr_ipv4_structure2;


  /* start with the hostname from the command line input */

  hostname = argv[1];

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET; /* indicates we want IPv4 */

  if (getaddrinfo(hostname, NULL, &hints, &getaddrinfo_result) == 0) {
    socketaddr_ipv4_structure = (struct sockaddr_in *) getaddrinfo_result->ai_addr; /* contains an IPv4 address in network byte order */
    ipv4addr_in_ascii = inet_ntoa(socketaddr_ipv4_structure->sin_addr);
    printf("IP address in dot format after manipulations: %s\n", ipv4addr_in_ascii);
    freeaddrinfo(getaddrinfo_result);
  }

  /* now suppose we start with a valid ipv4addr_in_ascii e.g. 128.42.10.1 */

  inet_aton(ipv4addr_in_ascii, &socketaddr_ipv4_structure2.sin_addr);
  socketaddr_ipv4_structure2.sin_family = AF_INET;
  socketaddr_ipv4_structure2.sin_port = 0;

  if (getnameinfo((struct sockaddr *)&socketaddr_ipv4_structure2, sizeof(struct sockaddr_in), hostname2, MAX_LEN, NULL, 0, 0) == 0) {
    printf("Host name after manipulations: %s\n", hostname2);
  }

  return 0;

}
