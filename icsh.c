/* ICCS227: Project 1: icsh
 * Name: Siranut Jongdee
 * StudentID: 6481261
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_CMD_BUFFER 255

char *builtin_str[] = {"echo","!!","exit"};

void line_read(char* line, ssize_t char_read){
    printf("enter line_read: line = %s, char_read = %ld\n", line, char_read);

    char *line_copy = NULL;
    const char *delim = " \n";
    int num_tokens = 0;
    char *token;

    /* allocate space for a copy of the line */
    line_copy = malloc(sizeof(char) * char_read);
    if (line_copy== NULL){
        perror("memory allocation error");
        exit(-1);
    }
    /* copy line to line_copy */
    strcpy(line_copy, line);

    //split line into an array of words
    // calculate the total number of tokens
    token = strtok(line, delim);

    while (token != NULL){
        num_tokens++;
        token = strtok(NULL, delim);
    }
    num_tokens++;

    /* Allocate space to hold the array of strings */
    char** argv = malloc(sizeof(char *) * num_tokens);

    /* Store each token in the argv array */
    token = strtok(line_copy, delim);

    int i=0;
    for (i = 0; token != NULL; i++){
        argv[i] = malloc(sizeof(char) * strlen(token));
        strcpy(argv[i], token);

        token = strtok(NULL, delim);
    }
    argv[i] = NULL;
    for(i = 0; i<num_tokens-1; i++){
        printf("%s\n", argv[i]);
    }
    printf("\n");
    free(argv);
    free(line_copy);
}

int main(){
    char buffer[MAX_CMD_BUFFER];
    printf("Starting IC shell\n");

    //main loop
    while (1) {
        printf("icsh $ ");
        fgets(buffer, 255, stdin);
        //line_read(buffer, char_read);
        printf("you said: %s\n", buffer);
    }
    
   return 0;
}
