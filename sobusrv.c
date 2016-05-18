#include <stdio.h>
#include <string.h>
#include "headers.h"
#include "backup.h"
#include "comando.h"

/* Todo:
 * - script de instalação
 * - struct de comando
 * - ver o que acontece quando temos dois processos sobre o mesmo ficheiro
 * - por a enviar ficeiros pelos pipes em vez de o fazer noservidor - nesse caso basta fazer 2 fifos adicionais, um para restore e outro para backup - ver como o fazer para varios clientes, comunicar dados através de uma estrutura de dados "dados"
 * - restore
 * - backup
 * - delete(em paralelo)
 * - gc em paralelo 
 * - ficheiro de log
*/ 

/* ver como me certificar que os ficheiros sao escritos por ordem correta (tenho que garantir que as linhas sao escritas pela ordem certa no ficheiro, apesar de poderem ser escritas por varios processos */

int backup(){
	return 0;
}	

int execComando(Comando cmd){
	char codigo_comando = get_codigoComando(cmd);
	int ret = -1;
	char * file;
	switch(codigo_comando){
		case 'b': file = get_filepath(cmd);
		 	  ret = backup(file);
			  free(file); 	
			  break;
		case 'r':
			  break;
	}
	return ret;
}

void setupComando(int fifo){
	int r;
	Comando cmd = aloca_comando(); /* verificar se da null*/ 
	if(cmd == NULL)
		printf("Erro de memoria. O servidor deve ser reiniciado para assegurar maxima performance\n");
	r = read(fifo, cmd, tamanhoComando());
	if(r == 0){
		free(cmd);
		return;
	}
	if(r != tamanhoComando()){
		free(cmd);	
		printf("Erro de leitura do comando. O Servidor vai encerrar!! \nTodos os comandos que nao tenham recebido mensagem de confirmacao deverao ser reintroduzidos\n"); /* uma ma leitura leva a que o conteudo do FIFO fique corrompido */
		kill(-getppid(), SIGKILL);
	}
	r = execComando(cmd);
	free(cmd);	
	if(r == 0)
		kill(get_pid_comando(cmd), SIGUSR1);
	else 
		kill(get_pid_comando(cmd), SIGUSR2);
	close(fifo);
}

int main(){
	int i, fifo;
	char backup_path[MAX_PATH];
	strcpy(backup_path, getenv("HOME"));
	strcat(backup_path, "/.Backup/fifo");

	/* termina o programa mas deixa os processos em execução */
	if(fork())
		_exit(0);	
	if(fork()){
		fifo = open(backup_path,  O_WRONLY); 
		pause();
		_exit(-1);
	}
	fifo = open(backup_path,  O_RDONLY); 
	if(fifo == -1)
		_exit(-1);

	for(i = 0; i < 5; i++){
		if(!fork()){
			setupComando(fifo);
			_exit(0);
		}
	}
			
	while(wait(NULL)){
		if(!fork()){
			setupComando(fifo);
			_exit(0);
		}
	}	
	_exit(0);
}


/* fazer syshandlers */
/* void sigHandler(int signal){
	if(signal == SIGCHLD)
}*/
/* criar comandos que abrem e fecham o fifo no cliente que devem enviar ao inicio e fim */
