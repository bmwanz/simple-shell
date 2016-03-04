#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define MAX_TOKEN_LENGTH 50
#define MAX_TOKEN_COUNT 100
#define MAX_LINE_LENGTH 512

// Simple implementation for Shell command
// Assume all arguments are seperated by space
// Erros are ignored when executing fgets(), fork(), and waitpid(). 

/**
 * Sample session
 *  shell: echo hello
 *   hello
 *   shell: ls
 *   Makefile  simple  simple_shell.c
 *   shell: exit
**/

int zCount = 0;
int *zPtr = &zCount;

// Contains function that returns the first index of the found item or -1 if not present.
int contains(char** args, char* string) {
	int count = 0;
	while (args[count] != NULL) {
		if (strcmp(args[count], string) == 0)
			return count;
		else
			count++;
	}
	return -1;
}

void runcommand(char* command, char** args) {
	int foundin = -1;
	int foundout = -1;
	int foundpipe = -1; 
	int foundpipe2 = -1;
	int fd;
	int count = 0;
	int saved_stdin = dup(0);
	int saved_stdout = dup(1);
	
	int pipefd1[2];  //pipe file descriptors
	int pipefd2[2];
	char* commands[256][256];  //store commands for piping
	//char* commands1[256];
	//char* commands2[256];
	int command_count;

	*zPtr = 0;

	// If the args contains metacharacters, get index
	foundin = contains(args, "<");
	foundout = contains(args, ">");
	foundpipe = contains (args, "|");

	//if there is redirect in
	//change stdin to input file
	if(foundin != -1)
	  {
	    fd = open(args[foundin+1], O_RDONLY);
	    dup2(fd,0);
	    close(fd);  //close to reuse fd variable
	  }

	//if there is redirect out
	//change stdout to output file
	if(foundout != -1)
	  {
	    fd = open(args[foundout+1], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
	    dup2(fd, 1);
	    close(fd);
	  }

	//if there is a pipe
	//count # of piped commands
	if(foundpipe != -1){
	  
	  
	  commands[0][0] = args[0];                  //store first command
	  int p_count = 0; 
	  int temp_p = 0;
	  int row = 0;
	  int column = 1;
	  command_count = 1;                  //already starts with 1 command
	  
	  while (args[p_count] != NULL){

	    
	    if(strncmp(args[p_count], "-",1) == 0){      //if flag found after command, add it to commands array
	      //printf("found a flag\n");
	      commands[row][column] = args[p_count];
	      column++;
	    }
	    
	    if (strcmp(args[p_count], "|") == 0){         //if pipe is found
	      if(p_count != foundpipe)
		foundpipe2 = p_count;                     //we found the second pipe
	      row++;
	      column = 0;
	      commands[row][column] = args[p_count+1];   //what comes after the pipe is another command
	      column++;
	      temp_p = p_count;
	      while(args[temp_p] != NULL){                //removes the pipe and moves everything
		args[temp_p] = args[temp_p+1];
		temp_p++;
	      }
	      //printf("here's the command without a pipe: %s %s %s\n", args[0], args[1], args[2]);
	      command_count++;                            //increment command counter
	    }

	    
	    p_count++;
	  }
	}

	if ((foundin != -1 || foundout != -1) && foundpipe == -1) 
	  {
	    // Remove <> and redirection names, left with original command and flags
	    for(count = 0; args[count] != NULL; count++)
		{
                        if (count >= foundin && foundin != -1) 
			  {
                                args[count] = NULL;
			  }
			else if(count >= foundout && foundout != -1)
			  {
			    args[count] = NULL;
			  }
		}
		count = 0; 
	    }
	//if(foundpipe2 != -1)
	  //printf("found another pipe\n");
	
	pipe(pipefd1);                          //initialize pipe
	pipe(pipefd2);
	pid_t pid = fork();
	//printf("commands if pipe found: %s %s %s\n", commands[0][0], commands[1], commands[2]);
	if(pid) { // parent
	  if (foundpipe != -1)
	    {

	      
	      pid_t pid2 = fork();  //fork for next command
	      if(pid2){ //parent 2
		
		if(foundpipe2 != -1) //if a second pipe exists
		  {
		    pid_t pid3 = fork();

		    if(pid3){ //parent3
		      close(pipefd1[0]);
		      close(pipefd1[1]);
		      close(pipefd2[0]);
		      close(pipefd2[1]);
		      waitpid(pid3, NULL, 0);
		      dup2(saved_stdin, 0);
		      dup2(saved_stdout, 1);
		    }
		    
		    else{ //child3
		      close(pipefd2[1]); //close write end of pipe
		      close(pipefd1[0]);
		      close(pipefd1[1]);
		      dup2(pipefd2[0], 0); //make read of pipe 2 stdin
		      dup2(saved_stdout, 1);//restore stdout
		      //printf("checking stdout on pipe 2!!\n");
		      
		      count = 0;
		      while(commands[2][count] != NULL)
			{
			  args[count] = commands[2][count];
			  count++;
			}
		      args[count] = NULL;
		      execvp(commands[2][0], args); //execute 3rd command
		    }
		  }
		
		close(pipefd1[0]);
		close(pipefd1[1]);
		close(pipefd2[0]);
		close(pipefd2[1]);
		waitpid(pid2, NULL, 0);
		dup2(saved_stdin, 0);
		dup2(saved_stdout, 1);

	      }
	      else{ //child 2

		close(pipefd1[1]); //close write end of pipe
		close(pipefd2[0]);
		dup2(pipefd1[0], 0); //replace stdin with read end of pipe
		
		if(foundpipe2 == -1){ //if there's no second pipe
		  close(pipefd2[1]);
		  dup2(saved_stdout, 1); //restore stdout
		  //printf("checking stdout when there's no 2nd pipe\n");
		}
		else
		  dup2(pipefd2[1],1); //replace stdout with write of pipe2
		count=0; 
		while(commands[1][count] != NULL)
		  {
		    args[count] = commands[1][count];
		    count++;
		  }
		args[count] = NULL;
		execvp(commands[1][0], args); //execute next command
	      }
	      close(pipefd1[0]);
	      close(pipefd1[1]);
	      close(pipefd2[0]);
	      close(pipefd2[1]);
	      waitpid(pid, NULL, 0);
	      dup2(saved_stdin, 0);
	      dup2(saved_stdout, 1);
	    }

	  else
	    {
	      waitpid(pid, NULL, 0);
	      dup2(saved_stdin, 0);   //restore standard in/out
	      dup2(saved_stdout, 1);
	    }
	} 
	else { // child
	  if (foundpipe != -1)
	    {
	      close(pipefd1[0]);    //close read end of pipe
	      close(pipefd2[0]);
	      close(pipefd2[1]);
	      dup2(pipefd1[1], 1);  //replace stdout with write end of pipe	      
	      //close(pipefd1[1]);

	      count=0; 
	      while(commands[0][count] != NULL)
		{
		  args[count] = commands[0][count];
		  //printf("args[count] = %s\n",commands[0][count]);
		  count++;
		}
	      args[count] = NULL;
	      count = 0;
	      while(count < 5){
		//printf("args[count] = %s\n", args[count]);
		count++;
	      }
	      execvp(commands[0][0], args);
	    }

	  else{
	    execvp(command, args); 
	  }

	}
	int rnCount, cnCount = 0;
	while(commands[rnCount][cnCount] != NULL){
	  while(commands[rnCount][cnCount] != NULL){
	    commands[rnCount][cnCount] = NULL;
	    cnCount++;
	  }
	  rnCount++;
	  cnCount = 0;
	}
	
	int anCount = 0;
	while (args[anCount] != NULL){
	  args[anCount] = NULL;
	  anCount++;
	}
	
	command = NULL;
}

void cnt(int sig) {
	*zPtr = *zPtr + 1;
	//printf("\n\tINTERRUPT DETECTED #%d\n", *zPtr);
	if (*zPtr == 2) {
		printf("\n");
		exit(0);
	}
}

int main(){
    char line[MAX_LINE_LENGTH];
    signal(SIGTSTP, cnt); 

    printf("shell: "); 
    while(fgets(line, MAX_LINE_LENGTH, stdin)) {
    	// Build the command and arguments, using execv conventions.
    	line[strlen(line)-1] = '\0'; // get rid of the new line
    	char* command = NULL;
    	char* arguments[MAX_TOKEN_COUNT];
    	int argument_count = 0;

    	char* token = strtok(line, " ");
    	while(token) {
      		if(!command) command = token;
      		arguments[argument_count] = token;
	      	argument_count++;
      		token = strtok(NULL, " ");
    	}
    	arguments[argument_count] = NULL;
	if(argument_count>0){
		if (strcmp(arguments[0], "exit") == 0)
            		exit(0);
    		runcommand(command, arguments);
	}

        printf("shell: "); 
    }
    return 0;
}
