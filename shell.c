#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/wait.h> /* definierar bland annat WIFEXITED */
#include <unistd.h>

#define OK       0
#define NO_INPUT 1
#define TOO_LONG 2
#define DELIMITERS '\s'

int status;

static int getLine (char *prmpt, char *buff, size_t sz) {
    int ch, extra;

    // Get line with buffer overrun protection.
    if (prmpt != NULL) {
        printf ("%s> ", prmpt);
        fflush (stdout);
    }
    if (fgets (buff, sz, stdin) == NULL)
        return NO_INPUT;

    // If it was too long, there'll be no newline. In that case, we flush
    // to end of line so that excess doesn't affect the next call.
    if (buff[strlen(buff)-1] != '\n') {
        extra = 0;
        while (((ch = getchar()) != '\n') && (ch != EOF))
            extra = 1;
        return (extra == 1) ? TOO_LONG : OK;
    }

    // Otherwise remove newline and give string back to caller.
    buff[strlen(buff)-1] = '\0';
    return OK;
}

void tokenize(char *str, char *tokens[]) {
    int i;
    tokens[0] = strtok(str, " ");
    for(i = 1; i < 5; i++){
        tokens[i] = strtok(NULL, " ");
    }
}

int main (void) {
    int rc;
    char buff[100];

    while(strcmp(buff, "exit")) {
        char my_cwd[1024];
        getcwd(my_cwd, 1024);
        rc = getLine(my_cwd, buff, sizeof(buff));
        if (rc == NO_INPUT) {
            // Extra NL since my system doesn't output that on EOF.
            printf ("\nNo input\n");
            return 1;
        }

        if (rc == TOO_LONG) {
            printf ("Input too long [%s]\n", buff);
            return 1;
        }

        char *tokens[5]; 
        tokenize(buff, tokens);

        if(strcmp(tokens[0], "cd") == 0) {
            printf("Change dir\n");
            int ret = chdir (tokens[1]);
            if (ret ==0) {
                printf("good\n");
            } else {
                printf("bad\n");
            }
        } else {
            pid_t pid = fork(); /* Create new process */

            if (pid == 0) { /* Code for child processes*/
                /* Execute current filter, print error message and exit on error */        

                execvp(tokens[0], tokens);

                /* If this is executed execlp has failed */
                perror("error");
                _exit(EXIT_FAILURE);
            }

            printf("PID: %d\n", pid);

            int childpid = wait( &status ); /* Vänta på ena child-processen */
            if( -1 == childpid )
            { perror( "wait() failed unexpectedly" ); exit( 1 ); }

            if( WIFEXITED( status ) ) /* child-processen har kört klart */
            {
                int child_status = WEXITSTATUS( status );
                if( 0 != child_status ) /* child had problems */
                {
                    fprintf( stderr, "Child (pid %ld) failed with exit code %d\n",
                           (long int) childpid, child_status );
                }
            }
            else
            {
                if( WIFSIGNALED( status ) ) /* child-processen avbröts av signal */
                {
                    int child_signal = WTERMSIG( status );
                    fprintf( stderr, "Child (pid %ld) was terminated by signal no. %d\n",
                           (long int) childpid, child_signal );
                }
            }
        }
    }
    return 0;
}