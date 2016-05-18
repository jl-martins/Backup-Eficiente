#include <stdio.h>
#include <string.h>
#include "headers.h"
#include "backup.h"
#include "comando.h"
#define TAMANHO_SHA1SUM 40 
#define SIZE_READS 4096
#define SEPARADOR 31
/* Todo:
 * - script de instalação
 * - struct de comando
 * - ver o que acontece quando temos dois processos sobre o mesmo ficheiro (podem ser sessoes diferentes - impedir a todo o custo 
 * - por a enviar ficeiros pelos pipes em vez de o fazer noservidor - nesse caso basta fazer 2 fifos adicionais, um para restore e outro para backup - ver como o fazer para varios clientes, comunicar dados através de uma estrutura de dados "dados"
 * - restore
 * - backup
 * - delete(em paralelo)
 * - gc em paralelo 
 * - ficheiro de log (escrever no servidor)
 * - fazer testes para explicarmos ao stor como é que sabemos que o programa esta correto 
*/ 

/* ver como me certificar que os ficheiros sao escritos por ordem correta (tenho que garantir que as linhas sao escritas pela ordem certa no ficheiro, apesar de poderem ser escritas por varios processos */

/* Devolve a string resultante da aplicação do comando sha1sum ao ficheiro 'filename' */
/* verificar sempre resultados do fork e outras syscalls */
/* deve-se verificar se o ficheiro existe antes da invocacao */


char backup_path[MAX_PATH]; /* vai conter o caminho da pasta de backups quando o programa inicia */
char metadata_path[MAX_PATH]; /* vai conter o caminho da pasta de metadata quando o programa inicia */ 
char data_path[MAX_PATH]; /* vai conter o caminho da pasta de dados quando o programa inicia */
char fifo_path[MAX_PATH];

char * sha1sum(char * filename){
	int pipefd[2];
	pipe(pipefd);
	if(fork()){
		char *  sha1 = malloc(TAMANHO_SHA1SUM + 1);
		close(pipefd[1]); /* fecha a ponta de escrita*/
		wait(NULL);
		read(pipefd[0], sha1, TAMANHO_SHA1SUM);
		close(pipefd[0]);
		sha1[TAMANHO_SHA1SUM] = '\0';	
		return sha1;
	}else{
		close(pipefd[0]);/* fecha a ponta de leitura */	
		if(dup2(pipefd[1], 1) != -1)
			close(pipefd[1]);
		else _exit(-1); /* melhorar condicoes */	
		execlp("sha1sum", "sha1sum", filename, NULL);	
		exit(-1);
	}	
}

void zipFile(char * filepath, char * newFile){
	int pipefd[2];
	pipe(pipefd);
	if(fork()){
		int fd = open(newFile, O_CREAT | O_WRONLY, 0666);
		char buf[SIZE_READS];
		int lidos;
		close(pipefd[1]); /* fecha a ponta de escrita*/
		wait(NULL);
		while((lidos = read(pipefd[0], buf, SIZE_READS)) > 0){
			write(fd, buf, lidos);
		}
		if(lidos == -1)
			printf("ERRO LEITURA!\n");
		close(pipefd[0]);
	}else{
		close(pipefd[0]);/* fecha a ponta de leitura */	
		if(dup2(pipefd[1], 1) != -1)
			close(pipefd[1]);
		else _exit(-1); /* melhorar condicoes */	
		execlp("gzip", "gzip", "-c", filepath, NULL);	
		exit(-1);
	}	
}

void parse_path(char * path){
	for( ; path[0]; path++){
		if(path[0] == '/')
			path[0] = SEPARADOR;
	}
}

int backup(char * file){

	char path_sha1_data[MAX_PATH];
	char path_link_metadata[MAX_PATH];
	char * sha1 = sha1sum(file);

	strcpy(path_sha1_data, data_path);
	strcat(path_sha1_data, sha1);
	strcat(path_sha1_data, ".gz");
	free(sha1);
	
	/* local na metadata */
	strcpy(path_link_metadata, metadata_path);
	
	
	if(access(path_sha1_data, F_OK) != -1){ // se o ficheiro existe 
		// regista o link se o nome nao estivera ser usado 					
			parse_path(file);
			strcat(path_link_metadata, file);
			symlink(path_sha1_data, path_link_metadata); 
	}else{
		// regista o link e faz o zip 
		if(!fork()){
			zipFile(file, path_sha1_data);	
			parse_path(file);
			strcat(path_link_metadata, file);
			symlink(path_sha1_data, path_link_metadata); 
		}
	}
	
	return 0;
}	

int restore(){
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
	if(cmd == NULL){
		_exit(-1);
	}
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
	if(r == 0)
		kill(get_pid_comando(cmd), SIGUSR1);
	else 
		kill(get_pid_comando(cmd), SIGUSR2);
	free(cmd);	
	close(fifo);
}

int main(){
	int i, fifo;
	
	/* faz setup da variavel global que contem o local dos backups */
	strcpy(backup_path, getenv("HOME"));
	strcat(backup_path, "/.Backup/");

	/* setup do path do fifo */
	strcpy(fifo_path, backup_path);
	strcat(fifo_path, "fifo");

	/* setup do local da metadata */
	strcpy(metadata_path, backup_path);
	strcat(metadata_path, "metadata/");

	/* setup do local da data */
	strcpy(data_path, backup_path);
	strcat(data_path, "data/");

	/* termina o programa mas deixa os processos em execução */
	if(fork())
		_exit(0);	
	
	if(!fork()){
		// abre o pipe para escrita -> faz com que os outros processos bloqueiem quando nao ha nada para ler do buffer 
		fifo = open(fifo_path,  O_WRONLY); 
		pause();
		_exit(-1);
	}

	fifo = open(fifo_path,  O_RDONLY); 
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
