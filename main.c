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
char** parsearCmd(char *usuario, char *retorno) {
    char *parseado[50];
    for(int i = 0; usuario[i] != '\0'; i++){
        // [Por implementar]
    }
    return NULL;
}

int main(){
	char args[100];		// Input completo del usuario
	char **parseado;	// Array del input por palabra
	char *space = " ";
	// Bucle principal de la shell
	while(1) {
		dirprint();
		fgets(args, sizeof(args), stdin); // Lee input desde stdin y lo guarda en args
		args[strcspn(args, "\n")] = '\0'; // 
		if(strcmp(args, "exit") == 0) {
			return 0;
		}
	}
}