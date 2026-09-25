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

typedef struct{
	pid_t pid;
	char comando[256];
	int activo;
} Job;

Job jobs[MAX_JOBS];
volatile sig_atomic_t pmon_flag = 0;

void sigalrm_handler(int sig){
	(void)sig; // Evita advertencias de compilación
	pmon_flag = 1;
}

void sigchld_handler(int sig){
	(void)sig;
	pid_t pid;
	int status;
	while((pid=waitpid(-1, &status, WNOHANG)) > 0) {
		for(int i=0; i<MAX_JOBS; i++){
			if(jobs[i].activo && jobs[i].pid==pid) {
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
			break;
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

typedef struct{
	unsigned long long utime;
	unsigned long long stime;
} ProcTime;

int obtenerTiempoProceso(pid_t pid, ProcTime *pt, char  estado){
	char path[256];
	snprintf(path, sizeof(path), "/proc/%d/stat", pid);
	FILE *f= fopen(path, "r");
	if(!f) return 0;
	char comm[256];
	fscanf(f, "%*d %s %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu", comm, &estado, &pt->utime, &pt->stime);
	fclose(f);
	return 1;
}

void ejecutarPmon(int segundos) {
    struct sigaction sa_old, sa_new, sa_alarm;

    // Temporalmente restaurar SIGINT para salir de pmon con ctrl+C sin cerrar la shell
    sa_new.sa_handler = SIG_DFL;
    sigemptyset(&sa_new.sa_mask);
    sa_new.sa_flags = 0;
    sigaction(SIGINT, &sa_new, &sa_old);

    // Configurar alarma con alarm() y SIGALRM
    sa_alarm.sa_handler = sigalrm_handler;
    sigemptyset(&sa_alarm.sa_mask);
    sa_alarm.sa_flags = 0;
    sigaction(SIGALRM, &sa_alarm, NULL);

    ProcTimes prev_times[MAX_JOBS] = {0};
    long ticks_per_sec = sysconf(_SC_CLK_TCK);

    while (1) {
        alarm(segundos);
        pmon_flag = 0;

        printf("\033[H\033[J"); // Limpiar pantalla
        printf("%-8s | %-15s | %-12s | %-10s | %-8s\n", "PID", "COMANDO", "ESTADO", "%CPU(aprox)", "RSS (KB)");
        printf("-------------------------------------------------------------------\n");

        for (int i = 0; i < MAX_JOBS; i++) {
            if (jobs[i].activo) {
                ProcTimes curr;
                char state_char;

                if (!obtenerTiemposProc(jobs[i].pid, &curr, &state_char)) {
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
        if (!pmon_flag) break; // Si la interrupción no fue de la alarma, sale de pmon
    }

    alarm(0);
    sigaction(SIGINT, &sa_old, NULL);
    printf("\nSaliendo de pmon...\n");
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