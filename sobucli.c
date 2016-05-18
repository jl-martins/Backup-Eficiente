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
static char* last_file;

int validate_cmd(int argc, char* argv[]);
void send_cmd(int fifo_fd, char* arg_path, char* resolved_path);
void sighandler(int sig);

int main(int argc, char* argv[]){
	int i, fifo_fd;
	char *resolved_path;
	char backup_path[MAX_PATH];

	if(validate_cmd(argc, argv) == -1)
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
		if(resolved_path == NULL)
			perror("realpath");
		else if(strlen(resolved_path) <= MAX_PATH)
			send_cmd(fifo_fd, argv[i], resolved_path);

		free(resolved_path);
	}
	while(wait(NULL) && errno != ECHILD)
		puts("esperando");

	close(fifo_fd);
	_exit(0); /* we only reach this point if no errors occurred */
}

int validate_cmd(int argc, char* argv[]){
	int r = 0;

	if(argc == 1){
		fputs("Invalid number of arguments. Usage: sobucli cmd [FILE]...\n", stderr);
		r = -1;
	}
	else if(!strcmp("gc", argv[1])){
		if(argc == 2)
			cmd_abbrev = 'g';
		else{
			fputs("Unexpected argument after 'gc'.\n", stderr);
			r = -1;
		}
	}
	else{
		if(!strcmp("backup", argv[1]))
			cmd_abbrev = 'b';
		else if(!strcmp("restore", argv[1]))
			cmd_abbrev = 'r';
		else if(!strcmp("delete", argv[1]))
			cmd_abbrev = 'd';
		else{
			fputs("Option not found.\nValid options: backup; restore; delete; gc.\n", stderr);
			r = -1;
		}
		
		if(r == 0 && argc < 3){
			fprintf(stderr, "Option '%s' expects at least one more argument.\n", argv[1]);
			r = -1;
		}
	}
	return r; /* so chegamos aqui se o comando for valido */
}

void sighandler(int sig){ /* SIMPLIFICAR ESTA FUNÇÃO */
	switch(sig){ 
		case SIGUSR1: /* operacao bem sucedida */
			if(cmd_abbrev == 'b')
				printf("%s: copied.\n", last_file);
			else if(cmd_abbrev == 'r')
				printf("%s: restored\n", last_file);
			else if(cmd_abbrev == 'd')
				printf("%s: deleted\n", last_file);
			else if(cmd_abbrev == 'g')
				puts("gc: Success");
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

	dir = opendir(resolved_path);
	if(errno == ENOTDIR){ /* o argumento e um ficheiro */
		switch(fork()){
			case 0: /* filho */
				cmd = aloca_inicializa_comando(cmd_abbrev, resolved_path);
				if(write(fifo_fd, cmd, tamanhoComando()) != tamanhoComando())
					PERROR_AND_EXIT("write");
	
				close(fifo_fd);
				free(cmd);
				last_file = arg_path;
				pause();
				_exit(1);
			case -1:
				perror("fork");
				_exit(1);
		}
	}
	else if(dir != NULL){
		/* resolved_path is a directory. Choose what to do! */
	}
	else
		perror("opendir");
}
