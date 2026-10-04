#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>

#define PORT 9410
#define AUTH_TOKEN "OPS-2705"
#define SID "5072"
#define BUFFER_SIZE 1024

/* Send one complete protocol line */
void send_response(int client_fd, const char *message)
{
    send(client_fd, message, strlen(message), 0);
}

/* Build SYSINFO response using Linux system information */
void handle_sysinfo(int client_fd)
{
    struct sysinfo info;
    double cpu_load;
    unsigned long mem_used_mb;
    unsigned long total_ram;
    unsigned long free_ram;
    char response[BUFFER_SIZE];

    if (sysinfo(&info) != 0)
    {
        send_response(client_fd,
                      "ERR 006 SYSINFO_FAILED SID:" SID "\n");
        return;
    }

    /*
     * loads[0] stores the 1-minute load average
     * using a fixed-point representation.
     */
    cpu_load = (double)info.loads[0] / 65536.0;

    total_ram = info.totalram * info.mem_unit;
    free_ram = info.freeram * info.mem_unit;

    mem_used_mb =
        (total_ram - free_ram) / (1024 * 1024);

    snprintf(response,
             sizeof(response),
             "OK SYSINFO %.2f %lu %ld SID:%s\n",
             cpu_load,
             mem_used_mb,
             info.uptime,
             SID);

    send_response(client_fd, response);
}

int main(void)
{
    int server_fd, client_fd;
    int opt = 1;

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

    /* Allow quick reuse of port 9410 */
    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    /* Bind Agent to personalised port */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    /* Start listening */
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

        client_fd =
            accept(server_fd,
                   (struct sockaddr *)&client_addr,
                   &client_len);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("\nController connected from %s\n",
               inet_ntoa(client_addr.sin_addr));

        int authenticated = 0;

        /*
         * Command loop:
         * keep receiving commands until Controller
         * disconnects.
         */
        while (1)
        {
            memset(buffer, 0, sizeof(buffer));

            ssize_t bytes_received =
                recv(client_fd,
                     buffer,
                     sizeof(buffer) - 1,
                     0);

            if (bytes_received <= 0)
            {
                printf("Controller disconnected.\n");
                break;
            }

            buffer[bytes_received] = '\0';

            /* Remove CR/LF */
            buffer[strcspn(buffer, "\r\n")] = '\0';

            printf("Received: %s\n", buffer);

            /*
             * AUTH must succeed before any
             * other command is accepted.
             */
            if (!authenticated)
            {
                if (strcmp(buffer,
                           "AUTH " AUTH_TOKEN) == 0)
                {
                    authenticated = 1;

                    send_response(
                        client_fd,
                        "OK AUTHENTICATED SID:"
                        SID "\n");

                    printf("Authentication successful.\n");
                }
                else
                {
                    send_response(
                        client_fd,
                        "ERR 001 AUTH_FAILED SID:"
                        SID "\n");

                    printf("Authentication failed.\n");
                }

                continue;
            }

            /* SYSINFO command */
            if (strcmp(buffer, "SYSINFO") == 0)
            {
                handle_sysinfo(client_fd);
            }

            /* QUIT command */
            else if (strcmp(buffer, "QUIT") == 0)
            {
                send_response(
                    client_fd,
                    "OK BYE SID:" SID "\n");

                printf("Controller requested QUIT.\n");
                break;
            }

            /* Unknown command */
            else
            {
                send_response(
                    client_fd,
                    "ERR 003 UNKNOWN_COMMAND SID:"
                    SID "\n");
            }
        }

        close(client_fd);

        printf("Controller connection closed.\n");
    }

    close(server_fd);

    return 0;
}
