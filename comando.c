#include "comando.h"
#define MAX_PATH 2048 /* Neste caso nao usamos o MAX_PATH e nao a macro PATH_MAX standard do linux para assegurarmos que a struct comando pode ser escrita de forma atomica com o getline */ 


struct comando{
	ppid_t pid; /* pid do processo que enviou o comando */
	char file_path[MAX_PATH];
	char codigoComando;
}


