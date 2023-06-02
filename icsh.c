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
#include <termios.h>
#include <errno.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

/* References
https://github.com/brenns10/lsh/blob/master/src/main.c
https://stackoverflow.com/questions/50280498/how-to-only-kill-the-child-process-in-the-foreground
https://www.gnu.org/s/libc/manual/html_node/Implementing-a-Shell.html
*/

#define MAX_CMD_BUFFER 255

/***********************************************define shell variable************************************************/

pid_t shell_pgid; //main process pgid
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;
int job_number=0;

/************************************************define job variable************************************************/
typedef struct process
{
  struct process *next;       /* next process in pipeline */
  char **argv;                /* command of the process */
  pid_t pid;                  /* process ID */
  int completed;              /* true if process has completed */
  int stopped;                /* true if process has stopped */
  int status;                 /* reported status value */
} process;

/* A job is a pipeline of processes.  */
typedef struct job
{
  struct job *next;           /* next active job */
  char **command;             /* command line, store main command */
  process *first_process;     /* list of processes in this job */
  pid_t pgid;                 /* process group ID */
  int notified;               /* true if user told about stopped job */
  struct termios tmodes;      /* saved terminal modes */
  int stdin, stdout, stderr;  /* standard i/o channels */
} job;

/* The active jobs are linked into a list.  This is its head.   */
job *first_job = NULL;

/************************************************define job function************************************************/

/* Find the active job with the indicated pgid */
job *find_job (pid_t pgid)
{
  job *j;

  for (j = first_job; j; j = j->next)
    if (j->pgid == pgid)
      return j;
  return NULL;
}

/* Return true if all processes in the job have stopped or completed */
int job_is_stopped (job *j)
{
  process *p;

  for (p = j->first_process; p; p = p->next)
    if (!p->completed && !p->stopped)
      return 0;
  return 1;
}

/* Return true if all processes in the job have completed */
int job_is_completed (job *j)
{
  process *p;

  for (p = j->first_process; p; p = p->next)
    if (!p->completed)
      return 0;
  return 1;
}

/* Store the status of the process pid that was returned by waitpid.
   Return 0 if all went well, nonzero otherwise.  */

int mark_process_status (pid_t pid, int status)
{
  job *j;
  process *p;

  if (pid > 0)
    {
      /* Update the record for the process.  */
      for (j = first_job; j; j = j->next)
        for (p = j->first_process; p; p = p->next)
          if (p->pid == pid)
            {
              p->status = status;
              if (WIFSTOPPED (status))
                p->stopped = 1;
              else
                {
                  p->completed = 1;
                  if (WIFSIGNALED (status))
                    fprintf (stderr, "%d: Terminated by signal %d.\n",
                             (int) pid, WTERMSIG (p->status));
                }
              return 0;
             }
      fprintf (stderr, "No child process %d.\n", pid);
      return -1;
    }
  else if (pid == 0 || errno == ECHILD)
    /* No processes ready to report.  */
    return -1;
  else {
    /* Other weird errors.  */
    perror ("waitpid");
    return -1;
  }
}

/* Check for processes that have status information available,
   blocking until all processes in the given job have reported.  */

void wait_for_job (job *j)
{
  int status;
  pid_t pid;

  do
    pid = waitpid (WAIT_ANY, &status, WUNTRACED);
  while (!mark_process_status (pid, status)
         && !job_is_stopped (j)
         && !job_is_completed (j));
}

/* Put job j in the foreground.  If cont is nonzero,
   restore the saved terminal modes and send the process group a
   SIGCONT signal to wake it up before we block.  */

void put_job_in_foreground (job *j, int cont)
{
  /* Put the job into the foreground.  */
  tcsetpgrp (shell_terminal, j->pgid);

  /* Send the job a continue signal, if necessary.  */
  if (cont)
    {
      tcsetattr (shell_terminal, TCSADRAIN, &j->tmodes);
      if (kill (- j->pgid, SIGCONT) < 0)
        perror ("kill (SIGCONT)");
    }

  /* Wait for it to report.  */
  wait_for_job (j);

  /* Put the shell back in the foreground.  */
  tcsetpgrp (shell_terminal, shell_pgid);

  /* Restore the shell’s terminal modes.  */
  tcgetattr (shell_terminal, &j->tmodes);
  tcsetattr (shell_terminal, TCSADRAIN, &shell_tmodes);
}

