# Elysian Web Server
Extremely lightweight, full HTTP Server implementation for embedded devices. 

Elysian is a free, open source, lightweight HTTP server implementation written entirely in C. 
The focus of the implementation is to reduce resource usage while still providing an  almost 
full-scale HTTP Server. This makes it suitable for use in memory-constrained  embedded devices 
where memory and CPU power are at a premium. 

Much emphasis is given on ease of use, easy integration and power consumption.

Elysian does not aim to be an alternative to other Web Server implementations designed to
serve thousands of requests (like Apache, NGINX or Lighttpd), but rather focuses on 
embedded microcontrollers, which typically are equipped with some tens of KB of RAM. 
Given that, high level server side scripting languages like PHP, ASP and JAVA are not 
and will not be supported.


Supported features include:
- Extremely lightweight: requires ~60kb FLASH and ~5kb RAM.
- Ported OS enviroments: FreeRtos, ChibiOS, Unix and Windows
- Ported TCP\IP enviroments: LwIP, Unix and Windows
- Ported filesystems: FatFS, Unix and Windows
- Support for dynamic content using the model-view-controller (MVC) design pattern
- Single thread implementation
- Extremely robust: all temporary failed operations (due to memory or network unavailability)
   are executed again with exponential backoff. Other clients are serviced normally in parallel.
- Support for GET/HEAD/POST/PUT HTTP requests
- Multiple client support
- HTTP pipelining and keep-alive connections
- Multi-partition scheme for file storage: Seperate partitions for files in ROM and external disk are used.
- Bounded memory usage


# Give it a try:
- Set your enviroment in elysian_config.h (choose from ELYSIAN_ENV_UNIX, ELYSIAN_ENV_WINDOWS, ELYSIAN_ENV_EMBEDDED). 
   Default is ELYSIAN_ENV_UNIX (could be also run from Cygwin).
- Issue "make". Image size may be big for embedded devices (~200 Kbytes), but that's because the default demo app contains images and audio files.
- Run the "elysian.out" executable.
- Open a browser and navigate to "http://localhost:9000"

# Samples:
![alt tag](https://raw.githubusercontent.com/npoulokefalos/Elysian-Web-Server/master/sample/sample.png)