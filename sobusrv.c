#include <stdio.h>
#include <string.h>
#include <unistd.h> 
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include "comando.h"
#define TAMANHO_SHA1SUM 40 
#define SIZE_READS 4096
/* Opcoes da ZipFile */
#define ZIP 0
#define UNZIP 1
#define PATH_FILE_INDICATOR "\031"

int delete(char * filename);
char * sha1sum(char * filename);
void zipFile(char * filepath, char * newFile, int opcao);
int backup(char * file);
int restore(char * filename);
int execComando(Comando cmd);
void setupComando(int fifo);
void logServer(char * msg);
int frestore(char * path);
int gc();
int temLink(char * file);

/* Todo:
 * - ficheiro de log (escrever no servidor)
 * - tirar os warnings
 * verificar todo o codigo e ver retorno de syscalls
 * comentar tudo 
*/ 

char backup_path[MAX_PATH];   /* pasta de backups */
char metadata_path[MAX_PATH]; /* pasta de metadata */ 
char data_path[MAX_PATH];     /* pasta de dados */
char fifo_path[MAX_PATH];     /* caminho do fifo */
char logfile_path[MAX_PATH];  /* caminho do ficheiro de log */
int log_fd;                   /* file descriptor do ficheiro de log */

/* Escreve uma mensagem no ficheiro de log */
void logServer(char * msg){
	write(log_fd, msg, strlen(msg));
}

/* Apaga os metadados de um ficheiro. Deve ser passado apenas o nome do ficheiro e nao o path */
int delete(char * filename){
	char path_link_metadata[MAX_PATH];
	char ficheiroPath[MAX_PATH]; /* caminho do ficheiro que guarda o path original do ficheiro */

	/* Poe em path_link_metadata o caminho do link para o .gz */
	strcpy(path_link_metadata, metadata_path);	
	strcat(path_link_metadata, filename);

	/* se nao foi feito um backup do ficheiro que se pretende restaurar, nao se pode restaurar o ficheiro */
	if(access(path_link_metadata, F_OK) == -1)
		return 0;
	
	/* Poe em ficheiroPath o caminho do ficheiro que contem o caminho original do ficheiro guardado */	
	strcpy(ficheiroPath, metadata_path);	
	strcat(ficheiroPath, PATH_FILE_INDICATOR);
	strcat(ficheiroPath, filename);

	/* apaga os dois ficheiros criados durante o backup */	
	if(unlink(ficheiroPath) == -1 || unlink(path_link_metadata) == -1)
		return -1;
	logServer("[DELETE] ");
	logServer(filename);
	logServer(": Sucesso\n");
	return 0;
}

/* Devolve a string resultante da aplicação do comando sha1sum ao ficheiro 'filename'.
 * Deve-se verificar se o ficheiro existe antes da invocacao 
 */
char * sha1sum(char * filename){
	int pipefd[2];
	int exitStatus;
	pipe(pipefd);
	if(fork()){
		char *  sha1 = malloc(TAMANHO_SHA1SUM + 1);
		if(close(pipefd[1]) == -1){  /* fecha a ponta de escrita */
		}	                    /* tira o aviso do compilador */  
		wait(&exitStatus);  

		if(WEXITSTATUS(exitStatus) != 0 ||
		   read(pipefd[0], sha1, TAMANHO_SHA1SUM) == -1){
			logServer("[ERRO] SHA1SUM: ");
			logServer(filename);
			logServer("\n");
			_exit(-1);
		}

		if(close(pipefd[0])){
		}
		sha1[TAMANHO_SHA1SUM] = '\0';	
		return sha1;
	}else{
		if(close(pipefd[0]) == -1){  /* fecha a ponta de leitura */
		}			
		if(dup2(pipefd[1], 1) != -1){
			if(close(pipefd[1]) == -1){
			}
		}
		else 
			_exit(-1); 	
		execlp("sha1sum", "sha1sum", filename, NULL);	
		exit(-1);
	}	
}

/* Realiza a operacao especificada em opcao(ZIP ou UNZIP) onde filepath é o caminho absoluto do ficheiro a ZIPAR/UNZIPAR e newFile é o caminho onde vai
 * ser escrito o resultado da operacao
 */
