#include "comando.h"
#include <dirent.h>
#include <errno.h> /* inclui ENOTDIR e outros códigos de erro */
#include <fcntl.h>
#include <limits.h> /* PATH_MAX e FILENAME_MAX */
#include <signal.h>
#include <stdio.h> /* perror() */
#include <stdlib.h>
#include <string.h> /* strcpy(), strcat() e strlen() */
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define PERROR_AND_EXIT(msg) {perror(__FILE__ ":" TOSTRING(__LINE__) ":" msg); _exit(1);}

extern int errno;

/* variables used in sighandler() for printing success/error messages */
static char cmd_abbrev = '\0';
static char* last_file = NULL;

char get_cmd_abbrev(int argc, char* argv[]);
void send_cmd(int fifo_fd, char* arg_path, char* resolved_path);
void sighandler(int sig);

int main(int argc, char* argv[]){
	int i, fifo_fd;
	char *resolved_path;
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

	for(i = 2; i < argc; ++i){
		resolved_path = realpath(argv[i], NULL);
		if(resolved_path == NULL || strlen(resolved_path) <= MAX_PATH)
			send_cmd(fifo_fd, argv[i], resolved_path);

		free(resolved_path);
	}
	while(wait(NULL) && errno != ECHILD)
		;

	close(fifo_fd);
	_exit(0); /* we only reach this point if no errors occurred */
}

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
		else{
			fputs("Opcao invalida.\nOpcoes validas: backup; restore; delete; gc.\n", stderr);
			r = '\0';
		}
		
		if(r != '\0' && argc < 3){ /* o comando existe mas precisa de argumentos */
			fprintf(stderr, "O comando '%s' precisa de pelo menos um argumento.\n", argv[1]);
			r = '\0';
		}
	}
	return r;
}

void sighandler(int sig){
	switch(sig){ 
		case SIGUSR1: /* operacao bem sucedida */
			if(cmd_abbrev == 'b')
				printf("%s: copiado.\n", last_file);
			else if(cmd_abbrev == 'r')
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
				fprintf(stderr, "[ERROR] Failure copying '%s'\n", last_file);
			else if(cmd_abbrev == 'r')
				fprintf(stderr, "[ERROR] Failure restoring '%s'\n", last_file);
			else if(cmd_abbrev == 'd')
				fprintf(stderr, "[ERROR] Failure deleting '%s'\n", last_file);
			else if(cmd_abbrev == 'g')
				fputs("[ERROR] gc failed", stderr);
			else
				fputs("[ERROR] Nao devia estar aqui.\n", stderr);

			_exit(1);
			break;
	}
}

void send_cmd(int fifo_fd, char* arg_path, char* resolved_path){
	DIR* dir;
	Comando cmd;

	if(cmd_abbrev == 'b'){
		if(resolved_path == NULL){ /* nao e possivel fazer backup sem um caminho absoluto */
			perror(arg_path);
			return;
		}
		else
			dir = opendir(resolved_path);
	}

	if(cmd_abbrev != 'b' || errno == ENOTDIR){
		switch(fork()){
			case 0: /* processo filho */
				last_file = arg_path;
				cmd = aloca_inicializa_comando(cmd_abbrev, resolved_path);
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
	else if(dir != NULL){
		puts("sou uma diretoria");
		/* resolve_path e uma diretoria. Escolher o que fazer! */
	}
	else
		perror("opendir");
}
