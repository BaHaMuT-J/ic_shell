#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

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
    printf("bad command\n");
    return 1;
    //return ic_launch(args);
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

int main(int argc, char **argv){
    printf("Starting IC shell\n");

    char buffer[MAX_CMD_BUFFER];
    char **args, **history;
    int status, i=0;

    do {
        printf("icsh $ ");
        
        fgets(buffer, 255, stdin);
        args = split_line(buffer);
        
        //save command for !!
        if(!history){
            history = malloc(MAX_CMD_BUFFER * sizeof(char*));
            for(i=0;args[i]!=NULL;i++){
                history[i] = strdup(args[i]);
            }
            history[i] = NULL;
        }
        else if(args[0] != NULL && history[0] != NULL && strcmp(args[0], builtin_str[1]) == 0 && strcmp(history[0], builtin_str[1]) != 0){
            free(args);
            args = malloc(MAX_CMD_BUFFER * sizeof(char*));
            for(i=0;history[i]!=NULL;i++){
                args[i] = strdup(history[i]);
            }
            args[i] = NULL;
            for(i=0;args[i]!=NULL;i++){
                printf("%s ", args[i]);
            }
            printf("\n");
        }
        else{
            free(history);
            history = malloc(MAX_CMD_BUFFER * sizeof(char*));
            for(i=0;args[i]!=NULL;i++){
                history[i] = strdup(args[i]);
            }
            history[i] = NULL;
        }
        
        status = ic_execute(args);

        free(args);
    } while (status);

    free(history);
    return ex_code;
}