/* Put a job in the background.  If the cont argument is true, send
   the process group a SIGCONT signal to wake it up.  */

void put_job_in_background (job *j, int cont)
{
  /* Send the job a continue signal, if necessary.  */
  if (cont)
    if (kill (-j->pgid, SIGCONT) < 0)
      perror ("kill (SIGCONT)");
}

void format_job_info (job *j, const char *status)
{
  fprintf (stderr, "%ld (%s): ", (long)j->pgid, status);
  for(int i=0;j->command[i] != NULL;i++) printf("%s ", j->command[i]);
  printf("\n");
}

/* Launch process runned by job launch*/
void launch_process (process *p, pid_t pgid,
                int infile, int outfile, int errfile,
                int foreground)
{
  pid_t pid;

  if (shell_is_interactive)
    {
      /* Put the process into the process group and give the process group
         the terminal, if appropriate.
         This has to be done both by the shell and in the individual
         child processes because of potential race conditions.  */
      pid = getpid ();
      if (pgid == 0) pgid = pid;
      setpgid (pid, pgid);
      if (foreground)
        tcsetpgrp (shell_terminal, pgid);

      /* Set the handling for job control signals back to the default.  */
      signal (SIGINT, SIG_DFL);
      signal (SIGQUIT, SIG_DFL);
      signal (SIGTSTP, SIG_DFL);
      signal (SIGTTIN, SIG_DFL);
      signal (SIGTTOU, SIG_DFL);
      signal (SIGCHLD, SIG_DFL);
    }

  /* Set the standard input/output channels of the new process.  */
  if (infile != STDIN_FILENO)
    {
      dup2 (infile, STDIN_FILENO);
      close (infile);
    }
  if (outfile != STDOUT_FILENO)
    {
      dup2 (outfile, STDOUT_FILENO);
      close (outfile);
    }
  if (errfile != STDERR_FILENO)
    {
      dup2 (errfile, STDERR_FILENO);
      close (errfile);
    }

  /* Exec the new process.  Make sure we exit.  */
  /*
  execvp (p->argv[0], p->argv);
  perror ("execvp");
  */
  exit (1);
}

void launch_job (job *j, int foreground)
{
  process *p;
  pid_t pid;
  int mypipe[2], infile, outfile;

  infile = j->stdin;
  for (p = j->first_process; p; p = p->next)
    {
      /* Set up pipes, if necessary.  */
      if (p->next)
        {
          if (pipe (mypipe) < 0)
            {
              perror ("pipe");
              exit (1);
            }
          outfile = mypipe[1];
        }
      else
        outfile = j->stdout;

      /* Fork the child processes.  */
      pid = fork ();
      if (pid == 0)
        /* This is the child process.  */
        launch_process (p, j->pgid, infile,
                        outfile, j->stderr, foreground);
      else if (pid < 0)
        {
          /* The fork failed.  */
          perror ("fork");
          exit (1);
        }
      else
        {
          /* This is the parent process.  */
          p->pid = pid;
          if (shell_is_interactive)
            {
              if (!j->pgid)
                j->pgid = pid;
              setpgid (pid, j->pgid);
            }
        }

      /* Clean up after pipes.  */
      if (infile != j->stdin)
        close (infile);
      if (outfile != j->stdout)
        close (outfile);
      infile = mypipe[0];
    }

  format_job_info (j, "launched");

  if (!shell_is_interactive)
    wait_for_job (j);
  else if (foreground)
    put_job_in_foreground (j, 0);
  else
    put_job_in_background (j, 0);
}

/* Check for processes that have status information available,
   without blocking.  */

void update_status (void)
{
  int status;
  pid_t pid;

  do
    pid = waitpid (WAIT_ANY, &status, WUNTRACED|WNOHANG);
  while (!mark_process_status (pid, status));
}

/* Free completed job*/
void free_job(job *j){
    job *temp;
    if(!first_job) return;
    if(j->pgid == first_job->pgid){
        temp = first_job;
        first_job = temp->next;
        free(temp);
        return;
    }
    for(temp = first_job;temp->next;temp = temp->next){
        if(temp->next->pgid == j->pgid){
            job *temp2;
            temp2 = temp->next;
            temp->next = temp->next->next;
            free(temp2->first_process->argv);
            free(temp2->first_process);
            free(temp2);
            return;
        }
    }
}