void zipFile(char * filepath, char * newFile, int opcao){
	int pipefd[2];
	int fork_result;

	if(pipe(pipefd) == -1 || (fork_result = fork()) == -1){
		logServer("[ERRO] zipFile: ");	
		logServer(filepath);
		logServer(" -> ");
		logServer(newFile);
		logServer("\n");
		_exit(-1);
	}

	if(fork_result){
		char buf[SIZE_READS];
		int lidos;
		int fd = open(newFile, O_CREAT | O_WRONLY, 0666);
		
		if(fd == -1)
			_exit(-1);
		if(close(pipefd[1]) == -1){ /* fecha a ponta de escrita*/
		} 

		while((lidos = read(pipefd[0], buf, SIZE_READS)) > 0){
			write(fd, buf, lidos);
		}
		if(lidos == -1){
			logServer("[ERRO] zipFile: erro de escrita\n");
		}
		close(pipefd[0]);
	}else{
		close(pipefd[0]);/* fecha a ponta de leitura */	
		if(dup2(pipefd[1], 1) != -1)
			close(pipefd[1]);
		else 
			_exit(-1); 	
		if(opcao == ZIP)
			execlp("gzip", "gzip", "-c", filepath, NULL);	
		else if(opcao == UNZIP)
			execlp("gzip", "gzip", "-cd", filepath, NULL);	
		exit(-1);
	}	
}

int temLink(char * name){
	char ficheiro[MAX_PATH];
	char caminho_link[MAX_PATH];
	DIR * metadata = opendir(metadata_path);
	struct dirent * d;
	int len = strlen(metadata_path);
	int k;
	strcpy(ficheiro, metadata_path);
	while(d = readdir(metadata)){
		if(d->d_name[0] == PATH_FILE_INDICATOR[0])
			continue;
		strcpy(ficheiro+len, d->d_name);
		k = readlink(ficheiro, caminho_link, MAX_PATH);
		if(k == -1)
			continue;
		caminho_link[k] = '\0';
		if(!strcmp(caminho_link, name))
			return 1;
	}
	free(metadata);
	return 0;
}

/* Nao pode ser usado ao mesmo tempo que um backup e delete */
int gc(){
	char ficheiro[MAX_PATH];
	DIR * data = opendir(data_path);
	struct dirent * d;
	int len = strlen(data_path);
	strcpy(ficheiro, data_path);
	while(d = readdir(data)){
		strcpy(ficheiro+len, d->d_name);
		if(!temLink(ficheiro))
			unlink(ficheiro);	
	}
	free(data);
	return 0;		
}

int frestore(char * caminhoAbsolutoPasta){
	DIR * metadata;
	struct dirent * d;
	char caminhoFicheiro[MAX_PATH];
	char conteudoFicheiroPath[MAX_PATH];	
	int len = strlen(metadata_path);
	int fd;

	metadata = opendir(metadata_path);
	strcpy(caminhoFicheiro, metadata_path);
	while(d = readdir(metadata)){
		if(d->d_name[0] != PATH_FILE_INDICATOR[0])
			continue;
		strcpy(caminhoFicheiro + len, d->d_name);
		fd = open(caminhoFicheiro, O_RDONLY);
		if(fd == -1 || read(fd, conteudoFicheiroPath, MAX_PATH) == -1)
			continue;
		if(strstr(conteudoFicheiroPath, caminhoAbsolutoPasta) == conteudoFicheiroPath)
			restore(d->d_name+1);	
		close(fd);		
	}
	free(metadata);
	return 0;	
}

/* Dado o caminho absoluto de um ficheiro, faz backup do mesmo */
int backup(char * file){
	char path_sha1_data[MAX_PATH];
	char path_link_metadata[MAX_PATH];
	char * sha1 = sha1sum(file);
	char nomeFicheiro[255];
	char ficheiroPath[MAX_PATH]; /* caminho do ficheiro que guarda o path original do ficheiro */
	
	int i, j, len, fd;
	len = strlen(file);

	/* poe em nomeFicheiro o nome do ficheiro */	
	for(i = 0, j = 0; i < len; i++){
		if(file[i] == '/')
			j = 0;
		else nomeFicheiro[j++] = file[i];
	}
	nomeFicheiro[j] = '\0';
	/* */
	
	strcpy(path_sha1_data, data_path);
	strcat(path_sha1_data, sha1);
	strcat(path_sha1_data, ".gz");
	
	/* local na metadata */
	strcpy(path_link_metadata, metadata_path);	
	strcat(path_link_metadata, nomeFicheiro);

	/* ficheiro que ira conter caminho original do ficheiro a guardar */	
	strcpy(ficheiroPath, metadata_path);	
	strcat(ficheiroPath, PATH_FILE_INDICATOR);
	strcat(ficheiroPath, nomeFicheiro);
	
	/* se ja houver um ficheiro diferente com o mesmo nome, nao podemos fazer backup sem ambiguidade. nao guardamos o ultimo ficheiro */
	if(symlink(path_sha1_data, path_link_metadata) == -1) 
		return -1; 
	if((fd = open(ficheiroPath, O_CREAT | O_WRONLY, 0666)) == -1)
		return -1;
	if(write(fd, file, strlen(file)) == -1)
		return -1;
	if(write(fd, "", 1) == -1)
		return -1;
	close(fd);

	if(access(path_sha1_data, F_OK) == -1) // se os dados nao estao guardados 
		zipFile(file, path_sha1_data, ZIP);	
		
	free(sha1);
	logServer("[BACKUP] ");
	logServer(file);
	logServer(": Sucesso\n");
	return 0;
}	

