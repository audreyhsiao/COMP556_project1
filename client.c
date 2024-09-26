#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/time.h>

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <hostname> <port> <size> <count>\n", argv[0]);
        exit(1);
    }

    char *hostname = argv[1];
    int port = atoi(argv[2]);
    int size = atoi(argv[3]);
    int count = atoi(argv[4]);
    char *buf;

    // Validate input ranges
    if (port < 18000 || port > 18200)
    {
        fprintf(stderr, "Port number must be between 18000 and 18200.\n");
        exit(1);
    }
    if (size < 18 || size > 65535)
    {
        fprintf(stderr, "Size must be between 18 and 65535 bytes.\n");
        exit(1);
    }
    if (count < 1 || count > 10000)
    {
        fprintf(stderr, "Count must be between 1 and 10000.\n");
        exit(1);
    }

    // Resolve hostname to IP address (extra)
    struct hostent *host_info = gethostbyname(hostname);
    if (host_info == NULL)
    {
        fprintf(stderr, "Error: could not resolve hostname %s\n", hostname);
        exit(1);
    }

    int sock;
    // Create the socket (stay same as example)
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
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
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error connecting to server");
        close(sock);
        exit(1);
    }

    buf = (char *)malloc(size);
    int temp_cnt = recv(sock, buf, size, 0);
    if (temp_cnt < 0)
    {
        perror("receive failure");
        abort();
    }

    /* in this simple example, the message is a string,
       we expect the last byte of the string to be 0, i.e. end of string */
    if (buf[temp_cnt - 1] != 0)
    {
        /* In general, TCP recv can return any number of bytes, not
       necessarily forming a complete message, so you need to
       parse the input to see if a complete message has been received.
           if not, more calls to recv is needed to get a complete message.
        */
        printf("Message incomplete, something is still being transmitted\n");
    }
    else
    {
        printf("Here is what we got: %s", buf);
    }

    // forming messages:
    int total_size = size;
    int data_len = size - 18;

    char *message = (char *)calloc(total_size, sizeof(char));
    if (message == NULL)
    {
        perror("Error allocating memory for message");
        close(sock);
        exit(1);
    }

    // Fill the message buffer once (outside the loop)
    // 1. Set the size field (2 bytes) at message[0] and message[1]
    // memset(message, htons((uint16_t)data_len), sizeof(uint16_t));
    memcpy(message, &((uint16_t){htons(data_len)}), sizeof(uint16_t));

    // 2. Set the timestamp (tv_sec and tv_usec) in network byte order at message[2] - message[17]
    // TODO: Reset timestamp at every send
    // already move timestamp assignment (inside the loop)

    // 3. Copy the user input data into the data portion starting at message[18]
    // memcpy(message + 18, input_data, data_len); // Copy the input data
    // no need to fill the message

    int total_time;
    // Fill the message buffer with the appropriate format: size, timestamp, and data
    for (int i = 0; i < count; i++)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);

        // memset(message + 2, htobe64(tv.tv_sec), 8);   // Copy tv_sec at offset 2
        // memset(message + 10, htobe64(tv.tv_usec), 8); // Copy tv_usec at offset 10
        memcpy(message + 2, &((uint64_t){htobe64(tv.tv_sec)}), sizeof(uint64_t));
        memcpy(message + 10, &((uint64_t){htobe64(tv.tv_usec)}), sizeof(uint64_t));

        for (size_t i = 18; i < size; i++)
        {
            message[i] = rand();
        }

        // Send the message
        if (send(sock, message, total_size, 0) != total_size)
        {
            perror("Error sending message");
            close(sock);
            free(message);
            exit(1);
        }
        // printf("-----------------------------------\n");
        // printf("Message sent!\n");
        // printf("Total Size: %d\n", total_size);
        // printf("Size: %d\n", data_len);
        // printf("Timestamp: %lu seconds, %lu microseconds\n", tv.tv_sec, tv.tv_usec);
        // printf("Data Sent: %s\n", message + 18);
        // printf("\n");

        // Receive the response
        buf = (char *)malloc(total_size);
        if (buf == NULL)
        {
            perror("Failed to allocate memory for the received buffer");
            exit(1);
        }

        int received_bytes = 0;
        int epoch = 1024; // Adjust chunk size as necessary

        // Allocate a buffer for the incoming message
        buf = (char *)malloc(total_size);
        if (buf == NULL)
        {
            perror("Failed to allocate memory for the received buffer");
            close(sock);
            free(message);
            exit(1);
        }

        // receive multiple chunk messages
        while (received_bytes < total_size)
        {
            int remain_bytes = total_size - received_bytes; // Calculate remaining bytes to receive
            int to_receive;
            if (remain_bytes < epoch)
            {
                to_receive = remain_bytes;
            }
            else
            {
                to_receive = epoch;
            }

            int received = recv(sock, buf + received_bytes, to_receive, 0);

            if (received < 0)
            {
                perror("Error receiving message");
                close(sock);
                free(message);
                free(buf);
                exit(1);
            }
            else if (received == 0)
            {
                // Connection closed by the peer
                printf("Connection closed by peer\n");
                close(sock);
                free(message);
                free(buf);
                exit(1);
            }

            received_bytes += received; // Update total bytes received
        }

        // Ensure that the message is long enough for size and timestamp fields
        if (received_bytes < 18)
        {
            printf("Error: Incomplete message received\n");
            close(sock);
            free(message);
            free(buf);
            exit(1);
        }

        // Print the received size, timestamp, and data
        uint16_t recv_size = ntohs(*(uint16_t *)buf);             // Convert size back to host byte order
        uint64_t recv_tv_sec = be64toh(*(uint64_t *)(buf + 2));   // Convert tv_sec back to host byte order
        uint64_t recv_tv_usec = be64toh(*(uint64_t *)(buf + 10)); // Convert tv_usec back to host byte order

        // printf("Received message!\n");
        // printf("Size: %d\n", recv_size);
        // printf("Timestamp: %lu seconds, %lu microseconds\n", recv_tv_sec, recv_tv_usec);
        // printf("Data Received: %s\n", buf + 18);

        FILE *fp = fopen("RoundTripTime.txt", "ab");
        if (fp == NULL)
        {
            perror("Error opening file");
            return 1;
        }

        total_time += (recv_tv_sec - tv.tv_sec) * 1000000 + (recv_tv_usec - tv.tv_usec);
        fprintf(fp, "%lu, %lu\n", tv.tv_sec, tv.tv_usec);
        fprintf(fp, "%lu, %lu\n", recv_tv_sec, recv_tv_usec);

        fclose(fp);
        free(buf);
    }

    printf("Completed %d message exchanges with %s:%d\n", count, hostname, port);
    printf("Average time: %d\n", total_time / count);
    printf("\n");
    return 0;
}
