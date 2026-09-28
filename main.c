#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#define MAX_JOBS 64

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

// Manejador que revisa que haya un & al final de un comando y lo quita de la frase
int identificarBackground(char *frase[], int nLineas) {
	int background = 0;
	if(strcspn(frase[nLineas-1], "&") == '\0') {
		background = 1;
		frase[nLineas-1] = NULL;
	}
	return background;
}

typedef struct{
	pid_t pid;
	char comando[256];
	int activo;
} Job;

Job jobs[MAX_JOBS];
volatile sig_atomic_t pmon_flag = 0;
volatile sig_atomic_t pmon_stop = 0;

void sigalrm_handler(int sig){
	(void)sig; // Evita advertencias de compilación
	pmon_flag = 1;
}

void pmon_sigint_handler(int sig){
	(void)sig; // Evita advertencias de compilación
	pmon_stop = 1;
}

void sigchld_handler(int sig){
	(void)sig;
	pid_t pid;
	int status;

	while((pid=waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		
		for(int i=0; i<MAX_JOBS; i++){
			if(jobs[i].activo && jobs[i].pid==pid) {
				printf("[%d]+ Done %s\n", i+1, jobs[i].comando);
				jobs[i].activo=0;
				break;
			}
		}
	}
}

void agregaJob(pid_t pid, const char *comando){
	for(int i=0; i<MAX_JOBS; i++){
		if(!jobs[i].activo){
			jobs[i].pid=pid;
			strncpy(jobs[i].comando, comando, sizeof(jobs[i].comando)-1);
			jobs[i].comando[sizeof(jobs[i].comando)-1]='\0';
			jobs[i].activo=1;

			printf("[%d] %d\n", i+1, pid);
			return;
		}
	}
	printf("No hay espacio para más jobs\n");
}

void listaJobs(){
	for(int i=0; i<MAX_JOBS; i++){
		if(jobs[i].activo){
			printf("[%d] Se esta ejecutando: %s\n", i+1, jobs[i].comando);
		}
	}
}

// Función que maneja todo tipo de comando externo a la shell, recibe el arreglo de strings de comando y un int 0 (false) o 1 (true)
// Para identificar si se debe correr en el background o no
void comandoExterno(char *comando[], int correrEnBackg) {
	// No se identificó que se quiera ejecutar en el background así que se ejecuta un fork + execvp normal
	if (correrEnBackg == 0) {
		pid_t pid = fork();

		if(pid == 0) {

			struct sigaction sa_default;
            sa_default.sa_handler = SIG_DFL;
            sigemptyset(&sa_default.sa_mask);
            sa_default.sa_flags = 0;
            sigaction(SIGINT, &sa_default, NULL); //Interrumpir hijo con Ctrl+C
			sigaction(SIGQUIT, &sa_default, NULL); //Core dump hijo con Ctrl+"\"
			sigaction(SIGTSTP, &sa_default, NULL); //Suspender hijo con Ctrl+Z

			execvp(comando[0], comando);
			perror("Error");
			exit(1);
		}
		else if (pid > 0) {
			int status;
			waitpid(pid, &status, WUNTRACED);
			if(WIFSTOPPED(status)){ // Si el proceso hijo fue detenido, se agrega a la lista de jobs
				char buffer[256] = {'\0'};
				strcat(buffer, comando[0]);
				for(int i = 1; comando[i] != NULL; i++){
					strcat(buffer, " ");
					strcat(buffer, comando[i]);
				}
				agregaJob(pid, buffer);
			}
		}
		else {
			perror("Error");
		}
	}
	// Si se identifica un & al final, entonces se ejecutará en el background
	else if (correrEnBackg == 1) {
		pid_t pid = fork();

		if(pid == 0) {
			//Ignorar señales SIGINT, SIGQUIT y SIGTSTP en el proceso hijo para que no se cierre con Ctrl+C, Ctrl+\ o Ctrl+Z
			struct sigaction sa_ign;
			sa_ign.sa_handler = SIG_IGN;
			sigemptyset(&sa_ign.sa_mask);
			sa_ign.sa_flags = 0;
			sigaction(SIGINT, &sa_ign, NULL);
			sigaction(SIGQUIT, &sa_ign, NULL);
			sigaction(SIGTSTP, &sa_ign, NULL);


			execvp(comando[0], comando);
			perror("Error");
			exit(1);
		} else if (pid > 0) {
			// Buffer para reconstruir el comando con espacios en un solo string
			char buffer[256] = {'\0'};
			strcat(buffer, comando[0]);
			for(int i = 1; comando[i] != NULL; i++){
				strcat(buffer, " ");
				strcat(buffer, comando[i]);
			}

			agregaJob(pid, buffer);
		}
		else if (pid < 0) {
			perror("Error");
		}
	}
}

typedef struct{
	unsigned long long utime;
	unsigned long long stime;
} ProcTime;

int obtenerTiempoProceso(pid_t pid, ProcTime *pt, char *estado){
	char path[256];
	snprintf(path, sizeof(path), "/proc/%d/stat", pid);
	FILE *f= fopen(path, "r");
	if(!f) return 0;
	char comm[256];
	fscanf(f, "%*d %s %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu", comm, estado, &pt->utime, &pt->stime);
	fclose(f);
	return 1;
}

//Leer memoria residente RSS
long obtenerRssProc(pid_t jobPid) {
	char path[256], line[256];
	snprintf(path, sizeof(path), "/proc/%d/status", jobPid);
	FILE *f = fopen(path, "r");
	if (!f) return 0;

	long rss = 0;
	while (fgets(line, sizeof(line), f)) {
		if (strncmp(line, "VmRSS:", 6) == 0) {
			sscanf(line + 6, "%ld", &rss);
			break;
		}
	}
	fclose(f);
	return rss;
}

void ejecutarPmon(int segundos) {
    struct sigaction sa_old_int, sa_new_int, sa_alarm;

    // Temporalmente restaurar SIGINT para salir de pmon con ctrl+C sin cerrar la shell
    sa_new_int.sa_handler = pmon_sigint_handler;
    sigemptyset(&sa_new_int.sa_mask);
    sa_new_int.sa_flags = 0;
    sigaction(SIGINT, &sa_new_int, &sa_old_int);

    // Configurar alarma con alarm() y SIGALRM
    sa_alarm.sa_handler = sigalrm_handler;
    sigemptyset(&sa_alarm.sa_mask);
    sa_alarm.sa_flags = 0;
    sigaction(SIGALRM, &sa_alarm, NULL);

    ProcTime prev_times[MAX_JOBS] = {0};
    long ticks_per_sec = sysconf(_SC_CLK_TCK);

	pmon_stop = 0;

    while (!pmon_stop) {
        alarm(segundos);
        pmon_flag = 0;

        printf("\033[H\033[J"); // Limpiar pantalla
        printf("%-8s | %-15s | %-12s | %-10s | %-8s\n", "PID", "COMANDO", "ESTADO", "%CPU(aprox)", "RSS (KB)");
        printf("-------------------------------------------------------------------\n");

        for (int i = 0; i < MAX_JOBS; i++) {
            if (jobs[i].activo) {
                ProcTime curr;
                char state_char;

                if (!obtenerTiempoProceso(jobs[i].pid, &curr, &state_char)) {
                    jobs[i].activo = 0; // Si desapareció de /proc, retirar
                    continue;
                }

                long rss = obtenerRssProc(jobs[i].pid);
                const char *estado_str = "ejecutando";
                if (state_char == 'S') estado_str = "durmiendo";
                else if (state_char == 'Z') estado_str = "zombie";
                else if (state_char == 'T') estado_str = "detenido";

                // Cálculo de %CPU según deltas
                unsigned long long delta = (curr.utime + curr.stime) - (prev_times[i].utime + prev_times[i].stime);
                double cpu_usage = (100.0 * (delta / (double)ticks_per_sec)) / segundos;
                prev_times[i] = curr;

                printf("%-8d | %-15s | %-12s | %-10.1f | %-8ld\n", 
                       jobs[i].pid, jobs[i].comando, estado_str, cpu_usage, rss);
            }
        }

        pause(); // Espera la señal de la alarma o Ctrl+C
    }

    alarm(0);
    sigaction(SIGINT, &sa_old_int, NULL);
    printf("\nSaliendo de pmon...\n");
}

int main(){
	struct sigaction sa_chld, sa_sign; // Estructuras para manejar señales
	sa_sign.sa_handler=SIG_IGN;
	sigemptyset(&sa_sign.sa_mask);
	sa_sign.sa_flags=0;
	sigaction(SIGINT, &sa_sign, NULL); // Ignorar Ctrl+C en la shell
	sigaction(SIGQUIT, &sa_sign, NULL); // Ignorar Ctrl+\ en la shell
	sigaction(SIGTSTP, &sa_sign, NULL); // Ignorar Ctrl+Z en la shell
	sa_chld.sa_handler=sigchld_handler;
	sigemptyset(&sa_chld.sa_mask);
	sa_chld.sa_flags=SA_RESTART | SA_NOCLDSTOP	;
	sigaction(SIGCHLD, &sa_chld, NULL);

	char args[100];			// Input completo del usuario
	char *parseado[100];	// Array del input por palabra

	// Bucle principal de la shell
	while(1) {
		dirprint();
		fgets(args, sizeof(args), stdin); // Lee input desde stdin y lo guarda en args
		args[strcspn(args, "\n")] = '\0'; // Reemplaza el primer salto de línea por ser el final de String

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

			// Comando "pmon [segundos]" para monitorear procesos, manejado en la función ejecutarPmon [[ WIP ]]
			else if (strcmp(parseado[0], "pmon") == 0) {
				if (nLineas > 1) {
					if(atoi(parseado[1]) != 0) {
						ejecutarPmon(atoi(parseado[1]));
					} else
						printf("Ingresar cantidad de segundos mayor a 0. Ej: 'pmon 2'\n");
				} else
					ejecutarPmon(2);
			}

			// Comando "jobs" para recibir una lista de los trabajos en background
			else if (strcmp(parseado[0], "jobs") == 0) {
				listaJobs();
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

			// Si no se reconoce ningún comando interno, se ejecutará un comando externo
			// La función identificarBackground decide si se debe ejectuar en el background o no
			else {
				comandoExterno(parseado, identificarBackground(parseado, nLineas));
			}
		}
	}
}