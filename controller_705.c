#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410
#define BUFFER_SIZE 4096

/* -------------------------------------------------
   Send all bytes
   ------------------------------------------------- */
int send_all(int fd, const void *data, size_t length)
{
    const unsigned char *ptr = data;
    size_t total = 0;

    while (total < length)
    {
        ssize_t sent =
            send(fd, ptr + total, length - total, 0);

        if (sent <= 0)
        {
            return -1;
        }

        total += (size_t)sent;
    }

    return 0;
}

/* -------------------------------------------------
   Receive one response line
   ------------------------------------------------- */
int receive_response(int sock_fd)
{
    char buffer[BUFFER_SIZE];
    size_t position = 0;

    while (position < sizeof(buffer) - 1)
    {
        char c;

        ssize_t received =
            recv(sock_fd, &c, 1, 0);

        if (received <= 0)
        {
            printf(
                "Failed to receive response from Agent.\n"
            );

            return -1;
        }

        buffer[position++] = c;

        if (c == '\n')
        {
            break;
        }
    }

    buffer[position] = '\0';

    printf(
        "Agent response: %s",
        buffer
    );

    return 0;
}

/* -------------------------------------------------
   Send normal text command
   ------------------------------------------------- */
int send_command(
    int sock_fd,
    const char *command)
{
    if (send_all(
            sock_fd,
            command,
            strlen(command)) < 0)
    {
        perror("send");
        return -1;
    }

    printf(
        "Sent: %s",
        command
    );

    return receive_response(sock_fd);
}

/* -------------------------------------------------
   PUT - Upload file to Agent
   ------------------------------------------------- */
int upload_file(
    int sock_fd,
    const char *local_filename,
    const char *remote_filename)
{
    FILE *file;

    struct stat file_info;

    char header[512];
    unsigned char buffer[BUFFER_SIZE];

    /* Get file size */
    if (stat(local_filename, &file_info) != 0)
    {
        perror("stat");
        return -1;
    }

    file =
        fopen(local_filename, "rb");

    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }

    /*
       PUT header:
       PUT <filename> <filesize>\n
    */
    snprintf(
        header,
        sizeof(header),
        "PUT %s %lld\n",
        remote_filename,
        (long long)file_info.st_size
    );

    /* Send PUT header */
    if (send_all(
            sock_fd,
            header,
            strlen(header)) < 0)
    {
        perror("send");

        fclose(file);

        return -1;
    }

    printf(
        "Sent: PUT %s %lld\n",
        remote_filename,
        (long long)file_info.st_size
    );

    /* Send exact raw file bytes */
    size_t bytes_read;

    while ((bytes_read =
                fread(
                    buffer,
                    1,
                    sizeof(buffer),
                    file)) > 0)
    {
        if (send_all(
                sock_fd,
                buffer,
                bytes_read) < 0)
        {
            perror("send file");

            fclose(file);

            return -1;
        }
    }

    fclose(file);

    printf(
        "File data sent: %s (%lld bytes)\n",
        local_filename,
        (long long)file_info.st_size
    );

    /* Receive PUT result */
    return receive_response(sock_fd);
}

/* -------------------------------------------------
   MAIN
   ------------------------------------------------- */
int main(void)
{
    int sock_fd;

    struct sockaddr_in server_addr;

    printf(
        "RemoteOps Controller - IT24102705\n"
    );

    /* Create TCP socket */
    sock_fd =
        socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd < 0)
    {
        perror("socket");
        return 1;
    }

    /* Configure Agent address */
    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons(PORT);

    if (inet_pton(
            AF_INET,
            SERVER_IP,
            &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");

        close(sock_fd);

        return 1;
    }

    printf(
        "Connecting to Agent %s:%d...\n",
        SERVER_IP,
        PORT
    );

    /* Connect */
    if (connect(
            sock_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0)
    {
        perror("connect");

        close(sock_fd);

        return 1;
    }

    printf(
        "Connected to RemoteOps Agent successfully.\n\n"
    );

    /* AUTH */
    if (send_command(
            sock_fd,
            "AUTH OPS-2705\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* SYSINFO */
    if (send_command(
            sock_fd,
            "SYSINFO\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* EXEC DATE */
    if (send_command(
            sock_fd,
            "EXEC DATE\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* Security test */
    if (send_command(
            sock_fd,
            "EXEC LS\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* PUT test file */
    if (upload_file(
            sock_fd,
            "upload_test.txt",
            "upload_test.txt") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* QUIT */
    if (send_command(
            sock_fd,
            "QUIT\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    close(sock_fd);

    return 0;
}
