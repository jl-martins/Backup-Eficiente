CFLAGS = -Wall -Wextra -O2
TARGET_ARCH = -march=native

all: sobucli sobusrv

.PHONY: all clean

debug: CFLAGS = -Wall -Wextra -g
debug: all

sobucli: sobucli.o comando.o comando.h
	$(LINK.c) $^ $(OUTPUT_OPTION)

sobusrv: sobusrv.o comando.o
	$(LINK.c) $^ $(OUTPUT_OPTION)

clean:
	$(RM) sobucli sobusrv
	$(RM) *.o