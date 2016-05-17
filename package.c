#include "headers.h"
#include "package.h"
#define BUF_SIZE 4000

struct package{
	int isComand; /* 1 se e um comando, 0 se for pacote de dados */
	pid_t pidSender; /* pid do processo que envia o pacote */
	size_t bytesInBuf; /* N bytes que devem ser lidos no buf */
	char buf[BUF_SIZE]; /* Deste tamanho para assegurar que os reads e writes de uma struct pacote sao atomicas */
}

PACKAGE newPACKAGE(){
	PACKAGE new = malloc(sizeof(struct package));
	return new;
}

void deletePACKAGE(PACKAGE package){
	free(package);
}

ssize_t writePackageOnFile(int fd, PACKAGE package){
	return write(fd, package->buf, package->bytesInBuf);
}

ssize_t readPackageFromFile(int fd, PACKAGE package){
	ssize_t ret = read(fd, package->buf, BUF_SIZE);
	package->bytesInBuf = (size_t) ret;
	return ret;
} 
