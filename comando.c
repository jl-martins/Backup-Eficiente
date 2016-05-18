#include "comando.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define MAX_PATH 2048 /* Neste caso nao usamos o MAX_PATH e nao a macro PATH_MAX standard do linux para assegurarmos que a struct comando pode ser escrita de forma atomica com o getline */ 


struct comando{
	pid_t pid; /* pid do processo que enviou o comando */
	char filepath[MAX_PATH];
	char codigoComando;
}

Comando aloca_comando(){
	return malloc(sizeof(struct comando));
}

Comando aloca_inicializa_comando(char codigoComando, char * filepath){
	Comando new = malloc(sizeof(struct comando));
	if(new == NULL)
		return NULL;
	strcpy(new->filepath, filepath);	
	new->pid = getpid();
	new->codigoComando = codigoComando;
	return new;
}

/* nenhum dos getters funciona com null */

pid_t get_pid_comando(Comando cmd){
	return cmd->pid;
}

char * get_filepath(Comando cmd){
	char * str = malloc((strlen(cmd->filepath) + 1) * sizeof(char));	
	if(str)
		strcpy(str, cmd->filepath);
	return str;
}

char get_codigoComando(Comando cmd){
	return cmd->codigoComando;
}

size_t tamanhoComando(){
	return sizeof(struct comando);
}
