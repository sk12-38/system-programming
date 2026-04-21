/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#define MAXARGS 128
/* My macro variables in phase2 */
#define MAXCMDS 128
/* My macro variables in phase3 */
#define MAXJOBS 16   // 동시에 관리할 수 있는 최대 job 개수

/* My type declaration in phase3 */
typedef enum {
    UNDEF = -1, // job이 없는 상태
    FG    = 0,  // foreground에서 실행 중, phase2 코드와 호환을 위해 0으로 설정.
    BG    = 1,  // background에서 실행 중, phase2 코드와 호환을 위해 1로 설정.
    ST    = 2   // 중지된 상태 (stopped)
} job_state;

typedef struct job_t {
    pid_t pid;              // job의 프로세스 ID (보통 첫 번째)
    pid_t pgid;             // 프로세스 그룹 ID (== job 전체를 대표)
    int jid;                // job ID (1부터 시작)
    job_state state;        // job의 상태: UNDEF, FG, BG, ST
    char cmdline[MAXLINE];  // 사용자가 입력한 명령 전체
} job_t;

/* My global variables in phase3 */
struct job_t jobs[MAXJOBS]; // job 리스트
int nextjid = 1;            // 다음에 부여할 job ID
volatile sig_atomic_t fg_done = 0;  // foreground job이 끝났는지 표시하는 flag
volatile sig_atomic_t running = 0;  // pipeline으로 들어올 때 fork된 자식 프로세스들이 모두 끝났는지 count

/* Function prototypes */
void eval(char *cmdline);
// int parseline(char *buf, char **argv);
int builtin_command(char **argv);
/* My function in phase2 */
int parseline(char *buf, char *cmds[MAXCMDS][MAXARGS], int *bg);
void insert_spaces_around_operators(char *cmdline, char *newbuf);
void strip_quotes(char *s);
/* My functions in phase3*/
/* Job functions */
void waitfg(pid_t pgid);
pid_t fgpid(void);
void initjobs(void);
int addjob(pid_t pid, pid_t pgid, job_state state, const char *cmdline);
void deletejob(pid_t pid);
void listjobs(void);
job_t *getjobpid(pid_t pid);
job_t *getjobjid(int jid);
int parse_jid(char *arg);
void do_fg(char **argv);
void do_bg(char **argv);
void do_kill(char **argv);
/* Signal Handlers */
void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);

