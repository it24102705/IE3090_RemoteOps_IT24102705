#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410
#define UDP_MONITOR_PORT 12000
#define BUFFER_SIZE 4096

/* =================================================
   SEND ALL TCP BYTES
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
   READ ONE TCP RESPONSE LINE
   ================================================= */
int read_response_line(
    int sock_fd,
    char *buffer,
    size_t buffer_size)
{
    size_t position = 0;

    while (position < buffer_size - 1)
    {
        char c;

        ssize_t received =
            recv(
                sock_fd,
                &c,
                1,
                0
            );

        if (received <= 0)
        {
            return -1;
        }

        buffer[position++] = c;

        if (c == '\n')
        {
            break;
        }
    }

    buffer[position] = '\0';

    return 0;
}

/* =================================================
   RECEIVE NORMAL TCP RESPONSE
   ================================================= */
int receive_response(int sock_fd)
{
    char buffer[BUFFER_SIZE];

    if (read_response_line(
            sock_fd,
            buffer,
            sizeof(buffer)) < 0)
    {
        printf(
            "Failed to receive response from Agent.\n"
        );

        return -1;
    }

    printf(
        "Agent response: %s",
        buffer
    );

    return 0;
}

/* =================================================
   SEND NORMAL TCP COMMAND
   ================================================= */
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

/* =================================================
   PUT
   ================================================= */
int upload_file(
    int sock_fd,
    const char *local_filename,
    const char *remote_filename)
{
    FILE *file;

    struct stat file_info;

    char header[512];

    unsigned char buffer[BUFFER_SIZE];

    if (stat(
            local_filename,
            &file_info) != 0)
    {
        perror("stat");
        return -1;
    }

    file =
        fopen(
            local_filename,
            "rb"
        );

    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }

    snprintf(
        header,
        sizeof(header),
        "PUT %s %lld\n",
        remote_filename,
        (long long)file_info.st_size
    );

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

    return receive_response(sock_fd);
}

/* =================================================
   GET
   ================================================= */
int download_file(
    int sock_fd,
    const char *remote_filename,
    const char *local_filename)
{
    char command[512];
    char response[BUFFER_SIZE];

    char received_filename[256];
    char sid[64];

    unsigned long long filesize;

    unsigned char buffer[BUFFER_SIZE];

    snprintf(
        command,
        sizeof(command),
        "GET %s\n",
        remote_filename
    );

    if (send_all(
            sock_fd,
            command,
            strlen(command)) < 0)
    {
        perror("send");
        return -1;
    }

    printf(
        "Sent: GET %s\n",
        remote_filename
    );

    if (read_response_line(
            sock_fd,
            response,
            sizeof(response)) < 0)
    {
        printf(
            "Failed to receive GET response.\n"
        );

        return -1;
    }

    printf(
        "Agent response: %s",
        response
    );

    if (strncmp(
            response,
            "ERR ",
            4) == 0)
    {
        return -1;
    }

    if (sscanf(
            response,
            "OK FILE_SEND %255s %llu %63s",
            received_filename,
            &filesize,
            sid) != 3)
    {
        printf(
            "Invalid GET response format.\n"
        );

        return -1;
    }

    FILE *file =
        fopen(
            local_filename,
            "wb"
        );

    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }

    unsigned long long remaining =
        filesize;

    while (remaining > 0)
    {
        size_t wanted =
            remaining < BUFFER_SIZE
                ? (size_t)remaining
                : BUFFER_SIZE;

        ssize_t received =
            recv(
                sock_fd,
                buffer,
                wanted,
                0
            );

        if (received <= 0)
        {
            printf(
                "File download interrupted.\n"
            );

            fclose(file);

            remove(local_filename);

            return -1;
        }

        if (fwrite(
                buffer,
                1,
                (size_t)received,
                file) != (size_t)received)
        {
            printf(
                "Failed to write downloaded file.\n"
            );

            fclose(file);

            remove(local_filename);

            return -1;
        }

        remaining -=
            (unsigned long long)received;
    }

    fclose(file);

    printf(
        "File downloaded: %s (%llu bytes)\n",
        local_filename,
        filesize
    );

    return 0;
}

/* =================================================
   UDP MONITOR TEST
   ================================================= */
int run_udp_monitor(int tcp_fd)
{
    int udp_fd;

    struct sockaddr_in udp_addr;

    char command[128];
    char buffer[BUFFER_SIZE];

    udp_fd =
        socket(
            AF_INET,
            SOCK_DGRAM,
            0
        );

    if (udp_fd < 0)
    {
        perror("UDP socket");
        return -1;
    }

    memset(
        &udp_addr,
        0,
        sizeof(udp_addr)
    );

    udp_addr.sin_family =
        AF_INET;

    udp_addr.sin_addr.s_addr =
        INADDR_ANY;

    udp_addr.sin_port =
        htons(UDP_MONITOR_PORT);

    if (bind(
            udp_fd,
            (struct sockaddr *)&udp_addr,
            sizeof(udp_addr)) < 0)
    {
        perror("UDP bind");

        close(udp_fd);

        return -1;
    }

    /*
       Prevent UDP receive from waiting forever.
    */
    struct timeval timeout;

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    setsockopt(
        udp_fd,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout)
    );

    snprintf(
        command,
        sizeof(command),
        "MONITOR START %d\n",
        UDP_MONITOR_PORT
    );

    if (send_command(
            tcp_fd,
            command) < 0)
    {
        close(udp_fd);
        return -1;
    }

    printf(
        "\nWaiting for UDP monitoring data...\n"
    );

    /*
       Receive three UDP SYSINFO datagrams.
       Agent interval = 5 seconds.
    */
    for (int i = 1; i <= 3; i++)
    {
        ssize_t received =
            recvfrom(
                udp_fd,
                buffer,
                sizeof(buffer) - 1,
                0,
                NULL,
                NULL
            );

        if (received < 0)
        {
            perror("UDP recvfrom");

            send_command(
                tcp_fd,
                "MONITOR STOP\n"
            );

            close(udp_fd);

            return -1;
        }

        buffer[received] = '\0';

        printf(
            "UDP Monitor #%d: %s\n",
            i,
            buffer
        );
    }

    printf("\n");

    if (send_command(
            tcp_fd,
            "MONITOR STOP\n") < 0)
    {
        close(udp_fd);
        return -1;
    }

    close(udp_fd);

    return 0;
}

/* =================================================
   MAIN
   ================================================= */
int main(void)
{
    int sock_fd;

    struct sockaddr_in server_addr;

    printf(
        "RemoteOps Controller - IT24102705\n"
    );

    sock_fd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (sock_fd < 0)
    {
        perror("socket");
        return 1;
    }

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
   
       /* LISTPROC */
    if (send_command(
            sock_fd,
            "LISTPROC\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");



    /* EXEC ALLOWED */
    if (send_command(
            sock_fd,
            "EXEC DATE\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* EXEC NOT ALLOWED */
    if (send_command(
            sock_fd,
            "EXEC LS\n") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* PUT */
    if (upload_file(
            sock_fd,
            "upload_test.txt",
            "upload_test.txt") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* GET */
    if (download_file(
            sock_fd,
            "upload_test.txt",
            "downloaded_test.txt") < 0)
    {
        close(sock_fd);
        return 1;
    }

    printf("\n");

    /* UDP MONITOR */
    if (run_udp_monitor(
            sock_fd) < 0)
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
