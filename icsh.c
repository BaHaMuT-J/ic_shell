/* ICCS227: Project 1: icsh
 * Name: Siranut Jongdee
 * StudentID: 6481261
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
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
int ic_execute_external(char **args);

//echo
int ic_echo(char **args){
    int i=0;
    
    //for check "<" and ">"
    char **io;
    io = malloc(sizeof(char*) * 2);
    io[0] = "<";
    io[1] = ">";

    //if there is i/o redirection, run external version
    for(i=0;args[i]!=NULL;i++){
        if(strcmp(args[i], io[0]) == 0 || strcmp(args[i], io[1]) == 0){
            free(io);
            return ic_execute_external(args);
        }
    }

    //if not, run builtin version
    for(i=1;args[i]!=NULL;i++){
        printf("%s ", args[i]);
    }
    if(i > 1){
        printf("\n");
    }
    free(io);
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
    int status, check_io=0, i=0, j=0;
    char **new_command, **io;
    int in_out;

    pid = fork();
    if (pid == 0) {
        // Child process

        //for compare "<" and ">"
        io = malloc(sizeof(char*) * 2);
        io[0] = "<";
        io[1] = ">";

        //check whether there is i/o redirection
        for(i=0;args[i]!=NULL;i++){
            if(strcmp(args[i], io[0]) == 0){
                // input redirection
                in_out = open (args[i+1], O_RDONLY);
                //if file cannot be read
                if(in_out<=0){
                    perror("execute_external: cannot read the file");
                    exit(EXIT_FAILURE);
                }

                //redirect
                dup2(in_out, 0);

                //increase check_io to tell program that there is i/o redirection
                check_io--;
                break;
            }
            else if(strcmp(args[i], io[1]) == 0){
                //output redirection
                in_out = open (args[i+1], O_TRUNC | O_CREAT | O_WRONLY, 0666);
                //if file cannot be read
                if(in_out<=0){
                    perror("execute_external: cannot read the file");
                    exit(EXIT_FAILURE);
                }

                //redirect
                dup2(in_out, 1);

                //increase check_io to tell program that there is i/o redirection
                check_io++;
                break;
            }
        }

        //if there is i/o redirection, copy args to new_command until "<" or ">"
        if(check_io<0){
            //input redirection, copy all but skip "<"
            new_command = malloc(MAX_CMD_BUFFER * sizeof(char*));
            for(j=0;j<i;j++){
                new_command[j] = strdup(args[j]);
            }
            for(j;args[j+1]!=NULL;j++){
                new_command[j] = strdup(args[j+1]);
            }
            new_command[j] = NULL;
        }
        else if(check_io>0){
            //output redirection, copy until ">"
            new_command = malloc(MAX_CMD_BUFFER * sizeof(char*));
            for(j=0;j<i;j++){
                new_command[j] = strdup(args[j]);
            }
            new_command[i] = NULL;
        }
        
        //execute external command
        //if there is i/o redirection, run new_command
        if(check_io){
            if (execvp(new_command[0], new_command) == -1) {
                //if command cannot be found or has invalid arguments
                printf("bad command\n");
            }
        }
        //if not, run args
        else{
            if (execvp(args[0], args) == -1) {
                //if command cannot be found or has invalid arguments
                printf("bad command\n");
            }
        }
        
        //if there is i/o redirection, free new_command
        free(io);
        if(check_io){
            free(new_command);
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

    //check whether it is bultin command
    for (i = 0; i < num_builtins(); i++) {
        if(strcmp(args[0], builtin_str[i]) == 0){
            return (*builtin_func[i])(args);
        }
    }

    //if command is not builtin, run external command
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
        //if there is a file, read lines from file
        if(file_read){
            //read line by line until EOF
            if(fgets(buffer, MAX_CMD_BUFFER, file_read)) {
                args = split_line(buffer);
            }
            else{
                break;
            }
        }
        //main loop of the shell
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
        
        //run command
        status = ic_execute(args);
        
        //free agrs before receive new command in the next run
        free(args);
    } while (status);
    
    free(history);
    if(file_read){
        fclose(file_read);
    }

    return ex_code;
}