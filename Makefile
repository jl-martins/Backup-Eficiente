CFLAGS = -Wall -Wextra -O2
TARGET_ARCH = -march=native

all: sobucli sobusrv

.PHONY: all install clean

install: all
	cd $(HOME)
	mkdir -p .Backup
	cd .Backup
	mkfifo fifo -m 0666
	sudo mv sobucli sobusrv /usr/bin

debug: CFLAGS = -Wall -Wextra -g
debug: all

sobucli: sobucli.o comando.o comando.h
	$(LINK.c) $^ $(OUTPUT_OPTION)

sobusrv: sobusrv.o comando.o
	$(LINK.c) $^ $(OUTPUT_OPTION)

clean:
	$(RM) sobucli sobusrv
	$(RM) *.o