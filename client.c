#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/time.h>


// #define MAX_DATA_SIZE 1024
// struct ppmsg {
//     uint16_t size;         // 2 bytes (total size)
//     struct timeval tv;     // 8 bytes for tv_sec + 8 bytes for tv_usec
//     char data[MAX_DATA_SIZE]; // Arbitrary data
// };
//
// // Function to construct the message
// void create_ppmsg(struct ppmsg *msg, const char *data) {
//     // Get the current time
//     gettimeofday(&(msg->tv), NULL);
//
//     // Copy the data into the message
//     strncpy(msg->data, data, MAX_DATA_SIZE);
//
//     // Set the size field
//     // Size = size of tv_sec + tv_usec + size of data
//     msg->size = htons(sizeof(msg->tv) + strlen(data));
//
//     // Convert timestamp to network byte order
//     msg->tv.tv_sec = htonl(msg->tv.tv_sec);
//     msg->tv.tv_usec = htonl(msg->tv.tv_usec);
// }




int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <hostname> <port> <size> <count>\n", argv[0]);
        exit(1);
    }

    char *hostname = argv[1];
    int port = atoi(argv[2]);
    int size = atoi(argv[3]);
    int count = atoi(argv[4]);

    // Validate input ranges
    if (port < 18000 || port > 18200) {
        fprintf(stderr, "Port number must be between 18000 and 18200.\n");
        exit(1);
    }
    if (size < 18 || size > 65535) {
        fprintf(stderr, "Size must be between 18 and 65535 bytes.\n");
        exit(1);
    }
    if (count < 1 || count > 10000) {
        fprintf(stderr, "Count must be between 1 and 10000.\n");
        exit(1);
    }

    // Resolve hostname to IP address (extra)
    struct hostent *host_info = gethostbyname(hostname);
    if (host_info == NULL) {
        fprintf(stderr, "Error: could not resolve hostname %s\n", hostname);
        exit(1);
    }

    int sock;
    // Create the socket (stay same as example)
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creating socket");
        exit(1);
    }

    // Set up the server address structure (stay same as example)
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, host_info->h_addr, host_info->h_length);


    // Connect to the server (stay same as example)
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to server");
        close(sock);
        exit(1);
    }

    // forming messages:
    // Calculate the total size of the message (size (2 bytes) + timestamp (16 bytes) + data)
    int data_len = size;  // Example data length
    int total_size = 2 + 16 + data_len;  // Size field (2 bytes) + timestamps (16 bytes) + data

    char *input_data = (char *)malloc(data_len);
    if (input_data == NULL) {
        perror("Error allocating memory for data input");
        close(sock);
        exit(1);
    }
    printf("Enter the ping-pong message: ");
    fgets(input_data, data_len, stdin);

    // Allocate buffer for the message
    char *message = (char *)malloc(total_size);
    if (message == NULL) {
        perror("Error allocating memory for message");
        close(sock);
        exit(1);
    }

    // Fill the message buffer once (outside the loop)
    // 1. Set the size field (2 bytes) at message[0] and message[1]
    uint16_t data_size = htons(total_size);
    memcpy(message, &data_size, sizeof(uint16_t));

    // 2. Set the timestamp (tv_sec and tv_usec) in network byte order at message[2] - message[17]
    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint32_t tv_sec = htonl(tv.tv_sec);  // Convert tv_sec to network byte order
    uint32_t tv_usec = htonl(tv.tv_usec);  // Convert tv_usec to network byte order

    memcpy(message + 2, &tv_sec, sizeof(uint32_t));   // Copy tv_sec at offset 2
    memcpy(message + 6, &tv_usec, sizeof(uint32_t));  // Copy tv_usec at offset 6

    // 3. Copy the user input data into the data portion starting at message[18]
    memcpy(message + 18, input_data, data_len);  // Copy the input data


    // Fill the message buffer with the appropriate format: size, timestamp, and data
    for (int i = 0; i < count; i++) {

        // Send the message
        if (send(sock, message, total_size, 0) != total_size) {
            perror("Error sending message");
            close(sock);
            free(message);
            exit(1);
        }

        // Receive the response
        int bytes_received = 0;
        while (bytes_received < total_size) {
            int received = recv(sock, message + bytes_received, total_size - bytes_received, 0);
            if (received < 0) {
                perror("Error receiving message");
                close(sock);
                free(message);
                exit(1);
            }
            bytes_received += received;
        }

        // Print the received size, timestamp, and data
        uint16_t recv_size = ntohs(*(uint16_t *)message);  // Convert size back to host byte order
        uint32_t recv_tv_sec = ntohl(*(uint32_t *)(message + 2));  // Convert tv_sec back to host byte order
        uint32_t recv_tv_usec = ntohl(*(uint32_t *)(message + 6));  // Convert tv_usec back to host byte order

        printf("Received message:\n");
        printf("Size: %d\n", recv_size);
        printf("Timestamp: %u seconds, %u microseconds\n", recv_tv_sec, recv_tv_usec);
        printf("Data: %.*s\n", data_len, message + 18);  // Print the data portion
    }

    printf("Completed %d message exchanges with %s:%d\n", count, hostname, port);

    // Clean up
    close(sock);
    free(message);
    return 0;

}
