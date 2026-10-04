# IE3090 RemoteOps - AI Prompt Log

**Registration Number:** IT24102705  
**Module:** IE3090 - Network Programming  
**Assignment:** RemoteOps  
**AI Tool Used:** ChatGPT  
**Usage:** Part 1 implementation support, debugging, explanation and documentation

---

## Interaction 1 - Understanding the Assignment

**Date:** 04 October 2026

**Prompt / Request Summary:**  
Asked ChatGPT to explain the RemoteOps assignment requirements and guide the implementation step by step.

**AI Assistance:**  
ChatGPT helped identify the required Agent and Controller programs, TCP control channel, UDP monitoring, authentication, SYSINFO, LISTPROC, EXEC, PUT, GET, concurrency, logging and personalized values.

**How I Used the Output:**  
I used the explanation as an implementation plan and checked the required protocol formats against the assignment specification.

---

## Interaction 2 - Personalized Configuration

**Date:** 04 October 2026

**Prompt / Request Summary:**  
Asked for help calculating and applying the personalized values based on registration number IT24102705.

**AI Assistance:**  
The following values were identified:

- TCP Port: 9410
- Authentication Token: OPS-2705
- Session ID: 5072
- Agent file: agent_705.c
- Controller file: controller_705.c
- Makefile: Makefile_705
- Log file: remoteops_IT24102705.log
- Storage directory: ./agentfiles/IT24102705/

**How I Used the Output:**  
I manually applied these values to the source code and tested them on CentOS.

---

## Interaction 3 - TCP Connection and Authentication

**Date:** 04 October 2026

**Prompt / Request Summary:**  
Asked for step-by-step help implementing the TCP Agent, Controller and authentication.

**AI Assistance:**  
ChatGPT provided guidance for socket(), bind(), listen(), accept(), connect(), send() and recv(), and helped implement the AUTH protocol.

**How I Used the Output:**  
I entered the code into my own project, compiled it using GCC and tested the Agent and Controller locally.

---

## Interaction 4 - SYSINFO and LISTPROC

**Date:** 04 October 2026

**Prompt / Request Summary:**  
Asked for help implementing the SYSINFO and LISTPROC commands.

**AI Assistance:**  
ChatGPT suggested using Linux system information for SYSINFO and popen() with ps for the process snapshot.

**How I Used the Output:**  
I compiled and tested both commands and verified that the responses contained SID:5072.

---

## Interaction 5 - EXEC Whitelist

**Date:** 04 October 2026

**Prompt / Request Summary:**  
Asked for help implementing controlled remote command execution.

**AI Assistance:**  
ChatGPT helped implement a fixed whitelist containing DATE, UPTIME, DISKFREE, HOSTNAME and WHOAMI.

An unsupported command such as EXEC LS was configured to return:

ERR 002 COMMAND_NOT_ALLOWED SID:5072

**Correction / Modification:**  
During development, the EXEC success response format needed to be corrected to match the assignment specification exactly:

OK EXEC_RESULT <output> SID:5072

I checked the assignment requirement and used the corrected protocol format.

**How I Used the Output:**  
I tested both an allowed command and a rejected command.

---

## Interaction 6 - TCP Framing and PUT

**Date:** 04 October 2026

**Prompt / Request Summary:**  
Asked for help implementing reliable PUT file transfer over TCP.

**AI Assistance:**  
ChatGPT explained that TCP is a byte stream and that one send() call is not guaranteed to equal one recv() call.

A buffered line reader, send_all() and exact-byte file receiving logic were used.

**How I Used the Output:**  
I tested the implementation using upload_test.txt containing test data. The Agent successfully stored the 40-byte file in the personalized storage directory.

---

## Interaction 7 - GET File Transfer

**Date:** 04 October 2026

**Prompt / Request Summary:**  
Asked for help implementing GET while preserving the existing working code.

**AI Assistance:**  
ChatGPT helped implement the required response:

OK FILE_SEND <filename> <filesize> SID:5072

followed by exactly the declared number of raw bytes.

**How I Used the Output:**  
I downloaded upload_test.txt as downloaded_test.txt and used the Linux cmp command to verify that the original and downloaded files were byte-for-byte identical.

---

## Interaction 8 - Concurrent Controllers

**Date:** 04 October 2026

**Prompt / Request Summary:**  
Asked for help modifying the Agent to support at least five simultaneous Controllers.

**AI Assistance:**  
ChatGPT suggested a thread-per-connection design using POSIX pthreads.

Each Controller connection was given independent authentication and connection state.

**How I Used the Output:**  
I compiled the Agent with -pthread and tested five simultaneous TCP connections. All five connections successfully authenticated and requested SYSINFO.

---

## Interaction 9 - UDP Monitoring

**Date:** 04 October 2026

**Prompt / Request Summary:**  
Asked for help implementing MONITOR START and MONITOR STOP using UDP.

**AI Assistance:**  
ChatGPT helped implement a per-session UDP monitoring thread and a Controller UDP socket.

A monitoring interval of 5 seconds was selected.

**How I Used the Output:**  
I tested MONITOR START 12000 and received three periodic UDP SYSINFO messages before sending MONITOR STOP.

---

## Interaction 10 - Activity Logging

**Date:** 04 October 2026

**Prompt / Request Summary:**  
Asked for help implementing the personalized Agent log file.

**AI Assistance:**  
ChatGPT suggested a timestamped logging function protected by a pthread mutex to avoid concurrent writes from multiple Controller threads.

**How I Used the Output:**  
I tested the log file remoteops_IT24102705.log and verified entries for connections, authentication, commands, PUT, GET, UDP monitoring, QUIT and disconnection.

The authentication token itself was not written to the activity log.

---

## Interaction 11 - Makefile and Documentation

**Date:** 04 October 2026

**Prompt / Request Summary:**  
Asked for help checking the personalized Makefile and project documentation.

**AI Assistance:**  
ChatGPT helped create Makefile_705 with GCC warning flags and pthread support and helped structure README.md and design_diary.md.

**How I Used the Output:**  
I tested the Makefile using:

make -f Makefile_705

and reviewed the generated documentation against the implemented features.

---

## Overall AI Usage

ChatGPT was used as a development assistant for explanations, code suggestions, debugging guidance and documentation structure.

The generated suggestions were not treated as automatically correct. I compiled and tested the implementation on CentOS, compared protocol responses with the assignment specification, corrected issues where necessary, and collected execution evidence.

The implementation was developed incrementally and stored in Git with meaningful commits.
