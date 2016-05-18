#ifndef __COMANDO__
	#define __COMANDO__
	#define MAX_PATH 2048 /* comprimento maximo do path que aceitamos nos comandos */
	#include <sys/types.h>
	typedef struct comando * Comando;
	Comando aloca_comando();
	Comando aloca_inicializa_comando(char codigoComando, char * caminho_ficheiro);
	pid_t get_pid_comando(Comando cmd);
	char * get_filepath(Comando cmd);
	char get_codigoComando(Comando cmd);
	ssize_t tamanhoComando();
#endif
