#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

void dirprint(){
	char cwd[1024];
	printf("\nDIR:%s:~$", getcwd(cwd, sizeof(cwd))); 
}

int main(){
	char *args;
	char *space = " ";
	while(1){
		dirprint();
		fgets(args,100,stdin);
		args[strcspn(args, "\n")] = 0;
		if(strcmp(args, "exit") == 0){
			break;
		}
	}
	return 0;
}