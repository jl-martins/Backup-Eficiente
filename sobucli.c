#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <limits.h> /* PATH_MAX e FILENAME_MAX */
#include <stdio.h> /* perror() */
#include <stdlib.h>
#include <string.h> /* memcpy() e strlen() */

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
void send_cmd(int fifo_fd, int argc, char* argv[], char cmd_abbrev);
void sighandler(int sig);

int main(int argc, char* argv[]){
	int i, fifo_fd;
	char base_path[PATH_MAX];
	char cmd_abbrev, *resolved_path;

	cmd_abbrev = get_cmd_abbrev(argv[1]);
	if(validate_cmd(argc, cmd, cmd_abbrev) == 0)
		_exit(1);

	strcpy(base_path, getenv("HOME"));
	strcat(base_path, "/.Backup/fifo");
	fifo_fd = open(base_path, O_WRONLY);
	if(fifo_fd == -1)
		PERROR_AND_EXIT("open")

	if(signal(SIGUSR1, sighandler) == SIG_ERR || signal(SIGUSR2, sighandler) == SIG_ERR)
		PERROR_AND_EXIT("signal")

	for(i = 2; i < argc; ++i){
		resolved_path = realpath(argv[i], NULL);
		if(resolved_path == NULL)
			perror("realpath");
		else{
			send_cmd(fifo_fd, cmd_abbrev, resolved_path);
			free(resolved_path);
		}
	}
	_exit(0); /* we only reach this point if no errors occurred */
}

void sighandler(int sig){ /* SIMPLIFICAR ESTA FUNÇÃO */
	switch(sig){ 
		case SIGUSR1:
			if(last_cmd == 'b')
				printf("%s: copied.\n", last_file);
			else if(last_cmd == 'r')
				printf("%s: restored\n", last_file);
			else if(last_cmd == 'd')
				printf("%s: deleted\n", last_file);
			else /* last_cmd == 'g' */
				puts("gc: Success");
		case SIGUSR2:
			if(last_cmd == 'b')
				printf("[ERROR] Failure copying '%s'\n", last_file);
			else if(last_cmd == 'r')
				printf("[ERROR] Failure restoring '%s'\n", last_file);
			else if(last_cmd == 'd')
				printf("[ERROR] Failure deleting '%s'\n", last_file);
			else /* last_cmd == 'g' */
				puts("[ERROR] gc failed");
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

void send_cmd(int fifo_fd, char cmd_abbrev, const char* resolved_path){
	pid_t p;
	DIR* dir;

	/* Não escrever '\0'!!! nas Strings */
	dir = opendir(resolved_path);
	if(errno == ENOTDIR){ /* user entered a file */
		p = fork();
		switch(p){
			case 0: /* I'm the child */
				last_cmd = cmd_abbrev;
				last_file = resolved_path;
				pause();
				_exit(0);
				break;
			case -1:
				perror("fork");
				_exit(1);
			default: /* I'm the parent */
				cmd = create_cmd(p, resolved_path);
				if(write(fifo_fd, cmd, sizeof(struct cmd_s)) == -1)
					kill(p, SIGINT);
	}
	else if(dir != NULL){
		/* resolved_path is a directory. Choose what to do! */
	}
	else
		perror("opendir");
}

