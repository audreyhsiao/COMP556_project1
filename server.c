#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <assert.h>

/**************************************************/
/* a few simple linked list functions             */
/**************************************************/

/* A linked list node data structure to maintain application
   information related to a connected socket */
struct node
{
    int socket;
    struct sockaddr_in client_addr;
    int pending_data_out;
    /* you will need to introduce some variables here to record
       all the information regarding this socket.
       e.g. what data needs to be sent next */
    int pending_data_in;
    char file_path[10];
    struct node *next;
};

/* remove the data structure associated with a connected socket
   used when tearing down the connection */
void dump(struct node *head, int socket)
{
    struct node *current, *temp;

    current = head;

    while (current->next)
    {
        if (current->next->socket == socket)
        {
            /* remove */
            temp = current->next;
            current->next = temp->next;
            free(temp); /* don't forget to free memory */
            return;
        }
        else
        {
            current = current->next;
        }
    }
}

/* create the data structure associated with a connected socket */
void add(struct node *head, int socket, struct sockaddr_in addr)
{
    struct node *new_node;

    new_node = (struct node *)malloc(sizeof(struct node));
    new_node->socket = socket;
    new_node->client_addr = addr;
    new_node->pending_data_out = 0;
    new_node->pending_data_in = 0;
    sprintf(new_node->file_path, "client:%d", socket);
    new_node->next = head->next;
    head->next = new_node;
}

/*****************************************/
/* main program                          */
/*****************************************/

