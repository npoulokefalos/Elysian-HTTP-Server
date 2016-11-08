SOURCE_C = \
	main.c \
	scheduler.c \
	elysian.c \
	cbuf.c \
	port.c \
	os.c \
	resource.c \
	mvc.c \
	http.c \
	fs.c \
	stats.c \
	strings_file.c \
	strings.c \
	isp.c

UNAME := $(shell uname)
FLAGS=

ifeq ($(OS),Windows_NT)
	FLAGS=-lws2_32
else
	ifeq ($(UNAME), MINGW32_NT-6.1)
		FLAGS=-lws2_32
	endif
	ifeq ($(UNAME), MINGW64_NT-6.1)
		FLAGS=-lws2_32
	endif
endif

all:
	gcc -g -Wall $(SOURCE_C) $(FLAGS) -Wl,-Map=elysian.map -o elysian.out
	size elysian.out
