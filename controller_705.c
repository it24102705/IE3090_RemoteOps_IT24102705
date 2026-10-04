#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410
#define BUFFER_SIZE 1024

int main(void)
{
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

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

    printf("Connected to RemoteOps Agent successfully.\n");

    /* Send authentication command */
    const char *auth_command = "AUTH OPS-2705\n";

    if (send(sock_fd,
             auth_command,
             strlen(auth_command),
             0) < 0)
    {
        perror("send");
        close(sock_fd);
        return 1;
    }

    printf("Sent: AUTH OPS-2705\n");

    /* Receive authentication response */
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes_received =
        recv(sock_fd,
             buffer,
             sizeof(buffer) - 1,
             0);

    if (bytes_received <= 0)
    {
        printf("Failed to receive response from Agent.\n");
        close(sock_fd);
        return 1;
    }

    buffer[bytes_received] = '\0';

    printf("Agent response: %s", buffer);

    close(sock_fd);

    return 0;
}