/* simple server, takes one parameter, the server port number */
int main(int argc, char **argv)
{

    /* socket and option variables */
    int sock, new_sock, max;
    int optval = 1;

    /* server socket address variables */
    struct sockaddr_in sin, addr;
    unsigned short server_port = atoi(argv[1]);

    /* socket address variables for a connected client */
    socklen_t addr_len = sizeof(struct sockaddr_in);

    /* maximum number of pending connection requests */
    int BACKLOG = 5;

    /* variables for select */
    fd_set read_set, write_set;
    struct timeval time_out;
    int select_retval;

    /* a silly message */
    char *message = "Hello!\n";

    /* number of bytes sent/received */
    int count;

    /* linked list for keeping track of connected sockets */
    struct node head;
    struct node *current, *next;

    /* a buffer to read data */
    char *buf;
    uint16_t BUF_LEN = 65535;

    /* initialize dummy head node of linked list */
    head.socket = -1;
    head.next = 0;

    /* create a server socket to listen for TCP connection requests */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("opening TCP socket");
        abort();
    }

    /* set option so we can reuse the port number quickly after a restart */
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        perror("setting TCP socket option");
        abort();
    }

    /* fill in the address of the server socket */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(server_port);

    /* bind server socket to the address */
    if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("binding socket to address");
        abort();
    }

    /* put the server socket in listen mode */
    if (listen(sock, BACKLOG) < 0)
    {
        perror("listen on socket failed");
        abort();
    }

    buf = (char *)malloc(BUF_LEN);

    /* now we keep waiting for incoming connections,
       check for incoming data to receive,
       check for ready socket to send more data */
    while (1)
    {

        /* set up the file descriptor bit map that select should be watching */
        FD_ZERO(&read_set);  /* clear everything */
        FD_ZERO(&write_set); /* clear everything */

        FD_SET(sock, &read_set); /* put the listening socket in */
        max = sock;              /* initialize max */

        /* put connected sockets into the read and write sets to monitor them */
        for (current = head.next; current; current = current->next)
        {
            FD_SET(current->socket, &read_set);

            if (current->pending_data_out > 0 && current->pending_data_in == 0)
            {
                /* there is data pending to be sent, monitor the socket
                       in the write set so we know when it is ready to take more
                       data */
                FD_SET(current->socket, &write_set);
            }

            if (current->socket > max)
            {
                /* update max if necessary */
                max = current->socket;
            }
        }

        time_out.tv_usec = 100000; /* 1-tenth of a second timeout */
        time_out.tv_sec = 0;

        /* invoke select, make sure to pass max+1 !!! */
        select_retval = select(max + 1, &read_set, &write_set, NULL, &time_out);
        if (select_retval < 0)
        {
            perror("select failed");
            abort();
        }

        if (select_retval == 0)
        {
            /* no descriptor ready, timeout happened */
            continue;
        }

        if (select_retval > 0) /* at least one file descriptor is ready */
        {
            if (FD_ISSET(sock, &read_set)) /* check the server socket */
            {
                /* there is an incoming connection, try to accept it */
                new_sock = accept(sock, (struct sockaddr *)&addr, &addr_len);

                if (new_sock < 0)
                {
                    perror("error accepting connection");
                    free(message);
                    free(buf);
                    abort();
                }

                /* make the socket non-blocking so send and recv will
                       return immediately if the socket is not ready.
                       this is important to ensure the server does not get
                       stuck when trying to send data to a socket that
                       has too much data to send already.
                     */
                if (fcntl(new_sock, F_SETFL, O_NONBLOCK) < 0)
                {
                    perror("making socket non-blocking");
                    free(message);
                    free(buf);
                    abort();
                }

                /* the connection is made, everything is ready */
                /* let's see who's connecting to us */
                printf("Accepted connection. Client IP address is: %s\n",
                       inet_ntoa(addr.sin_addr));

                /* remember this client connection in our linked list */
                add(&head, new_sock, addr);

                /* let's send a message to the client just for fun */
                count = send(new_sock, message, strlen(message) + 1, 0);
                if (count < 0)
                {
                    perror("error sending message to client");
                    free(message);
                    free(buf);
                    abort();
                }
            }

            /* check other connected sockets, see if there is
                   anything to read or some socket is ready to send
                   more pending data */
            for (current = head.next; current; current = next)
            {
                next = current->next;

                /* see if we can now do some previously unsuccessful writes */
                if (FD_ISSET(current->socket, &write_set))
                {
                    /* the socket is now ready to take more data */
                    /* the socket data structure should have information
                           describing what data is supposed to be sent next.
                       but here for simplicity, let's say we are just
                           sending whatever is in the buffer buf
                         */

                    int fd = open(current->file_path, O_RDONLY);
                    if (fd < 0)
                    {
                        perror("File doesn't exist");
                        free(message);
                        free(buf);
                        exit(1);
                    }
                    unsigned short size_read = current->pending_data_out;

                    // load data into buffer to send
                    if (read(fd, buf, size_read) != size_read)
                    {
                        perror("Something wrong reading our data file");
                        free(message);
                        free(buf);
                        exit(1);
                    }

                    if (close(fd) < 0)
                    {
                        perror("Error closing file");
                        free(message);
                        free(buf);
                        exit(1);
                    }

                    count = send(current->socket, buf, size_read, MSG_DONTWAIT);
                    // reset buffer
                    memset(buf, 0, BUF_LEN);
                    if (count < 0)
                    {
                        if (errno == EAGAIN)
                        {
                            /* we are trying to dump too much data down the socket,
                            it cannot take more for the time being
                            will have to go back to select and wait til select
                            tells us the socket is ready for writing
                            */
                        }
                        else
                        {
                            /* something else is wrong */
                        }
                    }
                    else
                    {
                        /* note that it is important to check count for exactly
                            how many bytes were actually sent even when there are
                            no error. send() may send only a portion of the buffer
                            to be sent.
                        */
                        if (count == size_read)
                        {
                            current->pending_data_out = 0;
                            remove(current->file_path);
                        }
                    }
                }

                if (FD_ISSET(current->socket, &read_set))
                {
                    /* we have data from a client */

                    count = recv(current->socket, buf, BUF_LEN, 0);
                    struct timeval recv_time;
                    gettimeofday(&recv_time, NULL);
                    if (count <= 0)
                    {
                        /* something is wrong */
                        if (count == 0)
                        {
                            printf("Client closed connection. Client IP address is: %s\n", inet_ntoa(current->client_addr.sin_addr));
                        }
                        else
                        {
                            perror("error receiving from a client");
                        }

                        /* connection is closed, clean up */
                        remove(current->file_path);
                        close(current->socket);
                        dump(&head, current->socket);
                    }
                    else
                    {
                        int fd = open(current->file_path, O_CREAT | O_WRONLY | O_APPEND, 0777);

                        if (current->pending_data_in > 0)
                        {
                            if (current->pending_data_in > count)
                            {
                                write(fd, buf, count);
                                current->pending_data_in -= count;
                            }
                            else
                            {
                                write(fd, buf, current->pending_data_in);
                                current->pending_data_in = 0;
                            }
                            printf("Received part of message!\n");
                            printf("Count: %d, pending_data: %d\n", count, current->pending_data_in);
                        }
                        else
                        {
                            if (count < 18)
                            { // timpstepsize = tvsec(8 bytes) + tvusec(8 bytes)
                                printf("Error: incomplete file header");
                            }
                            // first two byte(size)
                            // convert network byte order to host byte order? not sure if needed
                            unsigned short size = ntohs(*(unsigned short *)buf);
                            // timestamp
                            uint64_t tv_sec = be64toh(*(uint64_t *)(buf + 2));
                            uint64_t tv_usec = be64toh(*(uint64_t *)(buf + 10));

                            if (size > count - 18)
                            {
                                printf("Received part of message (HEAD)!\n");
                                write(fd, buf, count);
                                current->pending_data_in = size - (count - 18);
                            }
                            else
                            {
                                printf("Received whole message!\n");
                                write(fd, buf, size + 18);
                            }
                            current->pending_data_out = size + 18;
                            // record receive time
                            FILE *fp = fopen("Delay.txt", "ab");
                            fprintf(fp, "%lu, %lu\n", recv_time.tv_sec, recv_time.tv_usec);
                            // and client send time
                            // recv_time - client_send_time = 2 * transmission delay + transmission independent delay
                            fprintf(fp, "%lu, %lu\n", tv_sec, tv_usec);
                            fprintf(fp, "%hu\n", count);
                            fclose(fp);

                            printf("Count: %d, Data Size: %d, pending_data: %d\n", count, size, current->pending_data_in);
                            printf("Timestamp: %lu seconds, %lu microseconds\n", tv_sec, tv_usec);
                        }
                        if (close(fd) < 0)
                        {
                            perror("Error closing file");
                            free(message);
                            free(buf);
                            exit(1);
                        }
                        // reset buffer
                        memset(buf, '\0', BUF_LEN);
                    }
                }
            }
        }
    }
}
