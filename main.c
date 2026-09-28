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

	// Se añade 1 para compensar por el index 0
	return nPalabra + 1;
}

//toma las pipes y las parsea como comandos normales antes de
//ejecutarlas
void execPipe(char pipes[100][100]){
	char *args[] = {NULL};
	char *args2[] = {NULL};
	char cmd[100][100];
	char cmd2[100][100];
	int fd[2];
	pipe(fd);
	pid_t pipe1 = fork();
	if(pipe1 == 0){
		dup2(fd[1], STDOUT_FILENO);
		close(fd[0]);
		close(fd[1]);
		parsearCmd(pipes[0],cmd);
		if(execvp(cmd[0], args) < 0){
			perror("error cmd1");
		}
		exit(1);
	}
	pid_t pipe2 = fork();
	if(pipe2 == 0){
		dup2(fd[0],STDIN_FILENO);
		close(fd[1]);
		close(fd[0]);
		parsearCmd(pipes[1],cmd2);
		if(execvp(cmd2[0], args2) < 0){
			perror("error cmd2");
		}
		exit(1);
	}
	close(fd[0]);
    close(fd[1]);
	wait(NULL);
	wait(NULL);
}

//función para correr comandos normales
int normCommand(char parseado[100][100]){
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
void parsearPipe(char *usuario, char pipes[100][100]){
	int j = 0, x = 0,z = 0;
	int contPipe = 0;
	for(int i = 0; usuario[i] != '\0'; i++){
		if(usuario[i] == '|'){
			contPipe++;
		}
	}
		while(usuario[j] != '\0' && usuario[j] != '\n'){
			if(usuario[j] == '|'){
				z++;
				j++;
				x = 0;
			}
			pipes[z][x] = usuario[j];
			x++;
			j++;
		}
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
			parsearPipe(args,pipes);
			execPipe(pipes);
		}
	}
}