#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>

#define PORT 9410
#define AUTH_TOKEN "OPS-2705"
#define SID "5072"

#define BUFFER_SIZE 4096
#define STORAGE_DIR "./agentfiles/IT24102705"
#define MAX_FILE_SIZE (10 * 1024 * 1024)

/* =================================================
   BUFFERED TCP READER
   ================================================= */
typedef struct
{
    int fd;
    unsigned char buffer[BUFFER_SIZE];
    size_t start;
    size_t end;
} ConnReader;

/* =================================================
   SEND ALL BYTES
   ================================================= */
int send_all(int fd, const void *data, size_t length)
{
    const unsigned char *ptr = data;
    size_t total = 0;

    while (total < length)
    {
        ssize_t sent =
            send(
                fd,
                ptr + total,
                length - total,
                0
            );

        if (sent <= 0)
        {
            return -1;
        }

        total += (size_t)sent;
    }

    return 0;
}

/* =================================================
   SEND TEXT RESPONSE
   ================================================= */
void send_response(
    int client_fd,
    const char *message)
{
    send_all(
        client_fd,
        message,
        strlen(message)
    );
}

/* =================================================
   READ ONE COMPLETE TCP LINE
   ================================================= */
ssize_t read_line(
    ConnReader *reader,
    char *line,
    size_t line_size)
{
    size_t line_pos = 0;

    while (1)
    {
        while (reader->start < reader->end)
        {
            unsigned char c =
                reader->buffer[reader->start++];

            if (c == '\n')
            {
                if (line_pos > 0 &&
                    line[line_pos - 1] == '\r')
                {
                    line_pos--;
                }

                line[line_pos] = '\0';

                return (ssize_t)line_pos;
            }

            if (line_pos + 1 >= line_size)
            {
                return -2;
            }

            line[line_pos++] = (char)c;
        }

        ssize_t received =
            recv(
                reader->fd,
                reader->buffer,
                sizeof(reader->buffer),
                0
            );

        if (received <= 0)
        {
            return received;
        }

        reader->start = 0;
        reader->end = (size_t)received;
    }
}

/* =================================================
   READ EXACT RAW BYTES
   Used by PUT
   ================================================= */
int read_exact(
    ConnReader *reader,
    FILE *file,
    size_t total_bytes)
{
    size_t remaining = total_bytes;

    while (remaining > 0)
    {
        /*
           First use any bytes already received
           after the PUT command line.
        */
        if (reader->start < reader->end)
        {
            size_t available =
                reader->end - reader->start;

            size_t amount =
                available < remaining
                ? available
                : remaining;

            if (fwrite(
                    reader->buffer + reader->start,
                    1,
                    amount,
                    file) != amount)
            {
                return -1;
            }

            reader->start += amount;
            remaining -= amount;

            continue;
        }

        unsigned char temp[BUFFER_SIZE];

        size_t wanted =
            remaining < sizeof(temp)
                ? remaining
                : sizeof(temp);

        ssize_t received =
            recv(
                reader->fd,
                temp,
                wanted,
                0
            );

        if (received <= 0)
        {
            return -1;
        }

        if (fwrite(
                temp,
                1,
                (size_t)received,
                file) != (size_t)received)
        {
            return -1;
        }

        remaining -= (size_t)received;
    }

    return 0;
}

/* =================================================
   SAFE FILENAME CHECK
   ================================================= */
int valid_filename(const char *filename)
{
    if (filename == NULL ||
        filename[0] == '\0')
    {
        return 0;
    }

    /*
       Prevent directory traversal.
    */
    if (strstr(filename, "..") != NULL)
    {
        return 0;
    }

    if (strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL)
    {
        return 0;
    }

    return 1;
}

/* =================================================
   SYSINFO
   ================================================= */
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

    cpu_load =
        (double)info.loads[0] / 65536.0;

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

    send_response(
        client_fd,
        response
    );
}

/* =================================================
   LISTPROC
   ================================================= */
