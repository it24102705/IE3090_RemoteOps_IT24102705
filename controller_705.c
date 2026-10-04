#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410
#define BUFFER_SIZE 4096

/* Send a command and receive one response */
int send_command(int sock_fd, const char *command)
{
    char buffer[BUFFER_SIZE];

    if (send(sock_fd, command, strlen(command), 0) < 0)
    {
        perror("send");
        return -1;
    }

    printf("Sent: %s", command);

    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes_received =
        recv(sock_fd,
             buffer,
             sizeof(buffer) - 1,
             0);

    if (bytes_received <= 0)
    {
        printf("Failed to receive response from Agent.\n");
        return -1;
    }

    buffer[bytes_received] = '\0';

    printf("Agent response: %s", buffer);

    return 0;
}

int main(void)
{
    int sock_fd;
    struct sockaddr_in server_addr;

    printf("RemoteOps Controller - IT24102705\n");

    /* Create TCP socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd < 0)
    {
        perror("socket");
        return 1;
    }

    /* Configure Agent address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sock_fd);
        return 1;
    }

    printf("Connecting to Agent %s:%d...\n",
           SERVER_IP, PORT);

    /* Connect to Agent */
    if (connect(sock_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Connected to RemoteOps Agent successfully.\n\n");

    /* 1. Authenticate */
    if (send_command(sock_fd,
                     "AUTH OPS-2705\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* 2. Request system information */
    if (send_command(sock_fd,
                     "SYSINFO\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* 3. Request process list */
    if (send_command(sock_fd,
                     "LISTPROC\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* 4. Graceful disconnect */
    if (send_command(sock_fd,
                     "QUIT\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    close(sock_fd);

    return 0;
}
