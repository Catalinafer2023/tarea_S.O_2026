#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

void dirprint(){
	char cwd[1024];
	printf("\n DIR%s:~$", getcwd(cwd, sizeof(cwd))); 
}

int main(void){
	while(){

	}
	return 0;
}