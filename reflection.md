# AI Usage Reflection

During the development of the RemoteOps assignment, I used ChatGPT as an AI-assisted learning and development tool. I mainly used it to understand the assignment requirements, plan the implementation, explain networking concepts, identify problems in my code, and improve documentation. However, I did not simply accept the generated suggestions. I compiled, tested, reviewed, and modified the implementation according to the official assignment specification.

At the beginning, AI helped me break the assignment into smaller development stages. These included TCP socket setup, authentication, SYSINFO, LISTPROC, EXEC, file transfer, concurrency, UDP monitoring, logging, and testing. This approach made the implementation easier to understand and allowed me to test each feature before moving to the next stage.

AI was particularly useful when implementing TCP communication. I learned that TCP is a byte-stream protocol and that one send() call does not necessarily correspond to one recv() call. Therefore, I implemented line-based command framing and functions for sending and receiving exact amounts of data. This was especially important for the PUT and GET commands because file contents are transferred as raw bytes.

I also used AI guidance when implementing pthread-based concurrency. The Agent creates a separate thread for each Controller connection, allowing multiple Controllers to communicate with the server simultaneously. I tested this with five concurrent Controller connections. For UDP monitoring, I implemented periodic system-information datagrams with a five-second interval and tested MONITOR START and MONITOR STOP.

One important lesson was that AI output must always be checked against the assignment specification. During the EXEC implementation, an initial suggested response format did not exactly match the required protocol. After comparing it with the assignment brief, I corrected the successful response to use "OK EXEC_RESULT <output> SID:<sid>". This showed me that AI suggestions can be useful but are not automatically correct.

I verified the implementation through practical testing on CentOS. I tested successful and failed authentication, SYSINFO, LISTPROC, all permitted EXEC commands, rejected commands, PUT and GET transfers, missing files, concurrent clients, UDP monitoring, logging, and graceful QUIT. I also used cmp to confirm that the uploaded and downloaded test files were identical.

Overall, AI helped me work more systematically, but the most valuable part of the assignment was testing and understanding the generated code myself. Through this project, I improved my understanding of TCP framing, file transfer, multithreading, UDP communication, command whitelisting, logging, and practical socket programming in C.
