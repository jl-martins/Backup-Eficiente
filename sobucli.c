#include "comando.h"
#include "readln.h"
#include <dirent.h>
#include <errno.h> /* inclui ENOTDIR e outros códigos de erro */
#include <fcntl.h>
#include <limits.h> /* PATH_MAX e FILENAME_MAX */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define PERROR_AND_EXIT(msg) {perror(__FILE__ ":" TOSTRING(__LINE__) ":" msg); _exit(1);}

/* variáveis usadas em sighandler() na impressão de mensagens de sucesso ou erro */
static char cmd_abbrev = '\0';
static char* last_file = NULL;
/* número de filhos criados pelo processo incial */
static int nchild;

char get_cmd_abbrev(int argc, char* argv[]);
void send_cmd(int fifo_fd, char* arg_path, char* full_path);
void send_cmd_backup_dir(int fifo_fd, char* full_path);
void sighandler(int sig);
int find(const char* path);
int is_file(char* path);
int is_dir(char* path);

int main(int argc, char* argv[]){
	int i, fifo_fd;
	char backup_path[MAX_PATH];

	cmd_abbrev = get_cmd_abbrev(argc, argv);
	if(cmd_abbrev == '\0')
		_exit(1);

	strcpy(backup_path, getenv("HOME"));
	strcat(backup_path, "/.Backup");
	strcat(backup_path, "/fifo");

	fifo_fd = open(backup_path, O_WRONLY);
	if(fifo_fd == -1)
		PERROR_AND_EXIT("open")

	if(signal(SIGUSR1, sighandler) == SIG_ERR || signal(SIGUSR2, sighandler) == SIG_ERR)
		PERROR_AND_EXIT("signal")

	nchild = 0; /* inicializa o número de processos filhos a 0 */
	if(cmd_abbrev == 'b'){
		char* full_path;

		for(i = 2; i < argc; ++i){
			full_path = realpath(argv[i], NULL);
			if(full_path == NULL)
				perror(argv[i]);
			else if(is_file(full_path))
				send_cmd(fifo_fd, argv[i], full_path);
			else if(is_dir(full_path))
				send_cmd_backup_dir(fifo_fd, full_path);
			else
				fprintf(stderr, "Erro: '%s' não é um ficheiro nem uma diretoria.\n", argv[i]);
			free(full_path);
		}
	}
	else if(cmd_abbrev == 'g')
		send_cmd(fifo_fd, "", NULL); /* gc nao tem argumentos, daí a string vazia "" */
	else{
		/* restore, frestore ou delete */
		for(i = 2; i < argc; ++i)
			send_cmd(fifo_fd, argv[i], NULL);
	}
	/* processo pai espera pelos filhos */
	while(wait(NULL) && errno != ECHILD)
		;

	close(fifo_fd);
	_exit(0); /* so chegamos aqui quando nao houve erros */
}

/* 
 * Valida o comando introduzido com base em argc e argv e, se este for
 * valido, devolve uma abreviatura do mesmo. A abreviatura devolvida
 * nao e mais do que a 1ª letra do comando.
 * Se o comando ou o numero de argumentos forem invalidos, imprime
 * uma mensagem de erro no stderr e devolve o carater '\0'.
 */
char get_cmd_abbrev(int argc, char* argv[]){
	char r = '\0';

	if(argc == 1)
		fputs("Numero invalido de argumentos. Utilizacao: sobucli cmd [FILE]...\n", stderr);
	else if(!strcmp("gc", argv[1]))
		(argc == 2) ? r = 'g' : fputs("'gc' nao recebe quaisquer argumentos.\n", stderr);
	else{
		if(!strcmp("backup", argv[1]))
			r = 'b';
		else if(!strcmp("restore", argv[1]))
			r = 'r';
		else if(!strcmp("delete", argv[1]))
			r = 'd';
		else if(!strcmp("frestore", argv[1]))
			r = 'f';
		else{
			fputs("Opcao invalida.\nOpcoes validas: backup; restore; delete; gc.\n", stderr);
			r = '\0';
		}
		
		if(r != '\0' && argc < 3){ /* o comando existe mas faltam argumentos */
			fprintf(stderr, "O comando '%s' precisa de pelo menos um argumento.\n", argv[1]);
			r = '\0';
		}
	}
	return r;
}

