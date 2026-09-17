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
	/*
		[[ TODO: Se requiere que las pipes sean de límite arbitrario por lo que parseado debe estar definido como 'char *parseado[100]' ]]
	 */
	char parseado[100][100];	// Array del input por palabra

	// Bucle principal de la shell
	while(1) {
		dirprint();
		fgets(args, sizeof(args), stdin); // Lee input desde stdin y lo guarda en args
		args[strcspn(args, "\n")] = '\0'; // Reemplaza el primer salto de línea por ser el final de String
		int nLineas = parsearCmd(args, parseado);

		if(strcmp(args, "exit") == 0) {
			return 0;
		}
	}
}