void handle_listproc(int client_fd)
{
    FILE *fp;

    char line[128];
    char response[BUFFER_SIZE];

    strcpy(
        response,
        "OK PROCS "
    );

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

    while (fgets(
               line,
               sizeof(line),
               fp) != NULL)
    {
        line[
            strcspn(line, "\r\n")
        ] = '\0';

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

    size_t len =
        strlen(response);

    if (len > 0 &&
        response[len - 1] == ',')
    {
        response[len - 1] = '\0';
    }

    strcat(
        response,
        " SID:" SID "\n"
    );

    send_response(
        client_fd,
        response
    );
}

/* =================================================
   EXEC
   ================================================= */
void handle_exec(
    int client_fd,
    const char *exec_command)
{
    const char *shell_command = NULL;

    FILE *fp;

    char line[256];
    char output[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    /*
       Fixed assignment whitelist.
    */
    if (strcmp(
            exec_command,
            "DATE") == 0)
    {
        shell_command = "date";
    }
    else if (strcmp(
                 exec_command,
                 "UPTIME") == 0)
    {
        shell_command = "uptime";
    }
    else if (strcmp(
                 exec_command,
                 "DISKFREE") == 0)
    {
        shell_command = "df -h";
    }
    else if (strcmp(
                 exec_command,
                 "HOSTNAME") == 0)
    {
        shell_command = "hostname";
    }
    else if (strcmp(
                 exec_command,
                 "WHOAMI") == 0)
    {
        shell_command = "whoami";
    }
    else
    {
        send_response(
            client_fd,
            "ERR 002 COMMAND_NOT_ALLOWED SID:"
            SID
            "\n"
        );

        return;
    }

    output[0] = '\0';

    fp = popen(
        shell_command,
        "r"
    );

    if (fp == NULL)
    {
        send_response(
            client_fd,
            "ERR 008 EXEC_FAILED SID:"
            SID
            "\n"
        );

        return;
    }

    while (fgets(
               line,
               sizeof(line),
               fp) != NULL)
    {
        line[
            strcspn(line, "\r\n")
        ] = '\0';

        if (strlen(output)
            + strlen(line)
            + 2
            >= sizeof(output))
        {
            break;
        }

        strcat(output, line);
        strcat(output, " ");
    }

    pclose(fp);

    size_t len =
        strlen(output);

    if (len > 0 &&
        output[len - 1] == ' ')
    {
        output[len - 1] = '\0';
    }

    /*
       Exact assignment format:
       OK EXEC_RESULT <output> SID:<sid>
    */
    snprintf(
        response,
        sizeof(response),
        "OK EXEC_RESULT %.3980s SID:%s\n",
        output,
        SID
    );

    send_response(
        client_fd,
        response
    );
}

/* =================================================
   PUT
   Controller -> Agent
   ================================================= */
void handle_put(
    int client_fd,
    ConnReader *reader,
    const char *filename,
    size_t filesize)
{
    char filepath[512];
    char response[BUFFER_SIZE];

    /*
       Maximum upload size = 10 MB.
    */
    if (filesize > MAX_FILE_SIZE)
    {
        send_response(
            client_fd,
            "ERR 004 FILE_TOO_LARGE SID:"
            SID
            "\n"
        );

        return;
    }

    if (!valid_filename(filename))
    {
        send_response(
            client_fd,
            "ERR 009 INVALID_FILENAME SID:"
            SID
            "\n"
        );

        return;
    }

    /*
       Create personalised storage directory.
    */
    mkdir(
        "./agentfiles",
        0755
    );

    mkdir(
        STORAGE_DIR,
        0755
    );

    snprintf(
        filepath,
        sizeof(filepath),
        "%s/%s",
        STORAGE_DIR,
        filename
    );

    FILE *file =
        fopen(
            filepath,
            "wb"
        );

    if (file == NULL)
    {
        send_response(
            client_fd,
            "ERR 010 FILE_WRITE_FAILED SID:"
            SID
            "\n"
        );

        return;
    }

    /*
       Receive exactly <filesize> raw bytes.
    */
    if (read_exact(
            reader,
            file,
            filesize) != 0)
    {
        fclose(file);

        /*
           Remove incomplete file.
        */
        remove(filepath);

        send_response(
            client_fd,
            "ERR 010 FILE_WRITE_FAILED SID:"
            SID
            "\n"
        );

        return;
    }

    fclose(file);

    /*
       Exact assignment PUT response.
    */
    snprintf(
        response,
        sizeof(response),
        "OK FILE_RECEIVED %s SID:%s\n",
        filename,
        SID
    );

    send_response(
        client_fd,
        response
    );

    printf(
        "File received: %s (%zu bytes)\n",
        filename,
        filesize
    );
}

/* =================================================
   GET
   Agent -> Controller
   ================================================= */
void handle_get(
    int client_fd,
    const char *filename)
{
    char filepath[512];
    char response[BUFFER_SIZE];

    unsigned char buffer[BUFFER_SIZE];

    struct stat file_info;

    /*
       Reject unsafe filenames.
    */
    if (!valid_filename(filename))
    {
        send_response(
            client_fd,
            "ERR 005 FILE_NOT_FOUND SID:"
            SID
            "\n"
        );

        return;
    }

    snprintf(
        filepath,
        sizeof(filepath),
        "%s/%s",
        STORAGE_DIR,
        filename
    );

    /*
       Check whether requested file exists.
    */
    if (stat(
            filepath,
            &file_info) != 0)
    {
        send_response(
            client_fd,
            "ERR 005 FILE_NOT_FOUND SID:"
            SID
            "\n"
        );

        return;
    }

    FILE *file =
        fopen(
            filepath,
            "rb"
        );

    if (file == NULL)
    {
        send_response(
            client_fd,
            "ERR 005 FILE_NOT_FOUND SID:"
            SID
            "\n"
        );

        return;
    }

    /*
       Exact assignment GET response header:

       OK FILE_SEND <filename> <filesize> SID:<sid>
    */
    snprintf(
        response,
        sizeof(response),
        "OK FILE_SEND %s %lld SID:%s\n",
        filename,
        (long long)file_info.st_size,
        SID
    );

    if (send_all(
            client_fd,
            response,
            strlen(response)) < 0)
    {
        fclose(file);
        return;
    }

    /*
       Send exactly filesize raw bytes
       immediately after the header.
    */
    size_t bytes_read;

    while ((bytes_read =
                fread(
                    buffer,
                    1,
                    sizeof(buffer),
                    file)) > 0)
    {
        if (send_all(
                client_fd,
                buffer,
                bytes_read) < 0)
        {
            fclose(file);
            return;
        }
    }

    fclose(file);

    printf(
        "File sent: %s (%lld bytes)\n",
        filename,
        (long long)file_info.st_size
    );
}

/* =================================================
   MAIN
   ================================================= */
int main(void)
{
    int server_fd;
    int client_fd;
    int opt = 1;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len;

    char command[BUFFER_SIZE];

    /* Create TCP socket */
    server_fd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    /*
       Allow quick restart of Agent.
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

    /* Bind to personalised port 9410 */
    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0)
    {
        perror("bind");

        close(server_fd);

        return 1;
    }

    if (listen(
            server_fd,
            5) < 0)
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

    /* =================================================
       ACCEPT CONTROLLER CONNECTIONS
       ================================================= */
    while (1)
    {
        client_len =
            sizeof(client_addr);

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
           Buffered TCP reader for this connection.
        */
        ConnReader reader;

        reader.fd = client_fd;
        reader.start = 0;
        reader.end = 0;

        int authenticated = 0;

        /* =============================================
           COMMAND LOOP
           ============================================= */
        while (1)
        {
            ssize_t result =
                read_line(
                    &reader,
                    command,
                    sizeof(command)
                );

            if (result == 0)
            {
                printf(
                    "Controller disconnected.\n"
                );

                break;
            }

            if (result < 0)
            {
                printf(
                    "Connection/read error.\n"
                );

                break;
            }

            printf(
                "Received: %s\n",
                command
            );

            /* =========================================
               AUTHENTICATION
               ========================================= */
            if (!authenticated)
            {
                if (strcmp(
                        command,
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

            /* =========================================
               SYSINFO
               ========================================= */
            if (strcmp(
                    command,
                    "SYSINFO"
                ) == 0)
            {
                handle_sysinfo(
                    client_fd
                );
            }

            /* =========================================
               LISTPROC
               ========================================= */
            else if (strcmp(
                         command,
                         "LISTPROC"
                     ) == 0)
            {
                handle_listproc(
                    client_fd
                );
            }

            /* =========================================
               EXEC
               ========================================= */
            else if (strncmp(
                         command,
                         "EXEC ",
                         5
                     ) == 0)
            {
                handle_exec(
                    client_fd,
                    command + 5
                );
            }

            /* =========================================
               PUT
               PUT <filename> <filesize>
               ========================================= */
            else if (strncmp(
                         command,
                         "PUT ",
                         4
                     ) == 0)
            {
                char filename[256];

                unsigned long long filesize;

                if (sscanf(
                        command + 4,
                        "%255s %llu",
                        filename,
                        &filesize) == 2)
                {
                    handle_put(
                        client_fd,
                        &reader,
                        filename,
                        (size_t)filesize
                    );
                }
                else
                {
                    send_response(
                        client_fd,
                        "ERR 011 INVALID_PUT_FORMAT SID:"
                        SID
                        "\n"
                    );
                }
            }

            /* =========================================
               GET
               GET <filename>
               ========================================= */
            else if (strncmp(
                         command,
                         "GET ",
                         4
                     ) == 0)
            {
                char filename[256];

                if (sscanf(
                        command + 4,
                        "%255s",
                        filename) == 1)
                {
                    handle_get(
                        client_fd,
                        filename
                    );
                }
                else
                {
                    send_response(
                        client_fd,
                        "ERR 005 FILE_NOT_FOUND SID:"
                        SID
                        "\n"
                    );
                }
            }

            /* =========================================
               QUIT
               ========================================= */
            else if (strcmp(
                         command,
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

            /* =========================================
               UNKNOWN COMMAND
               ========================================= */
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