/* Notify the user about stopped or terminated jobs.
   Delete terminated jobs from the active job list.  */

void do_job_notification (void)
{
  job *j, *jlast, *jnext;

  /* Update status information for child processes.  */
  update_status ();

  jlast = NULL;
  for (j = first_job; j; j = jnext)
    {
      jnext = j->next;

      /* If all processes have completed, tell the user the job has
         completed and delete it from the list of active jobs.  */
      if (job_is_completed (j)) {
        format_job_info (j, "completed");
        if (jlast)
          jlast->next = jnext;
        else
          first_job = jnext;
        free_job (j);
      }

      /* Notify the user about stopped jobs,
         marking them so that we won’t do this more than once.  */
      else if (job_is_stopped (j) && !j->notified) {
        format_job_info (j, "stopped");
        j->notified = 1;
        jlast = j;
      }

      /* Don’t say anything about jobs that are still running.  */
      else
        jlast = j;
    }
}

/* Mark a stopped job J as being running again.  */
void mark_job_as_running (job *j)
{
  process *p;

  for (p = j->first_process; p; p = p->next)
    p->stopped = 0;
  j->notified = 0;
}

void
continue_job (job *j, int foreground)
{
  mark_job_as_running (j);
  if (foreground)
    put_job_in_foreground (j, 1);
  else
    put_job_in_background (j, 1);
}

/***********************************************define builtin function***********************************************/

char *builtin_str[] = {"echo","!!","exit"/*,"jobs","fg","bg"*/};
int num_builtins() {return sizeof(builtin_str) / sizeof(char *);}
int ic_echo(char **args);
int ic_repeat(char **args);
int ic_exit(char **args);
int ic_execute(char **args);
int ic_job(char **args);
int ic_fg(char **args);
int ic_bg(char **args);
int (*builtin_func[]) (char **) = {&ic_echo, &ic_repeat, &ic_exit/*, &ic_job, &ic_fg, &ic_bg*/};
int ic_execute_external(char **args);
char **copy(char** args);
int ex_code=0;

/**************************************************builtin function***************************************************/

//echo
int ic_echo(char **args){
    pid_t pid;
    int status, i=0;
    char **io;

    pid = fork();

    if(pid==0){
        //Child Process
        signal(SIGINT, SIG_DFL);

        //for check "<", ">", and "$?"
        io = malloc(sizeof(char*) * 3);
        io[0] = "<";
        io[1] = ">";
        io[2] = "$?";

        //if there is i/o redirection, run external version
        for(i=0;args[i]!=NULL;i++){
            if(strcmp(args[i], io[0]) == 0 || strcmp(args[i], io[1]) == 0){
                free(io);
                return ic_execute_external(args);
            }
        }

        //if not, run builtin version
        for(i=1;args[i]!=NULL;i++){
            //echo $?, printf ex_code which was the exit code of the last command
            if(strcmp(args[1], io[2]) == 0){
                printf("%d\n", ex_code);
                break;
            }
            printf("%s ", args[i]);
        }
        if(i > 1){
            printf("\n");
        }
        free(io);
        exit(EXIT_SUCCESS);
    }
    else if (pid < 0) {
        // Error forking
        perror("execute_external: error forking");
    } 
    else {
        // Parent process
        waitpid(pid, &status, WUNTRACED);
        if(WIFEXITED(status)){
          ex_code = WEXITSTATUS(status);
        }
        else if(WIFSIGNALED(status)){
            ex_code = WTERMSIG(status);
        }
    }
    
    return 1;
}

//!!, if this function run, it means that there is no command other than !! before the present !!
int ic_repeat(char **args){
    return 1;
}

//exit
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

/************************************************job builtin function*************************************************/

//jobs
int ic_job(char **args){
    job* j = first_job;
    int i=0, m=0;
    char **status=malloc(sizeof(char*) * 2);
    status[0] = "Running";
    status[1] = "Stopped";
    for(i=0;j;j = j->next){
        printf("[%d] %s\t", i+1, status[j->notified]);
        for(m=0;j->command[m] != NULL;m++){
            printf("%s ", j->command[m]);
        }
        printf("\n");
    }
    free(status);
    return 1;
}

