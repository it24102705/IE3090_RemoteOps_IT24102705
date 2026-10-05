
# RemoteOps: Remote System Monitoring and Management Tool

## IE3090 - Network Programming

**Student Registration Number:** IT24102705  
**Project:** RemoteOps  
**Platform:** Linux / CentOS  
**Language:** C  
**TCP Port:** 9410  
**Session ID:** 5072  

---

## 1. Project Overview

RemoteOps is a client-server remote system monitoring and management tool developed using C and BSD sockets.

The system consists of two main programs:

- **Agent** - acts as the server.
- **Controller** - acts as the client.

The Agent accepts TCP connections from Controllers and supports authentication, system monitoring, process listing, controlled command execution, file transfer, UDP monitoring, activity logging, and concurrent Controller connections.

---

## 2. Personalized Configuration

The implementation uses the following personalized values for registration number IT24102705:

- TCP Port: `9410`
- Authentication Token: `OPS-2705`
- Session ID: `5072`
- Agent Source: `agent_705.c`
- Controller Source: `controller_705.c`
- Makefile: `Makefile_705`
- Log File: `remoteops_IT24102705.log`
- Agent Storage Directory: `./agentfiles/IT24102705/`

---

## 3. Implemented Features

The RemoteOps implementation supports:

- TCP client-server communication
- Per-connection authentication
- SYSINFO system information retrieval
- LISTPROC process listing
- EXEC command whitelist
- PUT file upload
- GET file download
- Exact byte-based TCP file transfer
- Multiple concurrent Controllers using POSIX threads
- UDP-based periodic system monitoring
- Thread-safe timestamped activity logging
- Graceful QUIT handling
- Error handling for unsupported commands
- Personalized Agent storage

---

## 4. EXEC Command Whitelist

For security, the Agent only allows the following EXEC commands:

- `DATE`
- `UPTIME`
- `DISKFREE`
- `HOSTNAME`
- `WHOAMI`

Any unsupported command is rejected.

Example:

```text
EXEC LS
ERR 002 COMMAND_NOT_ALLOWED SID:5072
## Final Testing

Final testing was performed on the RemoteOps implementation.
The Agent and Controller were tested for authentication, SYSINFO,
LISTPROC, EXEC, PUT, GET, concurrent client connections, UDP monitoring,
logging, and graceful disconnection.

## Functional Test Checklist

- AUTH: Valid authentication token accepted successfully.
- SYSINFO: CPU load, memory usage and uptime returned correctly.
- LISTPROC: Running process information returned successfully.
- EXEC: Whitelisted commands execute and invalid commands are rejected.
- PUT: Test file uploaded successfully to the personalised storage directory.
- GET: Uploaded test file downloaded successfully.
- Concurrency: Multiple Controller connections handled simultaneously.
- UDP Monitoring: MONITOR START receives periodic system information and MONITOR STOP stops the stream.
- Logging: Connections, commands and file transfers are recorded in the personalised log file.
- QUIT: Controller disconnects cleanly without crashing the Agent.
