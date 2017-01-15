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
by adopting the MVC design pattern, using C handler function as controllers.

Supported features include:
- Small footprint: requires ~50kb FLASH and ~5kb RAM
- Much emphasis on ease of use, easy integration and power consumption
- Support for dynamic content using the model-view-controller (MVC) design pattern
- Single thread / Μultiple client implementation
- Extremely robust: Εxponential backoff mechanism resolves any temporarυ memory or network unavailability issues. 
- Support for GET/HEAD/POST/PUT HTTP requests
- Support for multipart HTTP requests for file uploading.
- Chunked tranfer encoding and decoding
- HTTP pipelining and keep-alive connections
- Support for HTTP requests with expectation
- Support for multiple memory devices: Files can be stored to ROM, RAM, or External memory device (etc SD-Card, USB, Hard Disk)
- Bounded memory usage
- Single demo application demonstrating the whole API.
- Ported OS enviroments: FreeRTOS, ChibiOS, Unix, Windows
- Ported TCP\IP enviroments: LwIP, Unix, Windows
- Ported filesystems: FatFS, Unix, Windows

# Give it a try under Linux or Windows:

## Linux or Cygwin: 
- Open a terminal and navigate to "examples/api_demo"
- Isuue "make plat=linux"
- Run the "./elysian.out" executable.
- Open a browser and navigate to "http://localhost:9000"

## Windows (MinGW): 
- Open cmd and navigate to "examples/api_demo"
- Isuue "mingw32-make make plat=windows"
- Run the "elysian.out" executable.
- Open a browser and navigate to "http://localhost:9000"

# Samples:
![alt tag](https://raw.githubusercontent.com/npoulokefalos/Elysian-Web-Server/master/sample/sample.png)
![alt tag](https://raw.githubusercontent.com/npoulokefalos/Elysian-Web-Server/master/sample/sample2.png)
![alt tag](https://raw.githubusercontent.com/npoulokefalos/Elysian-Web-Server/master/sample/sample3.png)