int main() {
    /* Initialize job table */
    initjobs();

    /* Install Signall Handlers */
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGINT, sigint_handler);     // Ctrl+C
    Signal(SIGTSTP, sigtstp_handler);   // Ctrl+Z
    
    char cmdline[MAXLINE]; /* Command line */

     while (1) {
        printf("CSE4100-SP-P2> ");
        fflush(stdout); // 출력 버퍼 flush (꼭 필요함!)

        // 사용자 입력 받기
        if (fgets(cmdline, MAXLINE, stdin) == NULL) {
            if (feof(stdin)) {
                // 사용자가 Ctrl+D를 눌러 EOF를 입력한 경우
                printf("\n");
                break;
            }
            if (errno == EINTR) {
                // 시그널 (Ctrl+C 등)로 인해 fgets가 중단된 경우
                clearerr(stdin); // 에러 상태 플래그 초기화
                continue;        // 프롬프트 다시 출력
            }
            // 기타 fgets 오류
            perror("fgets error");
            continue;
        }

        eval(cmdline); // 명령 실행
    }
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
/* Pipeline: pipefd[0]: read, pipefd[1]: write */
void eval(char *cmdline) {
    // char *argv[MAXARGS]; /* not used in phase2 */
    char buf[MAXLINE];      /* Holds modified command line */
    int bg;                 /* Should the job run in bg or fg? */
    pid_t pid, first_pid = -1;              /* Process id, and set first child of  */
    /* My local variables in Phase1 */
    sigset_t mask_all, mask_one, prev_one;
    char pathbuf[MAXLINE];  /* Holds path where user wants to move */
    /* My local variables in Phase2 */
    char *cmds[MAXCMDS][MAXARGS]; /* Saves command groups */
    int num_cmds;   /* The number of command */

    /* Setting mask */
    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);

    strcpy(buf, cmdline);
    num_cmds = parseline(buf, cmds, &bg);
    running = num_cmds;
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
        if ((pid = Fork()) == 0) {
            /* ----------- Child Process ----------- */
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);  /* Unblock SIGCHLD */
            setpgid(0, 0); // phase3: 자식 자신을 새로운 process group leader로 설정
            
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
        /* ----------- Parent Process ----------- */
        if (i == 0) {
            first_pid = pid; // phase3: 첫 번째 자식의 pid 저장
        }

        setpgid(pid, first_pid); // phase3: 모든 자식을 같은 프로세스 그룹으로 묶기. 리더는 첫 번째 자식 프로세스.

        if (oldfd != -1)
            close(oldfd);
        if (i < num_cmds - 1) {
            close(pipefd[1]);
            oldfd = pipefd[0];
        }
    }
    Sigprocmask(SIG_BLOCK, &mask_all, NULL);
    addjob(first_pid, first_pid, bg, cmdline);  // phase3: race-free로 등록
    Sigprocmask(SIG_SETMASK, &prev_one, NULL);  // phase3: 그 후에 unblock
    /* Parent waits for foreground job to terminate */
    if (!bg) {  // Foreground jobs
        waitfg(first_pid);
    } else {    // Background jobs
        job_t *job = getjobpid(first_pid);  // ★ job 정보 다시 가져오기
        if (job)
            printf("[%d] %d\n", job->jid, job->pgid); // ★ [jid] pgid 형식 출력
        else
            printf("[?] %d\n", first_pid); // fallback
    }
}
/* $end eval */

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) {
    int status;
    pid_t pid;
    sigset_t mask_all, prev_all;
    Sigfillset(&mask_all);
    /* builtin command는 user mode(쉘)에서 처리되므로 signal의 수신이 바로 일어나지 않을 수 있다. 따라서 수신할 시그널이 있는지 system call로 확인한다. */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        deletejob(pid);
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
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
    /* Jobs builtin commmand */
    if (!strcmp(argv[0], "jobs")) {
        listjobs();
        return 1;
    }
    if (!strcmp(argv[0], "fg")) {
        do_fg(argv);
        return 1;
    }
    if (!strcmp(argv[0], "bg")) {
        do_bg(argv);
        return 1;
    }
    if (!strcmp(argv[0], "kill")) {
        do_kill(argv);
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

/* foreground에 있는 process group이 제대로 reaping되도록 explicitly wait */
void waitfg(pid_t pgid) {   
    sigset_t mask;

    Sigemptyset(&mask);  // 모든 시그널 unblock
    fg_done = 0;  // 새 foreground job 시작할 때 flag 초기화
    while (!fg_done) {
        sigsuspend(&mask);  // SIGCHLD가 올 때까지 잠자기
    }
}

/* Job functions in phase3 */ 

pid_t fgpid(void) { /* 현재 foreground에 있는 job의 pgid를 반환 */
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state == FG)
            return jobs[i].pgid;
    }
    return 0; // 없으면 0 반환
}

void initjobs(void) {
    for (int i = 0; i < MAXJOBS; i++) {
        jobs[i].pid = 0;
        jobs[i].pgid = 0;
        jobs[i].jid = 0;
        jobs[i].state = UNDEF;
        jobs[i].cmdline[0] = '\0'; // 안전하게 빈 문자열로 초기화
    }
}

int addjob(pid_t pid, pid_t pgid, job_state state, const char *cmdline) {
    if (pid <= 0) return 0;

    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == 0) { // 빈 슬롯
            jobs[i].pid = pid;
            jobs[i].pgid = pgid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1; // 1부터 다시 순환
            strncpy(jobs[i].cmdline, cmdline, MAXLINE - 1);
            jobs[i].cmdline[MAXLINE - 1] = '\0'; // null-terminate 보장
            return 1;
        }
    }
    printf("addjob error: job list full\n");
    return 0;
}

void deletejob(pid_t pid) { /* 주어진 pid에 해당하는 job을 삭제 (즉, 빈 슬롯으로 되돌림) */
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].pid = 0;
            jobs[i].pgid = 0;
            jobs[i].jid = 0;
            jobs[i].state = UNDEF;
            jobs[i].cmdline[0] = '\0';
            return;
        }
    }
}

void listjobs(void) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state) {
                case BG:
                    printf("Running ");
                    break;
                case FG:
                    printf("Foreground ");
                    break;
                case ST:
                    printf("Stopped ");
                    break;
                default:
                    printf("Unknown ");
                    break;
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}

job_t *getjobpid(pid_t pid) {   /* job 리스트에서 pid에 해당하는 job의 주소를 찾아 반환 */
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid)
            return &jobs[i];
    }
    return NULL;
}

job_t *getjobjid(int jid) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid == jid)
            return &jobs[i];
    }
    return NULL;
}

int parse_jid(char *arg) {  /* job ID를 추출할 때, argument로 "%1"이나 "1" 모두 job ID가 1인 것으로 간주 */
    if (!arg) return -1;
    if (arg[0] == '%')
        return atoi(&arg[1]);  // %1 → 1
    else
        return atoi(arg);      // 1 → 1
}

