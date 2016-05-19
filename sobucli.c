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

/* variables used in sighandler() for printing success/error messages */
static char cmd_abbrev = '\0';
static char* last_file = NULL;

char get_cmd_abbrev(int argc, char* argv[]);
void send_cmd(int fifo_fd, char* arg_path);
void send_cmd_full_path(int fifo_fd, char* arg_path, char* full_path);
void sighandler(int sig);

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

	if(cmd_abbrev == 'b' || cmd_abbrev == 'f'){
		char* full_path;

		for(i = 2; i < argc; ++i){
			full_path = realpath(argv[i], NULL);
			if(full_path == NULL)
				perror(argv[i]);
			else
				send_cmd_full_path(fifo_fd, argv[i], full_path);
			free(full_path);
		}
	}
	else if(cmd_abbrev == 'g')
		send_cmd(fifo_fd, ""); /* gc nao tem argumentos, daí a string vazia "" */
	else /* restore ou delete */
		for(i = 2; i < argc; ++i)
			send_cmd(fifo_fd, argv[i]);

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
		else if(!strcmp("fbackup", argv[1]))
			r = 'f';
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
				fprintf(stderr, "[ERROR] Falha a copiar '%s'\n", last_file);
			else if(strchr("rf", cmd_abbrev))
				fprintf(stderr, "[ERROR] Falha a recuperar '%s'\n", last_file);
			else if(cmd_abbrev == 'd')
				fprintf(stderr, "[ERROR] Falha a apagar '%s'\n", last_file);
			else if(cmd_abbrev == 'g')
				fputs("[ERROR] O comando 'gc' falhou", stderr);
			else
				fputs("[ERROR] Nao devia estar aqui.\n", stderr);

			_exit(1);
			break;
	}
}

void send_cmd_full_path(int fifo_fd, char* arg_path, char* full_path){
	Comando cmd;
	struct stat buf;

	if(stat(full_path, &buf) == -1){
		perror("stat");
		return;
	}

	if(S_ISREG(buf.st_mode)){ /* backup de um ficheiro */
		switch(fork()){
			case 0:
				last_file = arg_path;
				cmd = aloca_inicializa_comando(cmd_abbrev, full_path);
				if(write(fifo_fd, cmd, tamanhoComando()) != tamanhoComando())
					PERROR_AND_EXIT("write")

				close(fifo_fd);
				pause();
				_exit(1);
			case -1:
				perror("fork");
				_exit(1);
		}
	}
	else if(S_ISDIR(buf.st_mode)){ /* backup de uma diretoria */
		int len;
		struct dirent* dp;
		DIR* dir = opendir(full_path);
		
		if(dir == NULL){
			perror("opendir");
			return;
		}

		len = strlen(full_path);
		full_path[len++] = '/';
		while((dp = readdir(dir)) != NULL){
			strcpy(&full_path[len], dp->d_name);
			send_cmd_full_path(fifo_fd, full_path, full_path);
		}
	}
	else
		fprintf(stderr, "Erro: '%s' não é um ficheiro nem uma diretoria.\n", arg_path);
}

void send_cmd(int fifo_fd, char* arg_path){
	Comando cmd;

	switch(fork()){
		case 0: /* processo filho */
			last_file = arg_path;
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
