ELYSIAN_CORE_DIR=../../core
CC=gcc

include $(ELYSIAN_CORE_DIR)/elysian.mk

ifeq ($(plat),)
$(info Usage "make plat=linux" [for Linux or Cygwin] or "make plat=windows" [for Windows using MinGW])
$(error Exiting)
endif

$(info )
$(info --------------------------------------------------------------------------------)
$(info To generate the ROM file structure execute 'python fs_rom/makefsdata.py')
$(info --------------------------------------------------------------------------------)
$(info )

ifeq ($(plat),windows)
	# Windows
	FLAGS=-lws2_32
	DEFS=-DELYSIAN_ENV_WINDOWS
else
	#Linux
	FLAGS=
	DEFS=-DELYSIAN_ENV_UNIX
endif

APP_SOURCE_C = \
				main.c

SOURCE_C=$(addprefix $(ELYSIAN_CORE_DIR)/, $(ELYSIAN_SOURCE_C)) $(APP_SOURCE_C)
INCLUDE_DIR=-I$(ELYSIAN_CORE_DIR) -I../examples/demo -I.

all:
	$(CC) -g -Wall  $(INCLUDE_DIR) -Wl,-Map=elysian.map $(SOURCE_C) $(DEFS) $(FLAGS) -o elysian.out
	size elysian.out