/* 
 * Responde aos sinais SIGUSR1 (sucesso) e SIGUSR2 (insucesso), enviados pelo servidor
 * de forma a indicar o resultado de uma determinada operação. O processo que recebe o sinal
 * imprime uma mensagem com base na operacao que realizou e no sinal que recebeu. Apos
 * imprimir essa mensagem, o processo sinalizado invoca _exit(0) se tiver recebido SIGUSR1
 * ou _exit(1) se tiver recebido SIGUSR2.
 */
void sighandler(int sig){
	switch(sig){ 
		case SIGUSR1: /* operacao bem sucedida */
			if(cmd_abbrev == 'b')
				printf("%s: copiado.\n", last_file);
			else if(strchr("rf", cmd_abbrev))
				printf("%s: recuperado\n", last_file);
			else if(cmd_abbrev == 'd')
				printf("%s: apagado\n", last_file);
			else if(cmd_abbrev == 'g')
				puts("gc: OK");
			else{
				puts("Nao devia estar aqui!");
				_exit(1);
			}
			_exit(0);
			break;
		case SIGUSR2: /* erro */
			if(cmd_abbrev == 'b')
				fprintf(stderr, "[ERRO] Falha a copiar '%s'\n", last_file);
			else if(strchr("rf", cmd_abbrev))
				fprintf(stderr, "[ERRO] Falha a recuperar '%s'\n", last_file);
			else if(cmd_abbrev == 'd')
				fprintf(stderr, "[ERRO] Falha a apagar '%s'\n", last_file);
			else if(cmd_abbrev == 'g')
				fputs("[ERRO] O comando 'gc' falhou", stderr);
			else
				fputs("[ERRO] Nao devia estar aqui.\n", stderr);

			_exit(1);
			break;
	}
}

int find(const char* path){
	pid_t p;
	int pipefd[2];
	int out_fd, status;

	if(pipe(pipefd) == -1)
		return -1;

	p = fork();
	if(p == 0){ /* filho */
		close(pipefd[0]);
		if(dup2(pipefd[1], 1) == -1)
			_exit(1);
		else
			close(pipefd[1]);

		execlp("find", "find", path, NULL);
		_exit(1);
	}
	else if(p == -1){
		close(pipefd[0]); close(pipefd[1]); out_fd = -1;
	}
	else{ /* pai */
		close(pipefd[1]);
		p = wait(&status);
		if(p != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0)
			out_fd = pipefd[0];
		else{ /* algo correu mal no processo filho */
			close(pipefd[0]);
			out_fd = -1;
		}
	}
	return out_fd;
}

int is_file(char* path){
	struct stat sb;

	if(stat(path, &sb) == -1){
		perror("stat");
		return 0;
	}
	return S_ISREG(sb.st_mode);
}

int is_dir(char* path){
	struct stat sb;

	if(stat(path, &sb) == -1){
		perror("stat");
		return 0;
	}
	return S_ISDIR(sb.st_mode);
}

void send_cmd_backup_dir(int fifo_fd, char* full_path){
	int read_fd = find(full_path);
	struct buffer_t* buf;
	ssize_t nr;

	if(read_fd == -1){
		perror("find");
		return;
	}

	if((buf = malloc(sizeof(struct buffer_t))) == NULL){
		perror("malloc");
		return;
	}
	if(create_buffer(read_fd, buf, MAX_PATH) == -1){
		perror("create_buffer");
		return;
	}
	while((nr = readln(buf, (void**) &full_path)) > 0){
		full_path[strcspn(full_path, "\n")] = '\0'; /* remove '\n' de full_path */
		if(is_file(full_path))
			send_cmd(fifo_fd, NULL, full_path);
	}
}

/* Faz um fork(), cria a struct comando no processo filho e envia por fifo_fd */
void send_cmd(int fifo_fd, char* arg_path, char* full_path){
	Comando cmd;

	if(nchild >= 5){ /* limita o número de processos filho */
		wait(NULL);
		--nchild;
	}

	switch(fork()){
		case 0: /* processo filho */
			last_file = (arg_path != NULL) ? arg_path : full_path;
			if(full_path)
				cmd = aloca_inicializa_comando(cmd_abbrev, full_path);
			else
				cmd = aloca_inicializa_comando(cmd_abbrev, arg_path);
			
			if(write(fifo_fd, cmd, tamanhoComando()) != tamanhoComando())
				PERROR_AND_EXIT("write")
			
			close(fifo_fd);
			free(cmd);
			pause();
			_exit(1);
		case -1:
			perror("fork");
			_exit(1);
	}
}
