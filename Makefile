OBJNAME=wally
BASESRC=src
ARCH=arch/x86

MOD_PROC_FILE=

# WALLY_PROC_FILE, /proc/<name>, change this if you wish
COMPILER_OPTIONS := -Werror -Wall -DWALLY_PROC_FILE='"wally"'
EXTRA_CFLAGS := -I$(src)/src -I$(src)/fs ${COMPILER_OPTIONS}

SRC := src/${OBJNAME}.c src/kernel_addr.c src/pid.c src/fs.c

$(OBJNAME)-objs = $(SRC:.c=.o) $(ASRC:.S=.o)

obj-m := ${OBJNAME}.o

CC=gcc

all:
	make  -C  /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	$(CC) ./tests/test.c -o ./tests/test

clean:
	@make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	@rm -f .hide* *.o src/*.o
	@find . -name "*.swp" |xargs rm -fv 2>/dev/null
	@find . -name "*.swo" |xargs rm -fv 2>/dev/null
	@find . -name "*.swx" |xargs rm -fv 2>/dev/null
	@find . -name "*~" |xargs rm -fv 2>/dev/null
	@rm -f ./tests/test
	@echo "Clean."

tags:
	find . -iname '*.c' -o -iname '*.h' -o -iname '*.i' -o -iname '*.S' | xargs etags -a

.PHONY: all clean tags
