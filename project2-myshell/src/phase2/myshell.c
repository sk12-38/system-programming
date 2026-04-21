/* 왜 "sleep 10 & 입력 후, "ps t"를 입력하면 "CSE4100-SP-P2"가 늦게 출력될까? */

/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#define MAXARGS 128
/* My macro variables in phase2 */
#define MAXCMDS 128

/* My global variables in phase1 */
// volatile sig_atomic_t pid_atomic; not used in phase2

/* Function prototypes */
void eval(char *cmdline);
int builtin_command(char **argv);
/* My functions in phase1 */
/* Signal Handlers */
void sigchld_handler(int sig);
/* My function in phase2 */
int parseline(char *buf, char *cmds[MAXCMDS][MAXARGS], int *bg);
void insert_spaces_around_operators(char *cmdline, char *newbuf);
void strip_quotes(char *s);

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
/* Pipeline: pipefd[0]: read, pipefd[1]: write */
void eval(char *cmdline) 
{
    // char *argv[MAXARGS]; /* not used in phase2 */
    char buf[MAXLINE];      /* Holds modified command line */
    int bg;                 /* Should the job run in bg or fg? */
    pid_t pid;              /* Process id */
    /* My variables in Phase1 */
    char pathbuf[MAXLINE];  /* Holds path where user wants to move */
    sigset_t mask_all, mask_one, prev_one;
    /* My variables in Phase2 */
    char *cmds[MAXCMDS][MAXARGS]; /* Saves command groups */
    int num_cmds;   /* The number of command */

    /* Setting mask */
    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);

    strcpy(buf, cmdline);
    num_cmds = parseline(buf, cmds, &bg);
    /* Ignore empty lines and only '&' command */
    if (cmds[0][0] == NULL) {
        if (bg)
            fprintf(stderr, "syntax error: syntax error near unexpected token `&`\n");
        return;
    }
    /* if the single command is builtin_command, return */
    if (num_cmds == 1 && builtin_command(cmds[0]))
        return;

    /* else, handle execution of common shell commands like ls, echo, cat, etc. */
    int oldfd = -1; /* 이전 명령의 파이프 읽기 끝(fd[0])을 저장해두는 변수 */
    for (int i = 0; i < num_cmds; i++) {
        int pipefd[2];
        if (i < num_cmds - 1)   /* if current command is not the last, we need pipe */
            pipe(pipefd);
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);   /* Block SIGCHLD */
        if ((pid = Fork()) == 0) {  /* Child runs use job */
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);  /* Unblock SIGCHLD */
            if (oldfd != -1) {      /* 부모가 파이프를 연결해줬으므로 -1이 아니게 된다. */
                dup2(oldfd, STDIN_FILENO);  // pipe에서 입력을 받게 함
                close(oldfd);
            }
            if (i < num_cmds - 1) { /* 출력을 다음 명령어에게 전달 */
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            if (cmds[i][0][0] != '/') {
                snprintf(pathbuf, sizeof(pathbuf), "/bin/%s", cmds[i][0]);
                execve(pathbuf, cmds[i], environ);
            } else {
                execve(cmds[i][0], cmds[i], environ);
            }
            /* execve fails */
            if (builtin_command(cmds[i])) {
                exit(0); // builtin 명령어는 파이프 안에서 조용히 무시 ex> cd | ls
            } else {
                fprintf(stderr, "%s: Command not found.\n", cmds[i][0]);
                exit(1); // 진짜 실행 파일이 없을 때는 에러 출력
            }
        }
        /* Parent close and redirect oldfd */
        if (oldfd != -1)
            close(oldfd);
        if (i < num_cmds - 1) {
            close(pipefd[1]);
            oldfd = pipefd[0];
        }
    }

    /* Parent waits for foreground job to terminate */
    Sigprocmask(SIG_BLOCK, &mask_all, NULL);
    if (!bg) {
        int status;
        for (int i = 0; i < num_cmds; i++) {
            waitpid(-1, &status, 0);  // ✅ foreground 자식만 순차적으로 wait
        }
    } else {
        printf("[%d] %s", pid, cmdline);
    }
    Sigprocmask(SIG_SETMASK, &prev_one, NULL); 
}
/* $end eval */

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

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char *cmds[MAXCMDS][MAXARGS], int *bg) {
    char *delim;    /* Points to first space delimiter */
    int cmd_idx = 0, arg_idx = 0;
    static char spaced_buf[MAXLINE];                          // ★ 수정됨: 공백 삽입된 버퍼
    
    insert_spaces_around_operators(buf, spaced_buf);          // ★ 수정됨: 연산자(|<>&) 주변에 공백 삽입
    buf = spaced_buf;                                         // ★ 수정됨: 원래 buf 대신 spaced_buf 사용

    buf[strlen(buf) - 1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' '))   /* Ignore leading spaces */
        buf++;

    // ★ 수정 시작: 따옴표를 처리하는 로직 추가
    while (*buf) {
        while (*buf == ' ') buf++;
        if (*buf == '\0') break;

        if (*buf == '"') {
            buf++;  // opening quote 건너뛰기
            cmds[cmd_idx][arg_idx++] = buf;
            while (*buf && *buf != '"') buf++;
            if (*buf == '"') {
                *buf = '\0';  // 닫는 quote를 문자열 종료로 대체
                buf++;
            }
        } else if (*buf == '|') {
            cmds[cmd_idx][arg_idx] = NULL;
            cmd_idx++;
            arg_idx = 0;
            buf++;
        } else {
            cmds[cmd_idx][arg_idx++] = buf;
            delim = strchr(buf, ' ');
            if (delim) {
                *delim = '\0';
                buf = delim + 1;
            } else {
                break;
            }
        }
        while (*buf == ' ') buf++;
    }
    // ★ 수정 끝

    cmds[cmd_idx][arg_idx] = NULL;

    /* Should the job run in the background? */
    if (arg_idx > 0) {
        char *last_arg = cmds[cmd_idx][arg_idx - 1];
        int len = strlen(last_arg);
        if (len > 0 && last_arg[len - 1] == '&') {
            *bg = 1;
            if (len == 1) {
                // 단독 &였던 경우: 그냥 삭제
                cmds[cmd_idx][--arg_idx] = NULL;
            }
        } else {
            *bg = 0;
        }
    }
    return cmd_idx + 1;
}

/* $end parseline */

  /* "ls| grep filename"이나 "ls |grep filename"과 같은 명령어도 처리하기 위해 operator의 양옆으로 공백을 추가 */
void insert_spaces_around_operators(char *cmdline, char *newbuf) {
    char *p = cmdline;
    char *q = newbuf;

    while (*p) {
        if (*p == '|' || *p == '<' || *p == '>' || *p == '&') {
            if (q != newbuf && *(q-1) != ' ') *q++ = ' ';
            *q++ = *p++;
            if (*p != ' ') *q++ = ' ';
        } else {
            *q++ = *p++;
        }
    }
    *q = '\0';
}

void strip_quotes(char *s) {
    int len = strlen(s);
    if (len >= 2 && s[0] == '"' && s[len - 1] == '"') {
        // Shift 내용 왼쪽으로 한 칸씩 밀기
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0'; // 맨 뒤 따옴표 제거
    }
}
/* $end parseline */

/* Signal Handlers */
void sigchld_handler(int sig)
{
    int old_errno = errno; // Back-up errno
    int status;
    while ((waitpid(-1, &status, WNOHANG)) > 0);
    errno = old_errno; // Restore errno
}
