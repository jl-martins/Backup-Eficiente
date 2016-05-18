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

#define BUFF_SIZE 4096

extern int errno;

/* variables used in sighandler() for printing success/error messages */
static char last_cmd;
static char* last_file;

char get_cmd_abbrev(const char* cmd);
int validate_cmd(int argc, const char* cmd, char cmd_abbrev);
void send_cmd(int fifo_fd, char cmd_abbrev, char* resolved_path);
void sighandler(int sig);

int main(int argc, char* argv[]){
	int i, fifo_fd/*, err_fd*/;
	/*char err_path[MAX_PATH];*/
	char backup_path[MAX_PATH];
	char cmd_abbrev, *resolved_path;

	cmd_abbrev = (argc > 1) ? get_cmd_abbrev(argv[1]) : '\0';
	if(validate_cmd(argc, argv[1], cmd_abbrev) == 0)
		_exit(1);

	strcpy(backup_path, getenv("HOME"));
	strcat(backup_path, "/.Backup");
	/*strcpy(err_path, backup_path);
	strcat(err_path, "/err_log.txt");*/
	strcat(backup_path, "/fifo");

	/*err_fd = open("err_log.txt", O_WRONLY | O_APPEND);
	if(err_fd == -1)
		PERROR_AND_EXIT("open")
	

	if(dup2(err_fd, 2) == -1)
		PERROR_AND_EXIT("dup2")
	else
		close(err_fd);*/

	fifo_fd = open(backup_path, O_WRONLY);
	if(fifo_fd == -1)
		PERROR_AND_EXIT("open")

	if(signal(SIGUSR1, sighandler) == SIG_ERR || signal(SIGUSR2, sighandler) == SIG_ERR)
		PERROR_AND_EXIT("signal")

	for(i = 2; i < argc; ++i){
		resolved_path = realpath(argv[i], NULL);
		if(resolved_path == NULL)
			perror("realpath");
		else if(strlen(resolved_path) <= MAX_PATH){
			send_cmd(fifo_fd, cmd_abbrev, resolved_path);
			free(resolved_path);
		}
	}
	while(wait(NULL) && errno != ECHILD)
		;

	close(fifo_fd);
	_exit(0); /* we only reach this point if no errors occurred */
}

void sighandler(int sig){ /* SIMPLIFICAR ESTA FUNÇÃO */
	switch(sig){ 
		case SIGUSR1: /* successful operation */
			if(last_cmd == 'b')
				printf("%s: copied.\n", last_file);
			else if(last_cmd == 'r')
				printf("%s: restored\n", last_file);
			else if(last_cmd == 'd')
				printf("%s: deleted\n", last_file);
			else /* last_cmd == 'g' */
				puts("gc: Success");
			break;
		case SIGUSR2: /* failed operation */
			if(last_cmd == 'b'){
				printf("[ERROR] Failure copying '%s'\n", last_file);
				fprintf(stderr, "[ERROR] Failure copying '%s'\n", last_file);
			}
			else if(last_cmd == 'r'){
				printf("[ERROR] Failure restoring '%s'\n", last_file);
				fprintf(stderr, "[ERROR] Failure restoring '%s'\n", last_file);
			}
			else if(last_cmd == 'd'){
				printf("[ERROR] Failure deleting '%s'\n", last_file);
				fprintf(stderr, "[ERROR] Failure deleting '%s'\n", last_file);
			}
			else{ /* last_cmd == 'g' */
				puts("[ERROR] gc failed");
				fputs("[ERROR] gc failed", stderr);
			}
			break;
	}
}

/* Returns a command abbreviation for the given command or '\0' on invalid command. */
char get_cmd_abbrev(const char* cmd){
	char cmd_abbrev = '\0';

	if(!strcmp(cmd, "backup"))
		cmd_abbrev = 'b';
	else if(!strcmp(cmd, "restore"))
		cmd_abbrev = 'r';
	else if(!strcmp(cmd, "delete"))
		cmd_abbrev = 'd';
	else if(!strcmp(cmd, "gc"))
		cmd_abbrev = 'g';

	return cmd_abbrev;
}

int validate_cmd(int argc, const char* cmd, char cmd_abbrev){
	int r = 0;

	if(argc == 1){
		fputs("Invalid number of arguments. Usage: sobucli cmd [FILE]...\n", stderr);
	}
	else if(cmd_abbrev == '\0'){
		fputs("Option not found.\nValid options: backup; restore; delete; gc.\n", stderr);
	}
	else if(strchr("brd", cmd_abbrev) && argc < 3){
		fprintf(stderr, "Option '%s' expects at least one more argument.\n", cmd);
	}
	else if(cmd_abbrev == 'g' && argc > 2){
		fputs("Unexpected argument after 'gc'.\n", stderr);
	}
	else /* cmd is valid */
		r = 1;

	return r;
}

void send_cmd(int fifo_fd, char cmd_abbrev, char* resolved_path){
	DIR* dir;
	Comando cmd;

	dir = opendir(resolved_path);
	if(errno == ENOTDIR){ /* user entered a file */
		switch(fork()){
			case 0: /* I'm the child */
				cmd = aloca_inicializa_comando(cmd_abbrev, resolved_path);
				if(write(fifo_fd, cmd, tamanhoComando()) == -1)
					PERROR_AND_EXIT("write");
				last_cmd = cmd_abbrev;
				last_file = resolved_path;
				pause();
				_exit(0);
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
