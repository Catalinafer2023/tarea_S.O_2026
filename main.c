#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

// Función que imprime la dirección actual de la shell
void dirprint(){
	char cwd[1024];
	printf("\nDIR:%s:~$ ", getcwd(cwd, sizeof(cwd))); 
}

// Separa el String recibido del usuario en un array de Strings por cada palabra
// Devuelve la cantidad de palabras que tiene el input del usuario
int parsearCmd(char *usuario, char retorno[100][100]) {
	int nPalabra = 0;
	int nCaracter = 0;
	int dentroPalabra = 0;
	// Se recorre cada caracter del input del usuario y se añade uno a uno a cada fila del retorno
    for(int i = 0; usuario[i] != '\0'; i++){
        if (usuario[i] == ' ' || usuario[i] == '\n') {
			if(dentroPalabra) {
				usuario[i] = '\0';
				retorno[nPalabra][nCaracter] = '\0';
				nCaracter = 0; nPalabra++;
				dentroPalabra = 0;
			}
		} else {
			retorno[nPalabra][nCaracter++] = usuario[i];
			dentroPalabra = 1;
		}
    }
	retorno[nPalabra][nCaracter] = '\0';

	return nPalabra + (dentroPalabra ? 1 : 0);
}

//toma las pipes y las parsea como comandos normales antes de
//ejecutarlas
void execPipe(char pipes[100][100], int nCommands){
	int previousRead = -1;
	pid_t children[100];
	int nChildren = 0;

	for(int i = 0; i < nCommands; i++){
		int fd[2] = {-1, -1};
		int hasNext = i < nCommands - 1;
		if(hasNext && pipe(fd) < 0){
			perror("pipe");
			if(previousRead != -1) close(previousRead);
			for(int j = 0; j < nChildren; j++) waitpid(children[j], NULL, 0);
			return;
		}

		pid_t pid = fork();
		if(pid < 0){
			perror("fork");
			if(previousRead != -1) close(previousRead);
			if(hasNext){
				close(fd[0]);
				close(fd[1]);
			}
			for(int j = 0; j < nChildren; j++) waitpid(children[j], NULL, 0);
			return;
		}
		if(pid == 0){
			if(previousRead != -1) dup2(previousRead, STDIN_FILENO);
			if(hasNext) dup2(fd[1], STDOUT_FILENO);
			if(previousRead != -1) close(previousRead);
			if(hasNext){
				close(fd[0]);
				close(fd[1]);
			}

			char cmd[100][100];
			char *argv[101];
			int argc = parsearCmd(pipes[i], cmd);
			for(int j = 0; j < argc; j++) argv[j] = cmd[j];
			argv[argc] = NULL;
			if(argc == 0 || execvp(argv[0], argv) < 0) perror("error comando");
			exit(1);
		}

		children[nChildren++] = pid;
		if(previousRead != -1) close(previousRead);
		previousRead = -1;
		if(hasNext){
			close(fd[1]);
			previousRead = fd[0];
		}
	}

	for(int i = 0; i < nChildren; i++) waitpid(children[i], NULL, 0);
}

//función para correr comandos normales
void normCommand(char parseado[100][100]){
	char *args[] = {NULL};
	pid_t pid = fork();
	if(pid == 0){
		if(execvp(parseado[0], args) < 0){
			perror("error");
		}
		exit(1);
	}
	wait(NULL);
}

// toma los comandos y los separa por pipes
int parsearPipe(char *usuario, char pipes[100][100]){
	int command = 0;
	int character = 0;
	for(int i = 0; usuario[i] != '\0' && usuario[i] != '\n'; i++){
		if(usuario[i] == '|'){
			pipes[command][character] = '\0';
			command++;
			character = 0;
		} else {
			pipes[command][character++] = usuario[i];
		}
	}
	pipes[command][character] = '\0';
	return command + 1;
}

int main(){
	char args[100];				// Input completo del usuario
	char parseado[100][100];	// Array del input por palabra
	char pipes[100][100];
	// Bucle principal de la shell
	while(1) {
		dirprint();
		fgets(args, sizeof(args), stdin); // Lee input desde stdin y lo guarda en args
		args[strcspn(args, "\n")] = '\0'; // Reemplaza el primer salto de línea por ser el final de String
		if(strchr(args,'|') == 0){
			parsearCmd(args,parseado);
			if(strcmp(parseado[0], "exit") == 0) {
				printf("exit\n");
				if (atoi(parseado[1]) != 0) {
					printf("return con %d\n", atoi(parseado[1]));
					return atoi(parseado[1]);
				}
				printf("return con 0\n");
				return 0;
			}
			normCommand(parseado);
		}
		else{
			int nCommands = parsearPipe(args,pipes);
			execPipe(pipes, nCommands);
		}
	}
}