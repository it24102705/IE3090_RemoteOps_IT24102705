# RemoteOps Design Diary

**Registration Number:** IT24102705  
**Module:** IE3090 - Network Programming  
**Project:** RemoteOps  
**Language:** C  
**Platform:** Linux / CentOS  

---

## 1. Initial Project Setup

The RemoteOps project was created using the personalized values calculated from registration number IT24102705.

Personalized configuration:

- TCP Port: 9410
- Authentication Token: OPS-2705
- Session ID: 5072
- Agent Source: agent_705.c
- Controller Source: controller_705.c
- Makefile: Makefile_705
- Log File: remoteops_IT24102705.log
- Storage Directory: ./agentfiles/IT24102705/

The initial project structure was created before implementing networking features.

---

## 2. TCP Connection and Authentication

The first networking stage implemented the TCP connection between the Controller and Agent.

The Agent uses a TCP socket and performs:

1. socket()
2. bind()
3. listen()
4. accept()

The Controller performs:

1. socket()
2. connect()

The first command in a Controller session must be:

AUTH OPS-2705

Successful authentication returns:

OK AUTHENTICATED SID:5072

Authentication state is maintained separately for every Controller connection.

---

## 3. TCP Message Framing

TCP is a byte-stream protocol, so one send() call is not guaranteed to match one recv() call.

A buffered TCP reader was therefore implemented on the Agent.

Text commands are processed as newline-terminated lines.

For file transfers, the implementation switches from text processing to exact byte processing.

This prevents raw file data from being incorrectly interpreted as protocol commands.

---

## 4. SYSINFO

SYSINFO was implemented using Linux system information.

The Agent returns:

OK SYSINFO <cpu_load> <mem_used_mb> <uptime_sec> SID:5072

The system information function was later reused by UDP monitoring.

---

## 5. LISTPROC

LISTPROC was implemented using popen() with the Linux ps command.

The Agent obtains a process snapshot and returns the process information using the required PROCS response format.

---

## 6. Controlled Remote Execution

EXEC was implemented using a fixed whitelist.

Only the following commands are accepted:

- DATE
- UPTIME
- DISKFREE
- HOSTNAME
- WHOAMI

The Agent maps these protocol commands to predefined Linux commands.

Arbitrary commands supplied by a Controller are never directly passed to the shell.

For example:

EXEC LS

is rejected with:

ERR 002 COMMAND_NOT_ALLOWED SID:5072

This design reduces the risk of arbitrary remote command execution.

---

## 7. PUT File Upload

PUT was implemented to transfer files from the Controller to the Agent.

The Controller first sends:

PUT <filename> <filesize>

It then immediately sends exactly <filesize> raw bytes.

The Agent stores uploaded files in:

./agentfiles/IT24102705/

A 10 MB maximum upload size was selected for this implementation.

Filename validation was also added to reduce directory traversal risks.

---

## 8. GET File Download

GET was implemented to transfer a stored file from the Agent to the Controller.

The Agent sends:

OK FILE_SEND <filename> <filesize> SID:5072

followed immediately by exactly <filesize> raw bytes.

The implementation was tested using upload_test.txt.

The downloaded file was compared with the original file using cmp to verify that the transfer was byte-for-byte identical.

---

## 9. Concurrent Controller Support

The first Agent implementation handled one Controller at a time.

This was later changed to a thread-per-connection architecture using POSIX pthreads.

The main Agent thread accepts connections, while each Controller is processed by a separate worker thread.

Each Controller thread maintains its own:

- socket
- authentication state
- TCP receive buffer
- UDP monitoring state

The Agent was tested with five simultaneous Controller connections.

---

## 10. UDP Monitoring

UDP was added for periodic monitoring while TCP remains the control channel.

The Controller sends:

MONITOR START <udp_port>

The Agent creates a UDP monitoring thread for that Controller session.

Monitoring messages contain:

SYSINFO <cpu_load> <mem_used_mb> <uptime_sec> SID:5072

A monitoring interval of 5 seconds was selected.

The Controller can stop monitoring using:

MONITOR STOP

The Agent also stops monitoring automatically when the session ends.

---

## 11. Activity Logging

Thread-safe logging was added using a pthread mutex.

The personalized log file is:

remoteops_IT24102705.log

The log records timestamps for events including:

- Agent startup
- Controller connections
- authentication
- commands
- PUT transfers
- GET transfers
- UDP monitoring
- QUIT
- disconnections

The authentication token itself is not written to the activity log.

The mutex prevents multiple Controller threads from writing to the log file at the same time.

---

## 12. Graceful Connection Handling

QUIT returns:

OK BYE SID:5072

Before closing a Controller session, any active UDP monitoring is stopped.

SIGPIPE is ignored by the Agent so that an unexpected Controller disconnection during a send operation does not terminate the entire Agent process.

---

## 13. Build Process

A personalized Makefile named Makefile_705 is used.

The project can be built using:

make -f Makefile_705

The Agent requires pthread support and is compiled with:

-Wall -Wextra -pthread

---

## 14. Testing Performed

The implementation has been tested for:

- TCP connection
- successful authentication
- SYSINFO
- LISTPROC
- allowed EXEC command
- rejected EXEC command
- PUT upload
- GET download
- byte-for-byte file verification
- five simultaneous Controller connections
- UDP monitoring start
- periodic UDP SYSINFO messages
- UDP monitoring stop
- timestamped logging
- QUIT and connection cleanup

Screenshots were collected during testing for use as implementation evidence in the final report.

---

## 15. Final Architecture

The final RemoteOps architecture uses:

- TCP for reliable commands and responses
- TCP exact-byte transfer for PUT and GET
- UDP for periodic monitoring
- POSIX threads for concurrent Controllers
- per-session authentication
- per-session UDP monitoring state
- mutex-protected logging
- personalized file storage

This design separates reliable control operations from lightweight periodic monitoring while allowing multiple Controllers to use the Agent concurrently.
