#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define AUTH_TOKEN "OPS-2705"
#define SID "5072"
#define BUFFER_SIZE 1024

int main(void)
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];

    /* Create TCP socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    /* Allow quick reuse of the port */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));

    /* Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    /* Bind socket */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    /* Listen for connections */
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("RemoteOps Agent - IT24102705\n");
    printf("Agent listening on TCP port %d...\n", PORT);

    while (1)
    {
        client_len = sizeof(client_addr);

        /* Accept a Controller connection */
        client_fd = accept(server_fd,
                           (struct sockaddr *)&client_addr,
                           &client_len);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("Controller connected from %s\n",
               inet_ntoa(client_addr.sin_addr));

        memset(buffer, 0, sizeof(buffer));

        /* Receive first command */
        ssize_t bytes_received =
            recv(client_fd,
                 buffer,
                 sizeof(buffer) - 1,
                 0);

        if (bytes_received <= 0)
        {
            printf("Controller disconnected.\n");
            close(client_fd);
            continue;
        }

        buffer[bytes_received] = '\0';

        /* Remove newline */
        buffer[strcspn(buffer, "\r\n")] = '\0';

        printf("Received: %s\n", buffer);

        /* Authentication check */
        if (strcmp(buffer, "AUTH " AUTH_TOKEN) == 0)
        {
            const char *response =
                "OK AUTHENTICATED SID:" SID "\n";

            send(client_fd,
                 response,
                 strlen(response),
                 0);

            printf("Authentication successful.\n");
        }
        else
        {
            const char *response =
                "ERR 001 AUTH_FAILED SID:" SID "\n";

            send(client_fd,
                 response,
                 strlen(response),
                 0);

            printf("Authentication failed.\n");
        }

        close(client_fd);
        printf("Controller connection closed.\n");
    }

    close(server_fd);
    return 0;
}
