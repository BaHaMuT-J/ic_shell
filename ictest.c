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
int (*builtin_func[]) (char **) = {&ic_echo, &ic_repeat, &ic_exit};

//echo
int ic_echo(char **args){
    for(int i=0;args[i]!=NULL;i++){
        printf("%s", args[i]);
    }
    return 1;
}

//!!
int ic_repeat(char **args){
    return 1;
}

//exit
int ic_exit(char **args){
    if(args[1] == NULL){
        perror("exit: invalid argument");
        exit(EXIT_FAILURE);
    }
    int status = args[1];
    /*
    if(status < 0 || status > 255){

    }
    */
    print("goodbye\n");
    exit(status);
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
    printf("bad command\n");
    return 1;
    //return ic_launch(args);
}

char *read_line(void)
{
    int bufsize = MAX_CMD_BUFFER;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if(!buffer){
        perror("read_line: allocation error");
        exit(EXIT_FAILURE);
    }

    while(1){
        //read a character
        c=getchar();

        if(c == EOF){
            exit(EXIT_SUCCESS);
        } 
        else if(c == '\n'){
            buffer[position] = '\0';
            return buffer;
        } 
        else{
            buffer[position] = c;
        }
        position++;

        //if line exceed the buffer size, reallocate
        if(position >= bufsize){
            bufsize += MAX_CMD_BUFFER;
            buffer = realloc(buffer, bufsize);
            if(!buffer){
                perror("read_line: reallocation error");
                exit(EXIT_FAILURE);
            }
        }
    }
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

    char *line;
    char **args;
    int status;

    do {
        printf("icsh $ ");
        line = lsh_read_line();
        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
    } while (status);

    return 0;
}