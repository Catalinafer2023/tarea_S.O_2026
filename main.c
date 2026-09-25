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
int parsearCmd(char *usuario, char *retorno[]) {
	int nPalabra = 0;
	int dentroPalabra = 0;
	// Se recorre cada caracter del input del usuario y se añade uno a uno a cada fila del retorno
    for(int i = 0; usuario[i] != '\0'; i++) {
        if (usuario[i] == ' ' || usuario[i] == '\n') {
			usuario[i] = '\0';
			dentroPalabra = 0;
		} else {
			if(!dentroPalabra) {
				retorno[nPalabra++] = &usuario[i];
			}
			dentroPalabra = 1;
		}
    }
	retorno[nPalabra] = NULL;

	return nPalabra;
}

void comandoBackground(char *parseado[]) {
	pid_t pid = fork();

	if(pid == 0) {
		if(setsid() < 0) {perror("Error");}

		signal(SIGHUP, SIG_IGN);
		pid = fork();
		if(pid < 0) perror("Error");
		if(pid > 0) exit(0);

		chdir("/");
		close(0); close(1); close(2);

		// IMPORTANTE : Hace falta eliminar '&' de parseado para ejecutar execvp correctamente.
		//execvp(parseado[0], parseado);
		char *sample[] = {"sleep", "5", NULL}; // Arreglo de strings de prueba, eliminar mas tarde
		// IMPORTANTE : Se debe imprimir y almacenar el job(?) y PID del proceso de fondo creado
		execvp(sample[0], sample);
		perror("Error");
		exit(1);
	}
	else if (pid < 0) {
		perror("Error");
	}
}

void comandoExterno(char *parseado[]) {
	pid_t pid = fork();

	if(pid == 0) {
		execvp(parseado[0], parseado);
		perror("Error");
		exit(1);
	}
	else if (pid > 0) {
		int status;
		waitpid(pid, &status, 0);
	}
	else {
		perror("Error");
	}
}

int main(){
	char args[100];			// Input completo del usuario
	char *parseado[100];	// Array del input por palabra

	// Bucle principal de la shell
	while(1) {
		dirprint();
		fgets(args, sizeof(args), stdin); // Lee input desde stdin y lo guarda en args
		args[strcspn(args, "\n")] = '\0'; // Reemplaza el primer salto de línea por ser el final de String

		// Manejador que revisa que haya un único & al final del input.
		// IMPORTANTE : seguramente esto debería en realidad revisarse como la última palabra de parseado (una vez se defina este)
		int correrEnBackg = 0;
		if(args[strcspn(args, "&")] != '\0' && args[strcspn(args, "&") + 1] == '\0')
			correrEnBackg = 1;

		int nLineas = parsearCmd(args, parseado);

		if (nLineas > 0) {
			// Comando 'cd [dir]', cambia el directorio al especificado, sin argumentos devuelve a $HOME
			if(strcmp(parseado[0], "cd") == 0) {
				if(nLineas > 1) {
					// Buffer 'dir' para considerar espacios como nombres de directorios en vez de un argumento nuevo
					char dir[100] = {'\0'};
					strcat(dir, parseado[1]);
					for(int i = 2; i < nLineas; i++){
						strcat(dir, " ");
						strcat(dir, parseado[i]);
					}

					if(chdir(dir) != 0)
						perror("Error");
				} else {
					if(chdir(getenv("HOME")) != 0) // Nota: Esto lleva a home/[user]/, no es claro si en vez debería retornar a home/
						perror("Error");
				}
			}

			// Comando 'exit [n]', retorna con valor n, si no se ingresa nada o algo que no es un número retorna con 0
			else if (strcmp(parseado[0], "exit") == 0) {
				if (nLineas > 1) {
					if (atoi(parseado[1]) != 0) {
						printf("return con %d\n", atoi(parseado[1]));
						return atoi(parseado[1]);
					}
				}
				printf("return con 0\n");
				return 0;
			}

			// Si no se reconoce ningún comando interno, se ejecutará un comando externo con fork + execvp
			// Si se identifica un &, se correrá como proceso de fondo
			else {
				if(correrEnBackg) {
					comandoBackground(parseado);
				}
				else comandoExterno(parseado);
			}
		}
	}
}