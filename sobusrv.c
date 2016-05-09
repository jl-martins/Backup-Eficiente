#include <fcntl.h>
#include <unistd.h>

#include "backup.h"

int main(){
	if(!fork()){
		/*o filho fica responsavel pelo servidor*/	
		int fd = open(LOCAL_FIFO, O_RDONLY);
		/* definir como proceder quando le uma linha (onde é que a deve escrever, gerir o numero de processos abertos, etc) */
	}
	/* o pai termina a execucao */
	return 0;
}