void do_fg(char **argv) {
    if (!argv[1]) {
        printf("fg command requires a job ID\n");
        return;
    }

    int jid = parse_jid(argv[1]); // ✅ % 없음도 처리
    if (jid <= 0) {
        printf("fg: invalid job ID: %s\n", argv[1]);
        return;
    }

    job_t *job = getjobjid(jid);
    if (!job) {
        printf("fg: no such job: %d\n", jid);
        return;
    }

    kill(-job->pgid, SIGCONT);
    job->state = FG;
    waitfg(job->pgid);
}

void do_bg(char **argv) {
    if (!argv[1]) {
        printf("bg command requires a job ID\n");
        return;
    }

    int jid = parse_jid(argv[1]); // ✅ % 없음도 처리
    if (jid <= 0) {
        printf("bg: invalid job ID: %s\n", argv[1]);
        return;
    }

    job_t *job = getjobjid(jid);
    if (!job) {
        printf("bg: no such job: %d\n", jid);
        return;
    }

    kill(-job->pgid, SIGCONT);
    job->state = BG;
    printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
}

void do_kill(char **argv) {
    if (!argv[1]) {
        printf("kill: usage: kill [-s sigspec | -n signum | -sigspec] pid | jobspec ... or kill -l [sigspec]\n");
        return;
    }

    // --- Case 1: kill %jid (job ID) ---
    if (argv[1][0] == '%') {
        int jid = atoi(&argv[1][1]);
        if (jid <= 0) {
            printf("kill: invalid job ID: %s\n", argv[1]);
            return;
        }

        job_t *job = getjobjid(jid);
        if (!job) {
            printf("kill: %s: no such job\n", argv[1]);
            return;
        }

        // ✅ No root-process protection here (job is managed by shell)
        if (kill(-job->pgid, 0) < 0) {
            if (errno == ESRCH) {
                printf("kill: no such process group: %d\n", job->pgid);
                return;
            } else if (errno == EPERM) {
                printf("kill: permission denied for process group: %d\n", job->pgid);
                return;
            } else {
                perror("kill");
                return;
            }
        }

        // ✅ stopped 상태이면 먼저 SIGCONT 전송
        if (job->state == ST) {
            kill(-job->pgid, SIGCONT);
        }

        // ✅ 그 다음 SIGTERM 전송
        if (kill(-job->pgid, SIGTERM) < 0) {
            perror("kill error");
        }
    }

    // --- Case 2: kill pid (direct process) ---
    else {
        pid_t pid = atoi(argv[1]);

        // ✅ 추가: root 프로세스를 막기 위한 유저 권한 검사
        // 프로세스의 실제 UID를 가져와서 비교해야 하나, 쉘에서는 단순화해서 현재 사용자와 일치하는지만 확인
        if (kill(pid, 0) < 0) {
            if (errno == ESRCH) {   /* 해당되는 process ID가 없을 때 */
                printf("kill: (%d) - No such process\n", pid);
                return;
            } else if (errno == EPERM) {    /* 해당되는 process ID가 있지만 사용자에게 권한이 없을 때 */
                printf("kill: (%d) - Operation not permitted\n", pid);
                return;
            } else {
                perror("kill");
                return;
            }
        }
        // 실제로 시그널 전송
        if (kill(pid, SIGTERM) < 0) {
            perror("kill error");
        }
    }
}

/* Signal Handler in phase3 */
void sigchld_handler(int sig) {
    int status;
    int old_errno = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;

    Sigfillset(&mask_all);
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {  // ✅ WUNTRACED 추가 /* Reap a zombie child */
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        struct job_t *job = getjobpid(pid);
        if (job) {
            if (WIFSTOPPED(status)) {
                job->state = ST;
                Sio_puts("\n");
            } else if (WIFSIGNALED(status)) {
                deletejob(pid);
            } else if (WIFEXITED(status)) {
                deletejob(pid);
            }
        }
        running--;
        if (running == 0)
            fg_done = 1;
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    if (pid < 0 && errno != ECHILD)
        Sio_error("waitpid error");
    errno = old_errno;
}

void sigint_handler(int sig) {
    pid_t pgid = fgpid();
    if (pgid != 0) {
        // 포그라운드 job에 SIGINT 전달
        kill(-pgid, SIGINT);
        Sio_puts("\n");
    } else {
        // ❗ 포그라운드 job이 없으면 바로 프롬프트 출력
        Sio_puts("\nCSE4100-SP-P2> ");
        // 입력 대기 상태가 유지되므로 사용자에게 즉시 반응을 보여줄 수 있음
    }
}

void sigtstp_handler(int sig) {
    pid_t pgid = fgpid();
    if (pgid != 0) {
        // 포그라운드 job이 있으면 → 그 그룹에 SIGTSTP 전달
        kill(-pgid, SIGTSTP);
    } else {
        // 포그라운드 job이 없으면 → 프롬프트 다시 출력
        Sio_puts("\nCSE4100-SP-P2> ");
    }
}