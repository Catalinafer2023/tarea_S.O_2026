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

int main(){
	char args[100];				// Input completo del usuario
	char parseado[100][100];	// Array del input por palabra

	// Bucle principal de la shell
	while(1) {
		dirprint();
		fgets(args, sizeof(args), stdin); // Lee input desde stdin y lo guarda en args
		args[strcspn(args, "\n")] = '\0'; // Reemplaza el primer salto de línea por ser el final de String
		int nLineas = parsearCmd(args, parseado);

		// Comando 'exit [n]', retorna con valor n, si no se ingresa nada o algo que no es un número retorna con 0
		if(strcmp(parseado[0], "exit") == 0) {
			if (atoi(parseado[1]) != 0) {
				printf("return con %d\n", atoi(parseado[1]));
				return atoi(parseado[1]);
			}
			printf("return con 0\n");
			return 0;
		}
	}
}