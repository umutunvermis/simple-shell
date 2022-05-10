#include <stdio.h>  
#include <string.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/wait.h>

#include <sys/stat.h>
#include <sys/types.h>

#define MAXLINE 100
#define MAXARGS 11

struct command {
  int argc;              // number of args
  char *argv[MAXARGS];   // arguments list
  enum builtin_t {       // is argv[0] a builtin command?
    NONE, BYE, DIR, BG, FG, CD, HISTORY } builtin;
};

enum builtin_t parseBuiltin(struct command *cmd) {
    if (!strcmp(cmd->argv[0], "bye")) { // bye command
        return BYE;
    } else if (!strcmp(cmd->argv[0], "dir")) { // dir command
        return DIR;
    } else if (!strcmp(cmd->argv[0], "bg")) { // bg command
        return BG;
    } else if (!strcmp(cmd->argv[0], "cd")) { // cd command
        return CD;
    } else if (!strcmp(cmd->argv[0], "fg")) { // fg command
        return FG;
    } else if (!strcmp(cmd->argv[0], "history")) { // history command
        return HISTORY;
    } else {
        return NONE;
    }
}

void dir() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("Dir: %s\n", cwd);
}

void error(char *msg) {
  printf("Error: %s", msg);
  exit(0);
}

int parse(const char *commandLine, struct command *cmd) {
    static char array[MAXLINE];           // local copy of command line
    const char delims[10] = " \t\n";      // argument delimiters
    char *line = array;                   // ptr that traverses command line
    char *token;                          // ptr to the end of the current arg
    char *endline;                        // ptr to the end of the commandLine string
    int is_bg;                            // background job?

    if (commandLine == NULL) 
        error("command line is empty!\n");

    (void) strncpy(line, commandLine, MAXLINE);
    endline = line + strlen(line);

    // build argv list
    cmd->argc = 0;

    while (line < endline) {
        // skip delimiters
        line += strspn (line, delims);
        if (line >= endline) break;

        // Find token delimiter
        token = line + strcspn (line, delims);

        // terminate the token
        *token = '\0';

        // Record token
        cmd->argv[cmd->argc++] = line;

        // Check if arugments is full
        if (cmd->argc >= MAXARGS-1) break;

        line = token + 1;
    }

    // Add a NULL pointer at the end of arguments because of execvp
    cmd->argv[cmd->argc] = NULL;

    if (cmd->argc == 0)  // ignore blank line
        return 1;

    cmd->builtin = parseBuiltin(cmd);

    // should job run in background?
    if ((is_bg = (*cmd->argv[cmd->argc-1] == '&')) != 0)
        cmd->argv[--cmd->argc] = NULL;

    return is_bg;
}

void runSystemCommand(struct command *cmd, int bg) {
    pid_t pid;
    
    // FORK 
    if ((pid = fork()) < 0)
        error("fork() error");
    else if (pid == 0) { // Chid process program executes system command here

        // executing system command
        if (execvp(cmd->argv[0], cmd->argv) < 0) {
            printf("%s: Command not found\n", cmd->argv[0]);
            exit(0);
        }
        
    }
    else { // Parent process program waits child to execute and continues from there
       if (bg) 
          printf("Child in background [%d]\n",pid); //test
       else
       {
          // wait child
          wait(NULL);
          
       }
    }
}


int runBuiltinCommand(struct command *cmd, int bg) {
    switch (cmd->builtin) {
        case BYE:
        	exit(0);   // Foreground process, so exits program 
        case HISTORY:
            return 2;
        case DIR:
            dir(); break;
        case BG:
            break;
        case FG:
            break;  
        case CD:
        	if(cmd->argv[1] == NULL){
        		chdir(getenv("HOME"));
        		break;
        	}
        	chdir(cmd->argv[1]);
            break;
        default:
            error("Unknown builtin command");
    return 0;         
    }
}

void runPipedCommand(char *commandLine){                     

	struct command cmd1;
  	struct command cmd2;
  	char delim[2] = "|";
  	char token2[256];
  		
    char * token1 = strtok(commandLine, delim);           // first command
    int bg1 = parse(token1, &cmd1);
    
    // pipe	
    int fd[2];
    if (pipe(fd) == -1) {
        return;
    }
    // fork
    int pid1 = fork();
    if (pid1 < 0) {
        return;
    }
    
    if (pid1 == 0) {
        // Child process 1 (first command)
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        
        // executing system command
        if (execvp(cmd1.argv[0], cmd1.argv) < 0) {
            printf("%s: Command not found\n", cmd1.argv[0]);
            exit(0);
        }
        exit(0);
    }
    // second fork
    int pid2 = fork();
    if (pid2 < 0) {
        return;
    }
    
    if (pid2 == 0) {
        // Child process 2 (second command)
        
        // second command 
		token1 = strtok(NULL, delim);                                              //
		strcpy(token2,token1);                                                     //
		strcpy(token2,&token2[1]);    // white space remove                        // String Operations
		int bg2 = parse(token2, &cmd2);                                            //
        
        dup2(fd[0], STDIN_FILENO);                                                 
        close(fd[0]);
        close(fd[1]);
        
        // executing system command
        if (execvp(cmd2.argv[0], cmd2.argv) < 0) {
            printf("%s: Command not found\n", cmd2.argv[0]);
            exit(0);
        }
        exit(0);
    }
    
    // closing pipe in main process
    close(fd[0]);
    close(fd[1]);
    
    // wait 2 times for 2 child processes
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}


int run(char *commandLine) {
  int bg;
  struct command cmd;
  
  // if there is pipe
  if(strchr(commandLine, '|') != NULL){
  	runPipedCommand(commandLine);  // executes parse inside
  	return 0;
  }
  
  // parse line into command struct
  bg = parse(commandLine, &cmd); 

  // parse error
  if (bg == -1) return 0;
  // empty line - ignore
  if (cmd.argv[0] == NULL) return 0;

  if (cmd.builtin == NONE){
    runSystemCommand(&cmd, bg);
    return 0;
    }
   else 
    return runBuiltinCommand(&cmd, bg);
}



int main(int argc, char **argv) {
  char commandLine[MAXLINE];           // buffer for fgets
  char c;

  
  char history[256][32];               // array for history
  int i = 0;

  while (1) {
	
    printf("%s", "myshell>");          // prompt

    if ((fgets(commandLine, MAXLINE, stdin) == NULL) && ferror(stdin))
      error("fgets error");

    if (feof(stdin)) { 
      printf("\n");
      exit(0);
    }
    
    // stores command in history array
    strcpy(history[i], commandLine);
    
	// Remove trailing newline "\0"
	commandLine[strlen(commandLine)-1] = '\0';
			
	// run the command line
	int flag = run(commandLine);                                
		
	if (flag == 2){                                        // history command
		for(int loop=0; loop<i+1; loop++)
		{
			printf("[%2d] %s",loop+1,history[loop]);
		}    
	} 
    
  	
  	i++;
  }
}
