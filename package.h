#ifndef _PACOTE_
#define _PACOTE_
	typedef struct package * PACKAGE;
	PACKAGE newPACKAGE(int isComand, pid_t pidSender);
	void deletePACKAGE(PACKAGE package);
	ssize_t writePackageOnFile(int fd, PACKAGE package);
	ssize_t readPackageFromFile(int fd, PACKAGE package);
#endif
