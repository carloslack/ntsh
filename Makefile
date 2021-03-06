OBJNAME=ntsh
BASESRC=src
ARCH=arch/x86

MOD_PROC_FILE=

# PROCNAME, /proc/<name>, change this if you wish
COMPILER_OPTIONS := -Werror -Wall -DPROCNAME='"ntsh"' -DMODNAME='"ntsh"'
EXTRA_CFLAGS := -I$(src)/src -I$(src)/fs -I$(src)/asm ${COMPILER_OPTIONS}

SRC := src/${OBJNAME}.c src/kernel_addr.c src/pid.c src/fs.c
ASRC := src/asm/vfs_rmdir_trpline64.S

$(OBJNAME)-objs = $(SRC:.c=.o) $(ASRC:.S=.o)

obj-m := ${OBJNAME}.o

CC=gcc

all:
	make  -C  /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	$(CC) ./tests/test.c -o ./tests/test

# This make option is very hacky until I find a better
# way to integrate with LGTM & yaml
lgtm:
	make  -C  /lib/modules/$(shell dpkg --status linux-headers-generic |grep Depends| \
		cut -d ":" -f2| sed 's/ linux-headers-//g')/build M=$(PWD) modules
	$(CC) ./tests/test.c -o ./tests/test


clean:
	@make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	@rm -f *.o src/*.o
	@rm -f ./tests/test
	@echo "Clean."

tags:
	find . -iname '*.c' -o -iname '*.h' -o -iname '*.i' -o -iname '*.S' | xargs etags -a

.PHONY: all clean tags
