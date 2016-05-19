#include <unistd.h>
#include <stdlib.h>
#include "readln.h"

int create_buffer(int filedes, struct buffer_t* buffer, size_t nbyte){
	if(filedes < 0 || nbyte <= 0)
		return -1;

	buffer->data = malloc(nbyte * sizeof(char));
	if(buffer->data == NULL)
		return -1;

	buffer->filedes = filedes;
	buffer->nbyte = nbyte;
	buffer->head = 1;
	buffer->last = 0;
	return 0;
}

int destroy_buffer(struct buffer_t* buffer){
	if(buffer){
		free(buffer->data);
		free(buffer);
	}
	return 0;
}

ssize_t readln(struct buffer_t* buffer, void** buf){
	int head, last; /* temporary storage for buffer->head and buffer->last */
	size_t i, maxline = buffer->nbyte;
	char* dst_buf = *((char**) buf);
	
	i = 0;
	head = buffer->head;
	last = buffer->last;
	do{
		if(head > last){ /* on empty buffer, read another line */
			ssize_t nr = read(buffer->filedes, buffer->data, buffer->nbyte);
			if(nr <= 0) /* EOF or read error */
				return nr;
			
			head = buffer->head = 0;
			last = buffer->last = nr-1;
		}
		dst_buf[i++] = buffer->data[head++];
	} while(i < maxline && buffer->data[head] != '\n');

	if(buffer->data[head] == '\n')
		dst_buf[i++] = '\n';

	buffer->head = head + 1;
	return (ssize_t) i;
}
