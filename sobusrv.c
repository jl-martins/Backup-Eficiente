#include <stdio.h>
#include <string.h>
#include "headers.h"
#include "backup.h"
#include "comando.h"
#define TAMANHO_SHA1SUM 40 
#define SIZE_READS 4096
/* Opcoes da ZipFile */
#define ZIP 0
#define UNZIP 1

/* Todo:
 * relatorio - por no inicio que restriçoes é que consideramos: a frestore tem de serpassada com acminho absoluto para permitir que se possam restaurar 2 pastas iguais e quando se executam 2 comandos sobre o mesmo ficheiro em simultaneo, um deles pode falhar. Se nao falhar, a ordem de execucao dos comandos é indefinida.
 * - struct dados e enviar os dados a partir do cliente (fazemos o zip no servidor e evitamos problemas de permissoes. nota: os dados vao ser guardados na home do utilizador que criou a pasta mesmo que o utilizador do cliente seja diferente? (indicar a resposta no relatorio nas consideraçoes)
   - backup recursivo das pastas no cliente
?? se puser um fork à volta do zip da versao com pastas no servidor, ja funciona(prolly not, e com a versao nova do zip?)?~
 * - struct de comando
 * - ver o que acontece quando temos dois processos sobre o mesmo ficheiro (podem ser sessoes diferentes - impedir a todo o custo 
 * - por a enviar ficeiros pelos pipes em vez de o fazer noservidor - nesse caso basta fazer 2 fifos adicionais, um para restore e outro para backup - ver como o fazer para varios clientes, comunicar dados através de uma estrutura de dados "dados"
 * - delete(em paralelo)
 * - gc em paralelo 
 * - ficheiro de log (escrever no servidor)
 * - fazer testes para explicarmos ao stor como é que sabemos que o programa esta correto 
- fazer testes(unitarios) para o zipFile e outras funçoes
 * - por frestore no cliente
 * armar sinais para quando ficheiros nao existem 
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

void zipFile(char * filepath, char * newFile, int opcao){
	int pipefd[2];
	pipe(pipefd);
	if(fork()){
		int fd = open(newFile, O_CREAT | O_WRONLY, 0666);
		char buf[SIZE_READS];
		int lidos;
		close(pipefd[1]); /* fecha a ponta de escrita*/
		//wait(NULL);
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
		if(opcao == ZIP)
			execlp("gzip", "gzip", "-c", filepath, NULL);	
		else if(opcao == UNZIP)
			execlp("gzip", "gzip", "-cd", filepath, NULL);	
		exit(-1);
	}	
}

int backup(char * file){
	char path_sha1_data[MAX_PATH];
	char path_link_metadata[MAX_PATH];
	char * sha1 = sha1sum(file);
	char nomeFicheiro[256];/*substituir macro linux*/
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
	strcat(ficheiroPath, ".");
	strcat(ficheiroPath, nomeFicheiro);
	
	/* se ja houver um ficheiro diferente com o mesmo nome, nao podemos fazer backup sem ambiguidade. nao guardamos o ultimo ficheiro */
	if(symlink(path_sha1_data, path_link_metadata) == -1) /* substituir por if(access(path_link_metadata, F_OK) != -1) ??? */
		return -1; 
	/* fazer free(sha1) se return -1 */
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
	return 0;
}	

/* Neste caso, filename já não é o caminho absoluto mas sim apenas o nome*/
int restore(char * filename){
	int fd, r;
	char path_link_metadata[MAX_PATH];
	char caminho_backup[MAX_PATH];
	char ficheiroPath[MAX_PATH]; /* caminho do ficheiro que guarda o path original do ficheiro */
	char caminhoFicheiroARestaurar[MAX_PATH];

	/* local na metadata */
	strcpy(path_link_metadata, metadata_path);	
	strcat(path_link_metadata, filename);

	/* se nao foi feito um backup do ficheiro que se pretende restaurar, nao se pode restaurar o ficheiro */
	if(access(path_link_metadata, F_OK) == -1)
		return -1;
	
	/* ficheiro que contem caminho original do ficheiro a guardar */	
	strcpy(ficheiroPath, metadata_path);	
	strcat(ficheiroPath, ".");
	strcat(ficheiroPath, filename);
	
	fd = open(ficheiroPath, O_RDONLY);
	if(fd == -1)
		return -1;
	r = read(fd, caminhoFicheiroARestaurar, MAX_PATH);
	caminhoFicheiroARestaurar[r] = '\0';
	r = readlink(path_link_metadata, caminho_backup, MAX_PATH);
	caminho_backup[r] = 0;
	zipFile(caminho_backup, caminhoFicheiroARestaurar, UNZIP);
	
	
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
		case 'r': file = get_filepath(cmd);
		 	  ret = restore(file);
			  free(file); 	
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
