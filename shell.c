/*
    Implements a simple shell for UNIX.

    Commands built-in are cd (change directory) and exit. Other programs to run
    are executed as child processes.

    The program assumes no input longer than 70 characters and 5 arguments along with the given command.

    Authors:
    Christoffer Gunning, cgunning@kth.se
    Daniel Forslund, dforsl@kth.se

    Last changes made 2013-05-02
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>

#define OK       0
#define NO_INPUT 1
#define TOO_LONG 2
#define DELIMITERS '\s'
#define EXIT_CMD "exit"

/*
    Prompts the users current location
*/
void prompt() {
    char my_cwd[1024];
    getcwd(my_cwd, 1024);

    fputs(my_cwd, stdout);
    fputs("> ", stdout);
    fflush(stdout);
}

/* Blocks ctrl-c */
void handle_signal(int signo) {
    printf("\n");

    prompt();
}

/*
    Reads input from the command line
*/
static int getLine (char *buffer, size_t size) {
    int ch, extra;

    /* Wait for input */
    fgets (buffer, size, stdin);

     /* Verify that the input is not empty */
    if(buffer[0] == '\n') {
        return NO_INPUT;
    }

    /* 
        If it was too long, there'll be no newline. In that case, we flush
        to end of line so that excess doesn't affect the next call. 
    */
    if (buffer[strlen(buffer)-1] != '\n') {
        extra = 0;
        while (((ch = getchar()) != '\n') && (ch != EOF))
            extra = 1;
        return (extra == 1) ? TOO_LONG : OK;
    }

    // Otherwise remove newline and give string back to caller.
    buffer[strlen(buffer)-1] = '\0';
    return OK;
}

/*
    Tokenizes a string by defined delimiters
*/
void tokenize(char* str, char* tokens[]) {
    int i;

    tokens[0] = strtok(str, " ");
    for(i = 1; i < 6; i++){
        tokens[i] = strtok(NULL, " ");
    }

}

/*
    Find out whether a command is to be run as a background process or not
*/
bool is_background_process(char *tokens[]) {
    int i;
    for(i = 1; i < 6 && tokens[i] != NULL; i++)
        if(strcmp(tokens[i], "&") == 0)
            return true;

    return false;
}

/*
    Removes any terminated background child processes by running waitpid for each background
    process not already removed.
*/
void remove_terminated_processes(int* num_children) {
    int i, status, num_removed;
    for(i = 0; i < *num_children; i++) {
        pid_t child_pid = waitpid(-1, &status, WNOHANG);
        if(child_pid >  0) {
            printf("Background process %d has terminated with the status %d\n", child_pid, status);
            num_removed++;
        }
    }

    *num_children -= num_removed;
}

/*
    Change the current working directory
*/
void change_dir(char *dir) {
    if (chdir(dir) != 0)
        printf("An error occured.\n");
}

/*
    Main function of the program. Takes user input and spawnes processes for the given commands.
*/
int main(void) {
    /* 
        res = return values of function calls
        status = status variable for child processes
        num_children = current number of background child processes
    */
    int res, status, num_children;

    /* Buffer for input */
    char buffer[70];

    /* Used to keep a reference to child processes old signal handlers */
    struct sigaction old_action;

    /* Storage of start and end times to measure foreground processes' execution time */
    struct timeval start, finish;

    /* Block Ctrl-C and declare a new signal handler */
    signal(SIGINT, handle_signal);  

    /* Run until the user gives the exit command */
    while(1) {
        /* Prompt */
        prompt();

        /* Read input */
        res = getLine(buffer, sizeof(buffer));
        
        /* Validate the existance of input */
        if(res == NO_INPUT)
            continue;

        /* Validate the input not being to long */
        if(res == TOO_LONG) {
            printf("Input too long.\n");
            continue;
        }

        /* Check if the exit command was given */
        if(strcmp(buffer, EXIT_CMD) == 0) {
            exit(0);
        }

        /* Tokenize the input */
        char* args[6];
        tokenize(buffer, args);

        /*
            Run the given command (unless exited...). Handle the cd-command separately
        */
        if(strcmp(args[0], "cd") == 0) {
            /* Change working dir */
            change_dir(args[1]);
        } else {
            /* Create a child process */
            pid_t pid = fork(); 
            if(pid == -1)
                perror("Failed to fork()");

            /* Run in background? */
            bool as_background = is_background_process(args);

            /* Get the time of creation */
            res = gettimeofday(&start, NULL);
            if(res == -1)
                printf("Failed to execute gettimeofday()");

            /* Handle the forked child process */
            if(pid == 0) { 
                /* Allow the child process to be interrupted by SIGINT */
                res = sigaction(SIGINT, &old_action, NULL);
                if(res == -1) {
                    perror("Child process failed to change sigaction");
                    exit(1);
                }

                /* Change process group if it is to run in the background */
                if(as_background)
                    setpgid(0, 0);

                /* Execute current filter, print error message and exit on error */        
                execvp(args[0], args);

                /* If we reached this point, execvp has failed */
                perror("Failed to fork()");
                _exit(EXIT_FAILURE);
            } else if(pid > 0) {
                if(as_background) {
                    num_children++;
                    printf("Spawned background process with PID %d\n", pid);
                } 
                else
                    printf("Spawned foreground process with PID %d\n", pid);
            }

            /* Don't wait if it's to run in the background */
            if(!as_background) {
                /* Wait for the forked forground child process */
                int childpid = wait(&status); 
                if( -1 == childpid ) { 
                    perror("wait() failed unexpectedly"); 
                    exit(1); 
                }

                /* If the child is a foreground process; wait for it to finish and measure its execution time */
                if(!as_background) {
                    waitpid(pid, &status, 0);
                    printf("Forground process %d terminated\n", pid);
                    gettimeofday(&finish, NULL);
                    double exec_time = (finish.tv_usec - start.tv_usec)/1000.00;
                    printf("Time of execution: %.3f ms\n", exec_time);
                }
            }

            /* Finally poll and remove terminated background processes */
            remove_terminated_processes(&num_children);
        }
    }

    return 0;
}