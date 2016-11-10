# Elysian Web Server
Extremely lightweight, full HTTP Server implementation for embedded devices and microcontrollers. 

Elysian is a free, open source, lightweight HTTP server implementation written entirely in C. 
The focus of the implementation is to reduce resource usage while still providing an  almost 
full-scale HTTP Server, making it suitable for use in memory-constrained  embedded devices 
where memory and CPU power are at a premium. The Web Server allows to serve user-friendly HTML pages 
with images and data enabling embedded monitoring and control functionality into the deployed TCP/IP 
enabled appliances. 

Elysian is not intended to be used as a feature-packed server; since it focuses strictly on 
embedded microcontrollers (typically equipped with some tens of KB of RAM), high level 
server side scripting languages (PHP, LUA) will not be supported. Dynamic content is supported
using C controllers, following the standards of MVC.

Supported features include:
- Small footprint: requires ~90kb FLASH and ~5kb RAM
- Ported OS enviroments: FreeRTOS, ChibiOS, Unix, Windows
- Ported TCP\IP enviroments: LwIP, Unix,Windows
- Ported filesystems: FatFS, Unix, Windows
- Support for dynamic content using the model-view-controller (MVC) design pattern
- Single thread implementation
- Extremely robust: all temporary failed operations (due to memory or network unavailability)
   are executed again with exponential backoff. Other clients are serviced normally in parallel
- Support for GET/HEAD/POST/PUT HTTP requests
- Support for multipart HTTP requests for file uploading.
- Multiple client support
- HTTP pipelining and keep-alive connections
- Multi-partition scheme for file storage: Seperate partitions for files in ROM and external DISK (etc SD Card) are used
- Bounded memory usage
- Single demo application demonstrating the whole API.
- On-the-fly chunked tranfer decoding
- Much emphasis on ease of use, easy integration and power consumption


# Give it a try using Unix or Cygwin under Windows:
-  (Optionally )Set your enviroment in elysian_config.h (choose from ELYSIAN_ENV_UNIX, ELYSIAN_ENV_WINDOWS, ELYSIAN_ENV_EMBEDDED). 
   Default is ELYSIAN_ENV_UNIX (works also on Windows under Cygwin).
- Issue "make". Image size may be big for embedded devices (~200 Kbytes), but that's because the default demo app contains images and audio files.
- Run the "elysian.out" executable.
- Open a browser and navigate to "http://localhost:9000"

# Samples:
![alt tag](https://raw.githubusercontent.com/npoulokefalos/Elysian-Web-Server/master/sample/sample.png)
![alt tag](https://raw.githubusercontent.com/npoulokefalos/Elysian-Web-Server/master/sample/sample2.png)
![alt tag](https://raw.githubusercontent.com/npoulokefalos/Elysian-Web-Server/master/sample/sample3.png)