/* Neste caso, filename já não é o caminho absoluto mas sim apenas o nome*/
int restore(char * filename){
	int fd, r, i, fork_result;
	char path_link_metadata[MAX_PATH];
	char caminho_backup[MAX_PATH];
	char ficheiroPath[MAX_PATH]; /* caminho do ficheiro que guarda o path original do ficheiro */
	char caminhoFicheiroARestaurar[MAX_PATH];
	char path[MAX_PATH]; /* caminho do ficheir*/

	/* local na metadata */
	strcpy(path_link_metadata, metadata_path);	
	strcat(path_link_metadata, filename);

	/* se nao foi feito um backup do ficheiro que se pretende restaurar, nao se pode restaurar o ficheiro */
	if(access(path_link_metadata, F_OK) == -1)
		return -1;
	
	/* ficheiro que contem caminho original do ficheiro a guardar */	
	strcpy(ficheiroPath, metadata_path);	
	strcat(ficheiroPath, PATH_FILE_INDICATOR);
	strcat(ficheiroPath, filename);
	
	fd = open(ficheiroPath, O_RDONLY);
	if(fd == -1)
		return -1;
	r = read(fd, caminhoFicheiroARestaurar, MAX_PATH);
	if(r == -1)
		return -1;
	caminhoFicheiroARestaurar[r] = '\0';
	strcpy(path, caminhoFicheiroARestaurar);
	for(i = strlen(caminhoFicheiroARestaurar); 1; i--){
		if(path[i] == '/'){
			path[i] = '\0';
			break;
		}
	}	
	logServer(path);
	fork_result = fork();
	if(fork_result == -1){
		return -1;
	}
	if(!fork_result){
		execlp("mkdir", "mkdir", "-p", path, NULL);
		_exit(-1);
	} else {
		wait(&i);
		if(i == -1)
			return -1;
	}

	r = readlink(path_link_metadata, caminho_backup, MAX_PATH);
	if(r == -1)
		return -1;
	caminho_backup[r] = 0;
	zipFile(caminho_backup, caminhoFicheiroARestaurar, UNZIP);
	return 0;
}

/* Executa um comando guardado */
int execComando(Comando cmd){
	char codigo_comando = get_codigoComando(cmd);
	int ret = -1;
	char * file = get_filepath(cmd);
	switch(codigo_comando){
		case 'b': 
		 	  ret = backup(file);
			  break;
		case 'r': ret = restore(file);
			  break;
		case 'd': ret = delete(file);
			  break;
		case 'g': ret = gc();
			  break;
		case 'f': ret = frestore(file);
			  break;
	}
	free(file);
	return ret;
}

/* Le um comando do fifo e prepara todo o contexto para executar o comando */
void setupComando(int fifo){
	int r;
	Comando cmd = aloca_comando(); 
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
		logServer("Erro de leitura do comando. O Servidor vai encerrar!! \nTodos os comandos que nao tenham recebido mensagem de confirmacao deverao ser reintroduzidos\n"); /* uma ma leitura leva a que o conteudo do FIFO fique corrompido */
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
	int i, fifo, fork_result;		
	
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

	/* setup do local do ficheiro de log e abertura do mesmo */
	strcpy(logfile_path, backup_path);
	strcat(logfile_path, "log.txt");
	log_fd = open(logfile_path, O_CREAT | O_WRONLY | O_APPEND, 0666);

	/* termina o programa mas deixa os processos em execução */
	fork_result = fork();
	if(fork_result == -1)
		_exit(-1);
	if(fork_result)
		_exit(0);	

	fork_result = fork();	
	if(fork_result == -1)
		_exit(-1);
	if(!fork_result){
		// abre o pipe para escrita -> faz com que os outros processos bloqueiem quando nao ha nada para ler do buffer 
		fifo = open(fifo_path,  O_WRONLY); 
		pause();
		_exit(-1);
	}

	fifo = open(fifo_path,  O_RDONLY); 
	if(fifo == -1)
		_exit(-1);

	for(i = 0; i < 5; i++){
		fork_result = fork();
		if(fork_result == -1)
			_exit(-1);
		if(!fork_result){
			setupComando(fifo);
			_exit(0);
		}
	}
	
	/* so o processo pai é que chega aqui */			
	while(wait(NULL)){
		fork_result = fork();
		if(fork_result == -1)
			_exit(-1);
		if(!fork_result){
			setupComando(fifo);
			_exit(0);
		}
	}	
	_exit(0);
}
