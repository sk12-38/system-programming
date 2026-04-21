/* Signal handler 만들어 보자 */

/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#define MAXARGS 128

/* My global variables in phase1 */
volatile sig_atomic_t pid_atomic;

/* Function prototypes */
void eval(char *cmdline);
// int parseline(char *buf, char **argv);
int builtin_command(char **argv);
/* My functions in phase1 */
int parseline(char *buf, char **argv);
/* Signal Handlers */
void sigchld_handler(int sig);

int main() 
{
    /* Install Signall Handlers */
    Signal(SIGCHLD, sigchld_handler);

    char cmdline[MAXLINE]; /* Command line */

    while (1) {
	/* Read */
	printf("CSE4100-SP-P2> ");                   
	fgets(cmdline, MAXLINE, stdin); 
	if (feof(stdin)) {
        printf("\n");
	    exit(0);
    }

	/* Evaluate */
	eval(cmdline);
    } 
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    /* My local variables in Phase1 */
    char pathbuf[MAXLINE];  /* Holds path where user wants to move */
    sigset_t mask_all, mask_one, prev_one;
    
    /* Setting mask */
    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);

    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	    return;   /* Ignore empty lines */

    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);   /* Block SIGCHLD */
        if ((pid = Fork()) == 0) {  // Child runs user job
        // Handle execution of common shell commands like ls, echo, cat, etc.
            // If the command does not start with '/', treat it as a basic command
            // and automatically prepend "/bin/" to form the full executable path
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);  /* Unblock SIGCHLD */
            if (argv[0][0] != '/') {
                snprintf(pathbuf, sizeof(pathbuf), "/bin/%s", argv[0]);
                execve(pathbuf, argv, environ); // Execute command from /bin/
            }
            else
                execve(argv[0], argv, environ); // Execute command with full path
            // If execve() fails, print an error message and terminate the child
            // execve() never returns if successful; this block only runs on failure
            fprintf(stderr, "%s: Command not found.\n", argv[0]);
            exit(0);
        }
        /* Parent waits for foreground job to terminate */
        Sigprocmask(SIG_BLOCK, &mask_all, NULL);
        if (!bg){ 
            pid_atomic = 0;
            while (pid_atomic != pid) {   /* Explicitly wait SIGCHLD */
                sigsuspend(&prev_one);
            }
        }
        else { //when there is backgrount process!
            printf("[%d] %s", pid, cmdline);
        }
        Sigprocmask(SIG_SETMASK, &prev_one, NULL);  /* Unblock SIGCHLD */
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit")) /* quit command */
	    exit(0);  
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	    return 1;
    if (!strcmp(argv[0], "cd")) { /* Change current directory */
        char *path;
        /* Default: if there is no explicit path, move to home directory. */
        if (!argv[1] || !strcmp(argv[1], "~")) {
            path = getenv("HOME");
            if (path == NULL) {
                fprintf(stderr, "cd: HOME environment variable not set.\n");
                return 1;
            }
        } 
        /* else, move to the explicit path */
        else
            path = argv[1];
        /* change current directory using chdir function */
        if (chdir(path) != 0) {
            fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
        }
        return 1;
    }
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */
    int len;

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;

    // 수정된 부분: 큰따옴표로 감싸진 인자를 하나로 처리
    while (*buf) {
        while (*buf == ' ') buf++;         // Skip spaces
        if (*buf == '\0') break;

        if (*buf == '"') {                 // 따옴표로 시작하는 인자 처리
            buf++;                         // opening quote 건너뛰기
            argv[argc++] = buf;
            while (*buf && *buf != '"')    // closing quote까지 이동
                buf++;
            if (*buf == '"') {
                *buf = '\0';               // closing quote를 문자열 종료로 대체
                buf++;
            }
        } else {
            argv[argc++] = buf;
            delim = strchr(buf, ' ');
            if (delim) {
                *delim = '\0';
                buf = delim + 1;
            } else {
                break;
            }
        }
    }

    argv[argc] = NULL;

    if (argc == 0)  /* Ignore blank line */
        return 0;

    /* Handle '&' as background signal */
    len = strlen(argv[argc - 1]);
    if (argv[argc - 1][len - 1] == '&') {
        if (len == 1) {
            if (argc == 1)  /* only & */
                printf("syntax error near unexpected token `&'\n");
            /* If the argument is just "&" */
            argc--;
        } else {
            /* If the argument ends with &, like "100&" */
            argv[argc - 1][len - 1] = '\0';  // remove '&'
        }
        argv[argc] = NULL;
        bg = 1;
    } else {
        bg = 0;
    }

    return bg;
}
/* $end parseline */

/* Signal Handlers */
void sigchld_handler(int sig)
{
    int old_errno = errno; // Back-up errno
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        pid_atomic = pid;
    }
    errno = old_errno; // Restore errno
}