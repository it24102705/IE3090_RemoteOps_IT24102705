#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>

#define PORT 9410
#define AUTH_TOKEN "OPS-2705"
#define SID "5072"

#define BUFFER_SIZE 4096
#define STORAGE_DIR "./agentfiles/IT24102705"
#define LOG_FILE "remoteops_IT24102705.log"
#define MAX_FILE_SIZE (10 * 1024 * 1024)

#define MONITOR_INTERVAL 5

/* =================================================
   GLOBAL LOG MUTEX
   ================================================= */
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

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
   UDP MONITOR INFORMATION
   ================================================= */
typedef struct
{
    pthread_t thread_id;
    pthread_mutex_t mutex;

    int active;
    int thread_running;

    struct in_addr controller_ip;
    int udp_port;
} MonitorContext;

/* =================================================
   CONTROLLER SESSION
   ================================================= */
typedef struct
{
    int client_fd;
    struct sockaddr_in client_addr;

    MonitorContext monitor;
} ClientSession;

/* =================================================
   THREAD-SAFE LOGGING
   ================================================= */
void log_event(const char *format, ...)
{
    FILE *log_file;

    time_t now;
    struct tm time_info;

    char timestamp[64];

    time(&now);

    localtime_r(
        &now,
        &time_info
    );

    strftime(
        timestamp,
        sizeof(timestamp),
        "%Y-%m-%d %H:%M:%S",
        &time_info
    );

    pthread_mutex_lock(
        &log_mutex
    );

    log_file =
        fopen(
            LOG_FILE,
            "a"
        );

    if (log_file != NULL)
    {
        fprintf(
            log_file,
            "[%s] ",
            timestamp
        );

        va_list args;

        va_start(
            args,
            format
        );

        vfprintf(
            log_file,
            format,
            args
        );

        va_end(args);

        fprintf(
            log_file,
            "\n"
        );

        fclose(log_file);
    }

    pthread_mutex_unlock(
        &log_mutex
    );
}

/* =================================================
   SEND ALL TCP BYTES
   ================================================= */
int send_all(
    int fd,
    const void *data,
    size_t length)
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

        total +=
            (size_t)sent;
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
                reader->buffer[
                    reader->start++
                ];

            if (c == '\n')
            {
                if (line_pos > 0 &&
                    line[line_pos - 1] == '\r')
                {
                    line_pos--;
                }

                line[line_pos] = '\0';

                return
                    (ssize_t)line_pos;
            }

            if (line_pos + 1 >= line_size)
            {
                return -2;
            }

            line[line_pos++] =
                (char)c;
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
        reader->end =
            (size_t)received;
    }
}

/* =================================================
   READ EXACT RAW BYTES FOR PUT
   ================================================= */