//fg, incomplete
int ic_fg(char **args){
    //check whether argument of fg start with "%"
    char *t="%";
    int m=*t, n=args[1][0];
    if(m-n != 0){
        printf("bad command\n");
        return 1;
    }
    job *j;
    int i=0, index=args[1][1];
    for(i=1;i<index;i++) j = j->next;
    put_job_in_foreground(j, 1);
    return 1;
}

//bg, incomplete
int ic_bg(char **args){
    //check whether argument of fg start with "%"
    char *t="%";
    int m=*t, n=args[1][0];
    if(m-n != 0){
        printf("bad command\n");
        return 1;
    }
    job *j;
    int i=0, index=args[1][1];
    for(i=1;i<index;i++) j = j->next;
    put_job_in_background(j, 1);
    return 1;
}

//receive args from mainloop and assign it to jobs to be executed, incomplete
int assign_job(char **args){
    int i=0;
    job *j;
    j = first_job;
    for(i=0;i<job_number;i++){
        j = j->next;
    }
    j = malloc(sizeof(job));
    j->first_process = malloc(sizeof(process));
    j->first_process->argv = copy(args);
    job_number++;
    for(i=0;j->first_process->argv[i] != NULL;i++){
        printf("%s ", j->first_process->argv[i]);
    }
    printf("\n");
    //launch_job(j, 1);
    return 1;
}

//function for i/o redirection in ic_execute_external
char** io_redirect(char** args){
    char **new_command, **io;
    int in_out, check_io=0, i=0, j=0;

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
        for(;args[j+1]!=NULL;j++){
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

    free(io);

    //if there is i/o redirection, return new_command
    if(check_io){
            return new_command;
        }
    //if not, return args
    else{
        return args;
    }
}

//execute external command
int ic_execute_external(char **args){
    pid_t pid;
    int status;
    char **new_command;

    pid = fork();
    if (pid == 0) {
        // Child process
        signal(SIGINT, SIG_DFL);

        //get command and check i/o redirection
        new_command = io_redirect(args);

        //execute external command
        if (execvp(new_command[0], new_command) == -1) {
            //if command cannot be found or has invalid arguments
            printf("bad command\n");
            status = 1;
        }

        free(new_command);
        if(status == 1){
            exit(EXIT_FAILURE);
        }
        else{
            exit(EXIT_SUCCESS);
        }
    } 
    else if (pid < 0) {
        // Error forking
        perror("execute_external: error forking");
    } 
    else {
        // Parent process
        waitpid(pid, &status, WUNTRACED);
        if(WIFEXITED(status)){
          ex_code = WEXITSTATUS(status);
        }
        else if(WIFSIGNALED(status)){
            ex_code = WTERMSIG(status);
        }
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
    //init shell
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty (shell_terminal);
    if (shell_is_interactive){
        shell_pgid = getpid ();
        if (setpgid (shell_pgid, shell_pgid) < 0){
            perror ("Couldn't put the shell in its own process group");
            exit (1);
        }

        /* Grab control of the terminal.  */
        tcsetpgrp (shell_terminal, shell_pgid);

        /* Save default terminal attributes for shell.  */
        tcgetattr (shell_terminal, &shell_tmodes);
    }

    //ignore signal sent to parent
    signal (SIGINT, SIG_IGN);
    signal (SIGQUIT, SIG_IGN);
    signal (SIGTSTP, SIG_IGN);
    signal (SIGTTIN, SIG_IGN);
    signal (SIGTTOU, SIG_IGN);
    signal (SIGCHLD, SIG_IGN);
    
    /*
    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if(sigaction(SIGINT, &sa, NULL) == -1){
        printf("Couldn't catch SIGINT - Interrupt Signal\n");
    }
    if(sigaction(SIGTSTP, &sa, NULL) == -1){
        printf("Couldn't catch SIGTSTP - Suspension Signal\n");
    }
    */
        
    //file
    FILE* file_read=NULL;

    //check number of argv
	if(argc<2){
        printf("Starting IC shell\n");
    }
    else{
	    //check whether argv[1] is a file
	    struct stat statbuff;
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
    int status=1, i=0, time=0;

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
        if(time == 0){
            history = copy(args);
            time++;
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
            
            //update ex_code to 0 
            //if the exit code of the last command is 1, then user runs "!!" 2 times in a row, 
            //the first time "echo $?" will prints 1, while prints 0 in the second time since it prints the exit code of the first "!!"
            ex_code = 0;
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