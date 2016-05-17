#include "headers.h"
#include "backup.h"

/* Todo:
 * - Testar getLine e faze-la atomica
 * - struct de comando
 * - ver o que acontece quando temos dois processos sobre o mesmo ficheiro
 * - por a enviar ficeiros pelos pipes em vez de o fazer noservidor - nesse caso basta fazer 2 fifos adicionais, um para restore e outro para backup - ver como o fazer para varios clientes
 * - restore
 * - backup
 * - delete(em paralelo)
 * - gc em paralelo 
*/ 

int execComando(Comando cmd){
	
}

int main(){
	int i;
	int fd = open("~/.Backup/comandos",  O_RDONLY); /* subsitutir por HOME, criar fifo se nao houver */
	if(fd == -1)
		return -1;
	/* fazer comando no cliente que indica que a transferencia de dados acabou para poder matar processo de sinais */

	/* termina o programa mas deixa os processos em execução */
	if(fork())
		return 0;
	
	for(i = 0; i < 5; i++){
		if(!fork()){
			int r;
			struct comando * cmd;
			
			readline(fd, cmd, sizeof(struct comando));/* obtem um comando que está no fifo */
			r = execComando(cmd);
			if(r == 0)
				kill(getPidComando(cmd), SIGUSR1);
			else 
				kill(getPidComando(cmd), SIGUSR2);
			break;
			/* definir como proceder quando le uma linha (onde é que a deve escrever, gerir o numero de processos abertos, etc) */
		/*fazer com um ciclo com waits */
		}
	}
	
	while(wait()){
		if(!fork()){
			readline(fd, cmd, sizeof(COMANDO));
			execComando(cmd);
			break;
		}
	}	
	_exit(0);
}

/* a exec comando deve mandar o sinal */

/* fazer syshandlers */
/* void sigHandler(int signal){
	if(signal == SIGCHLD)
}*/
/* criar comandos que abrem e fecham o fifo no cliente que devem enviar ao inicio e fim */
