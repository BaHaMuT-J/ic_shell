/* ICCS227: Project 1: icsh
 * Name: Siranut Jongdee
 * StudentID: 6481261
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

//from https://github.com/brenns10/lsh/blob/master/src/main.c

#define MAX_CMD_BUFFER 255

//define builtin function
char *builtin_str[] = {"echo","!!","exit"};
int num_builtins() {return sizeof(builtin_str) / sizeof(char *);}
int ic_echo(char **args);
int ic_repeat(char **args);
int ic_exit(char **args);
int ic_execute(char **args);
int (*builtin_func[]) (char **) = {&ic_echo, &ic_repeat, &ic_exit};

//echo
int ic_echo(char **args){
    int i=0;
    for(i=1;args[i]!=NULL;i++){
        printf("%s ", args[i]);
    }
    if(i > 1){
        printf("\n");
    }
    return 1;
}

//!!, if this function run, it means that there is no command other than !! before the present !!
int ic_repeat(char **args){
    return 1;
}

//exit
int ex_code=0;
int ic_exit(char **args){
    if(args[1] == NULL){
        perror("exit: invalid argument");
        exit(EXIT_FAILURE);
    }
    ex_code = atoi(args[1]);
    /*
    if(ex_code < 0 || status > 255){
        printf("exi code less than 0 or more than 255\n");
    }
    */
    printf("goodbye\n");
    return 0;
}

//execute external command
int ic_execute_external(char **args){
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            //if command cannot be found or has invalid arguments
            printf("bad command\n");
        }
        exit(EXIT_FAILURE);
    } 
    else if (pid < 0) {
        // Error forking
        perror("execute_external: error forking");
    } 
    else {
        // Parent process
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

//execute builtin command
int ic_execute(char **args)
{
    int i;

    //if there is no command
    if (args[0] == NULL) {
        return 1;
    }

    for (i = 0; i < num_builtins(); i++) {
        if(strcmp(args[0], builtin_str[i]) == 0){
            return (*builtin_func[i])(args);
        }
    }

    //if command is not builtin
    return ic_execute_external(args);
}

char **split_line(char *line)
{
    int bufsize = MAX_CMD_BUFFER, position = 0;
    const char *delim = " \t\r\n\a";
    char **tokens = malloc(bufsize * sizeof(char*)), *token, **tokens_backup;

    if(!tokens){
        perror("split_line: allocation error");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, delim);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += MAX_CMD_BUFFER;
            tokens_backup = tokens;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if(!tokens){
		        free(tokens_backup);
                perror("split_line: reallocation error");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, delim);
    }

    tokens[position] = NULL;

    return tokens;
}

char **copy(char** source){
    int i=0;
    char **dest = malloc(MAX_CMD_BUFFER * sizeof(char*));
    for(i=0;source[i]!=NULL;i++){
        dest[i] = strdup(source[i]);
    }
    dest[i] = NULL;
    return dest;
}

int main(int argc, char **argv){
    FILE* file_read=NULL;

    //check number of argv
	if(argc<2){
        printf("Starting IC shell\n");
    }
    else{
	    //check whether argv[1] is a file
	    struct stat statbuff;
        char *filetype;
        stat(argv[1], &statbuff);
        if(S_ISREG(statbuff.st_mode)){
        }
        else{
		perror("Cannot find a file");
                exit(EXIT_FAILURE);
        }

        //open the file
	    file_read = fopen(argv[1], "r");
	    //check whether the file can be opened
	    if(!file_read){
		    perror("Cannot open the file");
		    exit(EXIT_FAILURE);
	    }
    }
    
    char buffer[MAX_CMD_BUFFER];
    char **args, **history;
    int status=1, i=0;

    do {
        if(file_read){
            if(fgets(buffer, MAX_CMD_BUFFER, file_read)) {
                args = split_line(buffer);
            }
            else{
                break;
            }
        }
        else{
            printf("icsh $ ");
            fgets(buffer, 255, stdin);
            args = split_line(buffer);
        }
        
        //save command for !!
        //the first command of the shell, copy it to history
        if(!history){
            history = copy(args);
        }
        //when command is !!, copy the last command which was not !! to args
        else if(args[0] != NULL && history[0] != NULL && strcmp(args[0], builtin_str[1]) == 0 && strcmp(history[0], builtin_str[1]) != 0){
            free(args);
            args = copy(history);
            
            //!!, print the command before execute
            for(i=0;args[i]!=NULL;i++){
                printf("%s ", args[i]);
            }
            printf("\n");
        }
        //if it is other command, except empty, copy args to history
        else if(args[0] != NULL){
            free(history);
            history = copy(args);
        }
        
        status = ic_execute(args);
        
        free(args);
    } while (status);
    
    free(history);
    if(file_read){
        fclose(file_read);
    }

    return ex_code;
}