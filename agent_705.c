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
#define BUFFER_SIZE 4096

/* -------------------------------------------------
   Send a response to the Controller
   ------------------------------------------------- */
void send_response(int client_fd, const char *message)
{
    send(client_fd, message, strlen(message), 0);
}

/* -------------------------------------------------
   SYSINFO
   Returns:
   CPU load
   Memory used in MB
   System uptime in seconds
   ------------------------------------------------- */
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
        send_response(
            client_fd,
            "ERR 006 SYSINFO_FAILED SID:" SID "\n"
        );

        return;
    }

    /* 1-minute system load average */
    cpu_load =
        (double)info.loads[0] / 65536.0;

    /* Calculate RAM usage */
    total_ram =
        info.totalram * info.mem_unit;

    free_ram =
        info.freeram * info.mem_unit;

    mem_used_mb =
        (total_ram - free_ram) /
        (1024 * 1024);

    snprintf(
        response,
        sizeof(response),

        "OK SYSINFO %.2f %lu %ld SID:%s\n",

        cpu_load,
        mem_used_mb,
        info.uptime,
        SID
    );

    send_response(client_fd, response);
}

/* -------------------------------------------------
   LISTPROC
   Returns a snapshot of currently running processes
   ------------------------------------------------- */
void handle_listproc(int client_fd)
{
    FILE *fp;

    char line[128];
    char response[BUFFER_SIZE];

    strcpy(response, "OK PROCS ");

    /*
       Get PID and process command name
       from the Linux ps command.
    */
    fp = popen(
        "ps -eo pid,comm --no-headers",
        "r"
    );

    if (fp == NULL)
    {
        send_response(
            client_fd,
            "ERR 007 LISTPROC_FAILED SID:" SID "\n"
        );

        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        /* Remove newline */
        line[strcspn(line, "\r\n")] = '\0';

        /*
           Ensure that we do not overflow
           the response buffer.
        */
        if (strlen(response)
            + strlen(line)
            + strlen(" SID:" SID "\n")
            + 2
            >= sizeof(response))
        {
            break;
        }

        strcat(response, line);
        strcat(response, ",");
    }

    pclose(fp);

    /* Remove final comma */
    size_t len = strlen(response);

    if (len > 0 &&
        response[len - 1] == ',')
    {
        response[len - 1] = '\0';
    }

    strcat(
        response,
        " SID:" SID "\n"
    );

    send_response(client_fd, response);
}

/* -------------------------------------------------
   MAIN
   ------------------------------------------------- */
int main(void)
{
    int server_fd;
    int client_fd;
    int opt = 1;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len;

    char buffer[BUFFER_SIZE];

    /* -------------------------------------------------
       Create TCP socket
       ------------------------------------------------- */
    server_fd =
        socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    /*
       Allow port 9410 to be reused quickly
       after restarting the Agent.
    */
    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)) < 0)
    {
        perror("setsockopt");

        close(server_fd);

        return 1;
    }

    /* -------------------------------------------------
       Configure server address
       ------------------------------------------------- */
    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(PORT);

    /* -------------------------------------------------
       Bind Agent to personalised port 9410
       ------------------------------------------------- */
    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0)
    {
        perror("bind");

        close(server_fd);

        return 1;
    }

    /* -------------------------------------------------
       Listen for Controller connections
       ------------------------------------------------- */
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");

        close(server_fd);

        return 1;
    }

    printf(
        "RemoteOps Agent - IT24102705\n"
    );

    printf(
        "Agent listening on TCP port %d...\n",
        PORT
    );

    /* -------------------------------------------------
       Main server loop
       ------------------------------------------------- */
    while (1)
    {
        client_len =
            sizeof(client_addr);

        /*
           Wait for Controller connection
        */
        client_fd =
            accept(
                server_fd,
                (struct sockaddr *)&client_addr,
                &client_len
            );

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf(
            "\nController connected from %s\n",
            inet_ntoa(client_addr.sin_addr)
        );

        /*
           Each new Controller must
           authenticate again.
        */
        int authenticated = 0;

        /* -------------------------------------------------
           Controller command loop
           ------------------------------------------------- */
        while (1)
        {
            memset(
                buffer,
                0,
                sizeof(buffer)
            );

            ssize_t bytes_received =
                recv(
                    client_fd,
                    buffer,
                    sizeof(buffer) - 1,
                    0
                );

            /*
               Controller disconnected or
               connection failed.
            */
            if (bytes_received <= 0)
            {
                printf(
                    "Controller disconnected.\n"
                );

                break;
            }

            buffer[bytes_received] =
                '\0';

            /*
               Remove newline characters.
            */
            buffer[
                strcspn(buffer, "\r\n")
            ] = '\0';

            printf(
                "Received: %s\n",
                buffer
            );

            /* =============================================
               AUTHENTICATION
               ============================================= */

            if (!authenticated)
            {
                if (strcmp(
                        buffer,
                        "AUTH " AUTH_TOKEN
                    ) == 0)
                {
                    authenticated = 1;

                    send_response(
                        client_fd,
                        "OK AUTHENTICATED SID:"
                        SID
                        "\n"
                    );

                    printf(
                        "Authentication successful.\n"
                    );
                }
                else
                {
                    send_response(
                        client_fd,
                        "ERR 001 AUTH_FAILED SID:"
                        SID
                        "\n"
                    );

                    printf(
                        "Authentication failed.\n"
                    );
                }

                continue;
            }

            /* =============================================
               SYSINFO
               ============================================= */

            if (strcmp(
                    buffer,
                    "SYSINFO"
                ) == 0)
            {
                handle_sysinfo(
                    client_fd
                );
            }

            /* =============================================
               LISTPROC
               ============================================= */

            else if (strcmp(
                         buffer,
                         "LISTPROC"
                     ) == 0)
            {
                handle_listproc(
                    client_fd
                );
            }

            /* =============================================
               QUIT
               ============================================= */

            else if (strcmp(
                         buffer,
                         "QUIT"
                     ) == 0)
            {
                send_response(
                    client_fd,
                    "OK BYE SID:"
                    SID
                    "\n"
                );

                printf(
                    "Controller requested QUIT.\n"
                );

                break;
            }

            /* =============================================
               UNKNOWN COMMAND
               ============================================= */

            else
            {
                send_response(
                    client_fd,
                    "ERR 003 UNKNOWN_COMMAND SID:"
                    SID
                    "\n"
                );
            }
        }

        close(client_fd);

        printf(
            "Controller connection closed.\n"
        );
    }

    close(server_fd);

    return 0;
}