int read_exact(
    ConnReader *reader,
    FILE *file,
    size_t total_bytes)
{
    size_t remaining =
        total_bytes;

    while (remaining > 0)
    {
        if (reader->start < reader->end)
        {
            size_t available =
                reader->end -
                reader->start;

            size_t amount =
                available < remaining
                    ? available
                    : remaining;

            if (fwrite(
                    reader->buffer
                        + reader->start,
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

        unsigned char temp[
            BUFFER_SIZE
        ];

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
                file) !=
            (size_t)received)
        {
            return -1;
        }

        remaining -=
            (size_t)received;
    }

    return 0;
}

/* =================================================
   SAFE FILENAME CHECK
   ================================================= */
int valid_filename(
    const char *filename)
{
    if (filename == NULL ||
        filename[0] == '\0')
    {
        return 0;
    }

    if (strstr(
            filename,
            "..") != NULL)
    {
        return 0;
    }

    if (strchr(
            filename,
            '/') != NULL ||
        strchr(
            filename,
            '\\') != NULL)
    {
        return 0;
    }

    return 1;
}

/* =================================================
   SYSTEM INFORMATION
   ================================================= */
int get_system_info(
    double *cpu_load,
    unsigned long *mem_used_mb,
    long *uptime_sec)
{
    struct sysinfo info;

    if (sysinfo(&info) != 0)
    {
        return -1;
    }

    *cpu_load =
        (double)info.loads[0] /
        65536.0;

    unsigned long total_ram =
        info.totalram *
        info.mem_unit;

    unsigned long free_ram =
        info.freeram *
        info.mem_unit;

    *mem_used_mb =
        (total_ram - free_ram) /
        (1024 * 1024);

    *uptime_sec =
        info.uptime;

    return 0;
}

/* =================================================
   SYSINFO
   ================================================= */
void handle_sysinfo(
    int client_fd)
{
    double cpu_load;
    unsigned long mem_used_mb;
    long uptime_sec;

    char response[BUFFER_SIZE];

    if (get_system_info(
            &cpu_load,
            &mem_used_mb,
            &uptime_sec) != 0)
    {
        send_response(
            client_fd,
            "ERR 006 SYSINFO_FAILED SID:"
            SID
            "\n"
        );

        return;
    }

    snprintf(
        response,
        sizeof(response),
        "OK SYSINFO %.2f %lu %ld SID:%s\n",
        cpu_load,
        mem_used_mb,
        uptime_sec,
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
void handle_listproc(
    int client_fd)
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
            "ERR 007 LISTPROC_FAILED SID:"
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
            strcspn(
                line,
                "\r\n"
            )
        ] = '\0';

        if (strlen(response)
            + strlen(line)
            + strlen(
                " SID:" SID "\n"
            )
            + 2
            >= sizeof(response))
        {
            break;
        }

        strcat(
            response,
            line
        );

        strcat(
            response,
            ","
        );
    }

    pclose(fp);

    size_t len =
        strlen(response);

    if (len > 0 &&
        response[len - 1] == ',')
    {
        response[len - 1] =
            '\0';
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
    const char *shell_command =
        NULL;

    FILE *fp;

    char line[256];
    char output[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    if (strcmp(
            exec_command,
            "DATE") == 0)
    {
        shell_command =
            "date";
    }
    else if (strcmp(
                 exec_command,
                 "UPTIME") == 0)
    {
        shell_command =
            "uptime";
    }
    else if (strcmp(
                 exec_command,
                 "DISKFREE") == 0)
    {
        shell_command =
            "df -h";
    }
    else if (strcmp(
                 exec_command,
                 "HOSTNAME") == 0)
    {
        shell_command =
            "hostname";
    }
    else if (strcmp(
                 exec_command,
                 "WHOAMI") == 0)
    {
        shell_command =
            "whoami";
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
            strcspn(
                line,
                "\r\n"
            )
        ] = '\0';

        if (strlen(output)
            + strlen(line)
            + 2
            >= sizeof(output))
        {
            break;
        }

        strcat(
            output,
            line
        );

        strcat(
            output,
            " "
        );
    }

    pclose(fp);

    size_t len =
        strlen(output);

    if (len > 0 &&
        output[len - 1] == ' ')
    {
        output[len - 1] =
            '\0';
    }

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
   ================================================= */
void handle_put(
    int client_fd,
    ConnReader *reader,
    const char *filename,
    size_t filesize)
{
    char filepath[512];
    char response[BUFFER_SIZE];

    if (filesize > MAX_FILE_SIZE)
    {
        send_response(
            client_fd,
            "ERR 004 FILE_TOO_LARGE SID:"
            SID
            "\n"
        );

        log_event(
            "PUT rejected: %s is too large (%zu bytes)",
            filename,
            filesize
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

        log_event(
            "PUT rejected: invalid filename"
        );

        return;
    }

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

        log_event(
            "PUT failed: could not create %s",
            filename
        );

        return;
    }

    if (read_exact(
            reader,
            file,
            filesize) != 0)
    {
        fclose(file);

        remove(filepath);

        send_response(
            client_fd,
            "ERR 010 FILE_WRITE_FAILED SID:"
            SID
            "\n"
        );

        log_event(
            "PUT failed during transfer: %s",
            filename
        );

        return;
    }

    fclose(file);

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

    log_event(
        "PUT completed: %s (%zu bytes)",
        filename,
        filesize
    );
}

/* =================================================
   GET
   ================================================= */
void handle_get(
    int client_fd,
    const char *filename)
{
    char filepath[512];
    char response[BUFFER_SIZE];

    unsigned char buffer[
        BUFFER_SIZE
    ];

    struct stat file_info;

    if (!valid_filename(filename))
    {
        send_response(
            client_fd,
            "ERR 005 FILE_NOT_FOUND SID:"
            SID
            "\n"
        );

        log_event(
            "GET rejected: invalid filename"
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

        log_event(
            "GET failed: file not found %s",
            filename
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

        log_event(
            "GET failed: unable to open %s",
            filename
        );

        return;
    }

    snprintf(
        response,
        sizeof(response),
        "OK FILE_SEND %s %lld SID:%s\n",
        filename,
        (long long)
            file_info.st_size,
        SID
    );

    if (send_all(
            client_fd,
            response,
            strlen(response)) < 0)
    {
        fclose(file);

        log_event(
            "GET failed while sending header: %s",
            filename
        );

        return;
    }

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

            log_event(
                "GET interrupted: %s",
                filename
            );

            return;
        }
    }

    fclose(file);

    printf(
        "File sent: %s (%lld bytes)\n",
        filename,
        (long long)
            file_info.st_size
    );

    log_event(
        "GET completed: %s (%lld bytes)",
        filename,
        (long long)
            file_info.st_size
    );
}

/* =================================================
   CHECK MONITOR STATE
   ================================================= */
int monitor_is_active(
    MonitorContext *monitor)
{
    int active;

    pthread_mutex_lock(
        &monitor->mutex
    );

    active =
        monitor->active;

    pthread_mutex_unlock(
        &monitor->mutex
    );

    return active;
}

/* =================================================
   UDP MONITOR THREAD
   ================================================= */
void *udp_monitor_thread(
    void *arg)
{
    MonitorContext *monitor =
        (MonitorContext *)arg;

    int udp_fd =
        socket(
            AF_INET,
            SOCK_DGRAM,
            0
        );

    if (udp_fd < 0)
    {
        perror("UDP socket");

        pthread_mutex_lock(
            &monitor->mutex
        );

        monitor->active = 0;
        monitor->thread_running = 0;

        pthread_mutex_unlock(
            &monitor->mutex
        );

        return NULL;
    }

    struct sockaddr_in destination;

    memset(
        &destination,
        0,
        sizeof(destination)
    );

    destination.sin_family =
        AF_INET;

    destination.sin_addr =
        monitor->controller_ip;

    destination.sin_port =
        htons(
            (unsigned short)
                monitor->udp_port
        );

    while (monitor_is_active(
               monitor))
    {
        double cpu_load;

        unsigned long mem_used_mb;

        long uptime_sec;

        char message[BUFFER_SIZE];

        if (get_system_info(
                &cpu_load,
                &mem_used_mb,
                &uptime_sec) == 0)
        {
            snprintf(
                message,
                sizeof(message),
                "SYSINFO %.2f %lu %ld SID:%s",
                cpu_load,
                mem_used_mb,
                uptime_sec,
                SID
            );

            sendto(
                udp_fd,
                message,
                strlen(message),
                0,
                (struct sockaddr *)
                    &destination,
                sizeof(destination)
            );
        }

        for (int i = 0;
             i < MONITOR_INTERVAL;
             i++)
        {
            if (!monitor_is_active(
                    monitor))
            {
                break;
            }

            sleep(1);
        }
    }

    close(udp_fd);

    pthread_mutex_lock(
        &monitor->mutex
    );

    monitor->thread_running =
        0;

    pthread_mutex_unlock(
        &monitor->mutex
    );

    return NULL;
}

/* =================================================
   MONITOR START
   ================================================= */
void handle_monitor_start(
    int client_fd,
    ClientSession *session,
    int udp_port)
{
    if (udp_port < 1 ||
        udp_port > 65535)
    {
        send_response(
            client_fd,
            "ERR 012 INVALID_UDP_PORT SID:"
            SID
            "\n"
        );

        log_event(
            "MONITOR START rejected: invalid UDP port %d",
            udp_port
        );

        return;
    }

    pthread_mutex_lock(
        &session->monitor.mutex
    );

    if (session->monitor.active ||
        session->monitor.thread_running)
    {
        pthread_mutex_unlock(
            &session->monitor.mutex
        );

        send_response(
            client_fd,
            "ERR 013 MONITOR_ALREADY_RUNNING SID:"
            SID
            "\n"
        );

        return;
    }

    session->monitor.active = 1;

    session->monitor.thread_running =
        1;

    session->monitor.controller_ip =
        session->client_addr.sin_addr;

    session->monitor.udp_port =
        udp_port;

    pthread_mutex_unlock(
        &session->monitor.mutex
    );

    if (pthread_create(
            &session->monitor.thread_id,
            NULL,
            udp_monitor_thread,
            &session->monitor) != 0)
    {
        pthread_mutex_lock(
            &session->monitor.mutex
        );

        session->monitor.active = 0;
        session->monitor.thread_running = 0;

        pthread_mutex_unlock(
            &session->monitor.mutex
        );

        send_response(
            client_fd,
            "ERR 014 MONITOR_START_FAILED SID:"
            SID
            "\n"
        );

        log_event(
            "MONITOR START failed on UDP port %d",
            udp_port
        );

        return;
    }

    send_response(
        client_fd,
        "OK MONITOR_STARTED SID:"
        SID
        "\n"
    );

    printf(
        "UDP monitor started for port %d.\n",
        udp_port
    );

    log_event(
        "UDP monitoring started on port %d",
        udp_port
    );
}

/* =================================================
   MONITOR STOP
   ================================================= */
void stop_monitor(
    int client_fd,
    ClientSession *session,
    int send_reply)
{
    int should_join = 0;

    pthread_mutex_lock(
        &session->monitor.mutex
    );

    if (session->monitor.active ||
        session->monitor.thread_running)
    {
        session->monitor.active = 0;
        should_join = 1;
    }

    pthread_mutex_unlock(
        &session->monitor.mutex
    );

    if (should_join)
    {
        pthread_join(
            session->monitor.thread_id,
            NULL
        );
    }

    if (send_reply)
    {
        send_response(
            client_fd,
            "OK MONITOR_STOPPED SID:"
            SID
            "\n"
        );

        log_event(
            "UDP monitoring stopped"
        );
    }
}

/* =================================================
   CONTROLLER THREAD
   ================================================= */
void *handle_client(
    void *arg)
{
    ClientSession *session =
        (ClientSession *)arg;

    int client_fd =
        session->client_fd;

    char client_ip[
        INET_ADDRSTRLEN
    ];

    inet_ntop(
        AF_INET,
        &session->client_addr.sin_addr,
        client_ip,
        sizeof(client_ip)
    );

    printf(
        "\nController connected from %s\n",
        client_ip
    );

    log_event(
        "Controller connected from %s",
        client_ip
    );

    int authenticated = 0;

    ConnReader reader;

    reader.fd = client_fd;
    reader.start = 0;
    reader.end = 0;

    char command[BUFFER_SIZE];

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
                "Controller %s disconnected.\n",
                client_ip
            );

            log_event(
                "Controller %s disconnected",
                client_ip
            );

            break;
        }

        if (result < 0)
        {
            printf(
                "Connection/read error from %s.\n",
                client_ip
            );

            log_event(
                "Connection/read error from %s",
                client_ip
            );

            break;
        }

        printf(
            "[%s] Received: %s\n",
            client_ip,
            command
        );

        /*
           Do not store the authentication token
           itself in the log file.
        */
        if (strncmp(
                command,
                "AUTH ",
                5) == 0)
        {
            log_event(
                "Command from %s: AUTH",
                client_ip
            );
        }
        else
        {
            log_event(
                "Command from %s: %s",
                client_ip,
                command
            );
        }

        /* =========================================
           AUTH
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
                    "[%s] Authentication successful.\n",
                    client_ip
                );

                log_event(
                    "Authentication successful for %s",
                    client_ip
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
                    "[%s] Authentication failed.\n",
                    client_ip
                );

                log_event(
                    "Authentication failed for %s",
                    client_ip
                );
            }

            continue;
        }

        /* SYSINFO */
        if (strcmp(
                command,
                "SYSINFO"
            ) == 0)
        {
            handle_sysinfo(
                client_fd
            );
        }

        /* LISTPROC */
        else if (strcmp(
                     command,
                     "LISTPROC"
                 ) == 0)
        {
            handle_listproc(
                client_fd
            );
        }

        /* EXEC */
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

        /* PUT */
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

        /* GET */
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

        /* MONITOR START */
        else if (strncmp(
                     command,
                     "MONITOR START ",
                     14
                 ) == 0)
        {
            int udp_port;

            if (sscanf(
                    command + 14,
                    "%d",
                    &udp_port) == 1)
            {
                handle_monitor_start(
                    client_fd,
                    session,
                    udp_port
                );
            }
            else
            {
                send_response(
                    client_fd,
                    "ERR 012 INVALID_UDP_PORT SID:"
                    SID
                    "\n"
                );
            }
        }

        /* MONITOR STOP */
        else if (strcmp(
                     command,
                     "MONITOR STOP"
                 ) == 0)
        {
            stop_monitor(
                client_fd,
                session,
                1
            );
        }

        /* QUIT */
        else if (strcmp(
                     command,
                     "QUIT"
                 ) == 0)
        {
            stop_monitor(
                client_fd,
                session,
                0
            );

            send_response(
                client_fd,
                "OK BYE SID:"
                SID
                "\n"
            );

            printf(
                "[%s] Controller requested QUIT.\n",
                client_ip
            );

            log_event(
                "Controller %s requested QUIT",
                client_ip
            );

            break;
        }

        /* UNKNOWN */
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

    stop_monitor(
        client_fd,
        session,
        0
    );

    close(client_fd);

    pthread_mutex_destroy(
        &session->monitor.mutex
    );

    printf(
        "Controller %s connection closed.\n",
        client_ip
    );

    log_event(
        "Controller %s connection closed",
        client_ip
    );

    free(session);

    return NULL;
}

