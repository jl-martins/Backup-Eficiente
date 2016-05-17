#include "headers.h"
#include "backup.h"
#include "comando.h"

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

int backup(){
	

int execComando(Comando cmd){
	char codigo_comando = get_codigoComando(cmd);
	int ret;
	char * file;
	switch(codigo_comando){
		case 'b': file = get_filepath(cmd);
		 	  ret = backup(file);
			  free(file); 	
		case 'r':
		default: return -1;
}

int main(){
	int i;
	int fifo = open("~/.Backup/comandos",  O_RDONLY); /* subsitutir por HOME, criar fifo se nao houver */
	if(fd == -1)
		return -1;
	/* fazer comando no cliente que indica que a transferencia de dados acabou para poder matar processo de sinais */

	/* termina o programa mas deixa os processos em execução */
	if(fork())
		return 0;
	
	for(i = 0; i < 5; i++){
		if(!fork()){
			int r;
			Comando cmd = aloca_comando(); /* verificar se da null*/ 
			if(cmd == null){
				printf("Erro mem\n");
				_exit(); /* necessario propagar exit para todos os processos */
			}
				
			r = read(fifo, cmd, sizeof(struct comando));
			if(r != sizeof(struct comando)){
				printf("Erro de leitura do comando");
				_exit(); /* necessario propagar exit para todos os processos */
			}
			r = execComando(cmd);
			if(r == 0)
				kill(get_pid_comando(cmd), SIGUSR1);
			else 
				kill(get_pid_comando(cmd), SIGUSR2);
			break;
			/* definir como proceder quando le uma linha (onde é que a deve escrever, gerir o numero de processos abertos, etc) */
		/*fazer com um ciclo com waits */
		}
	}
	
	while(wait()){
		if(!fork()){
			/* readline(fd, cmd, sizeof(COMANDO)); deve ser um read e nao um readline*/
			read(fifo, cmd, sizeof(struct comando));
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
