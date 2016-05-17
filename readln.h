#ifndef READLN_V2
#define READLN_V2

#include <stddef.h>

struct buffer_t{
	char* data;
	int head, last;
	size_t nbyte;
	int filedes;
};

int create_buffer(int filedes, struct buffer_t* buffer, size_t nbyte);
int destroy_buffer(struct buffer_t* buffer);

ssize_t readln(struct buffer_t* buffer, void** buf);

#endif