/* =================================================
   MAIN
   ================================================= */
int main(void)
{
    int server_fd;
    int opt = 1;

    struct sockaddr_in server_addr;

    signal(
        SIGPIPE,
        SIG_IGN
    );

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

    if (bind(
            server_fd,
            (struct sockaddr *)
                &server_addr,
            sizeof(server_addr)) < 0)
    {
        perror("bind");

        close(server_fd);

        return 1;
    }

    if (listen(
            server_fd,
            10) < 0)
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

    printf(
        "Concurrent Controller support enabled.\n"
    );

    printf(
        "UDP monitoring interval: %d seconds.\n",
        MONITOR_INTERVAL
    );

    printf(
        "Logging to: %s\n",
        LOG_FILE
    );

    log_event(
        "RemoteOps Agent started on TCP port %d",
        PORT
    );

    while (1)
    {
        ClientSession *session =
            malloc(
                sizeof(ClientSession)
            );

        if (session == NULL)
        {
            perror("malloc");
            continue;
        }

        memset(
            session,
            0,
            sizeof(ClientSession)
        );

        socklen_t client_len =
            sizeof(
                session->client_addr
            );

        session->client_fd =
            accept(
                server_fd,
                (struct sockaddr *)
                    &session->client_addr,
                &client_len
            );

        if (session->client_fd < 0)
        {
            perror("accept");

            free(session);

            continue;
        }

        pthread_mutex_init(
            &session->monitor.mutex,
            NULL
        );

        session->monitor.active =
            0;

        session->monitor.thread_running =
            0;

        pthread_t client_thread;

        if (pthread_create(
                &client_thread,
                NULL,
                handle_client,
                session) != 0)
        {
            perror("pthread_create");

            pthread_mutex_destroy(
                &session->monitor.mutex
            );

            close(
                session->client_fd
            );

            free(session);

            continue;
        }

        pthread_detach(
            client_thread
        );
    }

    close(server_fd);

    return 0;
}
