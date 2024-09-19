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
    int bytes_sent;
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
    new_node->bytes_sent = 0;
    new_node->pending_data_in = 0;
    sprintf(new_node->file_path, "%d", socket);
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
    char *message = "Welcome! COMP/ELEC 429 Students!\n";

    /* number of bytes sent/received */
    int count;

    /* numeric value received */
    int num;

    /* linked list for keeping track of connected sockets */
    struct node head;
    struct node *current, *next;

    /* a buffer to read data */
    char *buf;
    int BUF_LEN = 1000;

    buf = (char *)malloc(BUF_LEN);

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
                        exit(1);
                    }
                    lseek(fd, current->bytes_sent, SEEK_SET);
                    unsigned short size_read;

                    if (current->bytes_sent == 0)
                    {
                        // first time sending, create header
                        if (current->pending_data_out > BUF_LEN - 18)
                        {
                            // we have more to send, fill the whole buffer
                            size_read = BUF_LEN - 18;
                        }
                        else
                        {
                            size_read = current->pending_data_out;
                        }
                        memset(buf, size_read, 2);
                        struct timeval send_time;
                        gettimeofday(&send_time, NULL);
                        memset(buf + 2, send_time.tv_sec, 8);
                        memset(buf + 10, send_time.tv_usec, 8);
                        int sz = read(fd, buf + 18, size_read);
                        if (close(fd) < 0)
                        {
                            perror("Error closing file");
                            exit(1);
                        }
                        count = send(current->socket, buf, size_read + 18, MSG_DONTWAIT);
                        assert(count == size_read + 18);
                        current->bytes_sent += size_read;
                    }
                    else
                    {
                        // we have sent the header already, now just data
                        if (current->pending_data_out - current->bytes_sent > BUF_LEN)
                        {
                            // we still have more to send, this time fill the whole buffer
                            size_read = BUF_LEN;
                        }
                        else
                        {
                            size_read = current->pending_data_out - current->bytes_sent;
                        }
                        int sz = read(fd, buf, size_read);
                        if (close(fd) < 0)
                        {
                            perror("Error closing file");
                            exit(1);
                        }
                        count = send(current->socket, buf, size_read, MSG_DONTWAIT);
                        assert(count == size_read);
                        current->bytes_sent += count;
                    }
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
                        if (current->bytes_sent == current->pending_data_out)
                        {
                            current->pending_data_out = 0;
                            current->bytes_sent = 0;
                        }
                    }
                }

                if (FD_ISSET(current->socket, &read_set))
                {
                    /* we have data from a client */

                    count = recv(current->socket, buf, BUF_LEN, 0);
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
                        close(current->socket);
                        dump(&head, current->socket);
                    }
                    else
                    {
                        struct timeval recv_time;
                        gettimeofday(&recv_time, NULL);
                        // recv_time.tv_sec;
                        // recv_time.tvusec;

                        int fd = open(current->file_path, O_CREAT | O_WRONLY | O_APPEND);

                        if (current->pending_data_in > 0)
                        {
                            if (current->pending_data_in > BUF_LEN)
                            {
                                write(fd, buf, BUF_LEN);
                                current->pending_data_in -= BUF_LEN;
                            }
                            else
                            {
                                write(fd, buf, current->pending_data_in);
                                current->pending_data_in = 0;
                            }
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
                            long timeStamp = ntohl(*(long *)(buf + 2));
                            if (size > BUF_LEN - 18)
                            {
                                write(fd, buf + 18, BUF_LEN - 18);
                                current->pending_data_in = size - (BUF_LEN - 18);
                            }
                            else
                            {
                                write(fd, buf + 18, size);
                            }
                            current->pending_data_out = size;
                        }
                        if (close(fd) < 0)
                        {
                            perror("Error closing file");
                            exit(1);
                        }
                        // reset buffer
                        memset(buf, 0, BUF_LEN);
                    }
                }
            }
        }
    }
}
