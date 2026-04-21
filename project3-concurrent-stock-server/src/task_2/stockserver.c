#include "csapp.h"
#define STOCK_NUM 10

/* My structure in task_2 */
typedef struct { /* Represents a stock info */
    int ID;            /* Stock ID */
    int left_stock;    /* The amount of left stock */
    int price;         /* Price of stock */
    int readcnt;       /* Reference count (reader count) */
    sem_t mutex;       /* Semaphore to protect readcnt */
    sem_t w;           /* Semaphore to serialize writers (and first/last reader) */
} stock;

/* Binary Tree Node Structure */
typedef struct node {
    stock data;
    struct node *left;
    struct node *right;
} node;

/* Global variables */
node *root = NULL;
pthread_mutex_t save_mutex = PTHREAD_MUTEX_INITIALIZER;

size_t byte_cnt = 0;                                 // Modified: 총 바이트 수
pthread_mutex_t byte_cnt_mutex = PTHREAD_MUTEX_INITIALIZER; // Modified: byte_cnt 보호

/* Modified: 전역 클라이언트 카운터와 그 보호용 뮤텍스 추가 */
int client_count = 0;                                  // Modified: 활성 클라이언트 수
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER; // Modified: client_count 보호

void echo(int connfd);
void *thread(void *vargp);

/* Function prototypes */
void handle_request(int connfd);
node* create_node(int id, int left_stock, int price);
node* insert_node(node* root, int id, int left_stock, int price);
void load_stock_table();
void save_stock_table();
void handle_buy(int id, int amount, char* response);
void handle_sell(int id, int amount, char* response);
void handle_show(char* response);
void free_tree(node* root);
void cleanup(int sig);

int main(int argc, char **argv) 
{
    int listenfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;
    int *connfdp;

    /* If stockserver is terminated by signal(e.g. SIGINT or SIGTERM), 
       call cleanup function to save stock table and free binary tree */
    Signal(SIGINT, cleanup);  // Ctrl+C
    Signal(SIGTERM, cleanup); // kill 명령어

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    load_stock_table();

    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
        printf("\033[32mConnected\033[0m to (%s, %s)\n", client_hostname, client_port);

        /* Modified: 새 쓰레드 생성 전 클라이언트 카운터 증가 */
        pthread_mutex_lock(&count_mutex);      // Modified: lock before increment
        client_count++;                        // Modified: 카운트 증가
        pthread_mutex_unlock(&count_mutex);    // Modified: unlock after increment

        Pthread_create(&tid, NULL, thread, connfdp);
    }
    /* 절대 도달하지 않음 */
    save_stock_table();
    free_tree(root);
    return 0;
}

void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    handle_request(connfd);
    /* Modified: 쓰레드 완료 시 클라이언트 카운터 감소 및 마지막 클라이언트일 경우 저장 */
    pthread_mutex_lock(&count_mutex);           // Modified: lock before decrement
    client_count--;                             // Modified: 카운트 감소
    if (client_count == 0) {                    // Modified: 마지막 클라이언트인지 확인
        save_stock_table();                     // Modified: 마지막 클라이언트가 나갔을 때 호출
    }
    pthread_mutex_unlock(&count_mutex);         // Modified: unlock after operation
    Close(connfd);
    return NULL;
}

void handle_request(int connfd) {
    size_t n;
    char buf[MAXLINE], response[MAXLINE], temp[MAXLINE], cmd[10];
    rio_t rio;
    int id, amount;

    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        /* Modified: 받은 바이트 수를 byte_cnt에 누적 및 출력 */
        pthread_mutex_lock(&byte_cnt_mutex);          // Modified: lock before update
        byte_cnt += n;                                // Modified: 누적
        printf("Server received %ld (%ld total) bytes on fd %d\n", n, byte_cnt, connfd);
        pthread_mutex_unlock(&byte_cnt_mutex);        // Modified: unlock after update
        
        // Parse and handle commands
        if (sscanf(buf, "%s", cmd) == 1) {
            if (strcmp(cmd, "buy") == 0) {
                if (sscanf(buf, "%s %d %d", cmd, &id, &amount) == 3) {
                    handle_buy(id, amount, response);
                    snprintf(temp, MAXLINE, "buy %d %d\n%s", id, amount, response);
                    strcpy(response, temp);
                }
            } else if (strcmp(cmd, "sell") == 0) {
                if (sscanf(buf, "%s %d %d", cmd, &id, &amount) == 3) {
                    handle_sell(id, amount, response);
                    snprintf(temp, MAXLINE, "sell %d %d\n%s", id, amount, response);
                    strcpy(response, temp);
                }
            } else if (strcmp(cmd, "show") == 0) {
                handle_show(response);
                snprintf(temp, MAXLINE, "show\n%s", response);
                strcpy(response, temp);
            } else if (strcmp(cmd, "exit") == 0) {
                save_stock_table();
                break;
            }
        }
        
        Rio_writen(connfd, response, MAXLINE);
    }
    /* Modified: EOF 또는 exit 이후에도 save_stock_table() 호출하지 않음 */
    return; /* Modified: 단순히 반환 */
}

/* Create a new node */
node* create_node(int id, int left_stock, int price) {
    node* new_node = (node*)malloc(sizeof(node));
    new_node->data.ID = id;
    new_node->data.left_stock = left_stock;
    new_node->data.price = price;
    new_node->data.readcnt = 0;
    Sem_init(&new_node->data.mutex, 0, 1);
    Sem_init(&new_node->data.w, 0, 1);  // writelock 초기화
    new_node->left = new_node->right = NULL;
    return new_node;
}

/* Insert a node into the binary tree */
node* insert_node(node* root, int id, int left_stock, int price) {
    if (root == NULL) {
        return create_node(id, left_stock, price);
    }
    
    if (id < root->data.ID) {
        root->left = insert_node(root->left, id, left_stock, price);
    } else if (id > root->data.ID) {
        root->right = insert_node(root->right, id, left_stock, price);
    }
    
    return root;
}

/* Load stock table from file */
void load_stock_table() {
    FILE* fp = fopen("stock.txt", "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening stock.txt\n");
        exit(1);
    }
    
    int id, left_stock, price;
    while (fscanf(fp, "%d %d %d", &id, &left_stock, &price) == 3) {
        root = insert_node(root, id, left_stock, price);
    }
    
    fclose(fp);
}

/* Save stock table to file */
void save_stock_table() {
    pthread_mutex_lock(&save_mutex);  // 파일 I/O 직렬화

    FILE* fp = fopen("stock.txt", "w");
    if (fp == NULL) {
        fprintf(stderr, "Error opening stock.txt for writing\n");
        pthread_mutex_unlock(&save_mutex);
        exit(1);
    }
    
    // In-order traversal to save in sorted order
    void save_node(node* node) {
        if (node == NULL) return;
        save_node(node->left);
        fprintf(fp, "%d %d %d\n", node->data.ID, node->data.left_stock, node->data.price);
        save_node(node->right);
    }
    
    save_node(root);
    fclose(fp);
    
    pthread_mutex_unlock(&save_mutex);
}

/* Handle buy request (writer) */
void handle_buy(int id, int amount, char* response) {
    node* current = root;
    while (current != NULL) {
        if (id < current->data.ID) {
            current = current->left;
        } else if (id > current->data.ID) {
            current = current->right;
        } else {
            /* Writer lock */
            P(&current->data.w);
            if (current->data.left_stock >= amount) {
                current->data.left_stock -= amount;
                strcpy(response, "[buy] \033[32msuccess\033[0m\n");
            } else {
                strcpy(response, "Not enough left stock\n");
            }
            V(&current->data.w);
            return;
        }
    }
    strcpy(response, "Stock not found\n");
}

/* Handle sell request (writer) */
void handle_sell(int id, int amount, char* response) {
    node* current = root;
    while (current != NULL) {
        if (id < current->data.ID) {
            current = current->left;
        } else if (id > current->data.ID) {
            current = current->right;
        } else {
            /* Writer lock */
            P(&current->data.w);
            current->data.left_stock += amount;
            strcpy(response, "[sell] \033[32msuccess\033[0m\n");
            V(&current->data.w);
            return;
        }
    }
    strcpy(response, "Stock not found\n");
}

/* Handle show request (reader) */
void handle_show(char* response) {
    char temp[MAXLINE];
    response[0] = '\0';
    
    /* 재귀적으로 in-order 순회하며 reader–writer 잠금 적용 */
    void show_node(node* node) {
        if (node == NULL) return;
        show_node(node->left);

        /* --- reader 진입 --- */
        P(&node->data.mutex);
        node->data.readcnt++;
        if (node->data.readcnt == 1) {
            /* 첫 번째 reader라면 writer 차단 */
            P(&node->data.w);
        }
        V(&node->data.mutex);

        /* 데이터 읽기 (mutex 해제 상태) */
        snprintf(temp, MAXLINE, "%d %d %d\n",
                 node->data.ID,
                 node->data.left_stock,
                 node->data.price);
        strcat(response, temp);

        /* --- reader 퇴장 --- */
        P(&node->data.mutex);
        node->data.readcnt--;
        if (node->data.readcnt == 0) {
            /* 마지막 reader라면 writer 해제 */
            V(&node->data.w);
        }
        V(&node->data.mutex);

        show_node(node->right);
    }
    
    show_node(root);
}

/* Free the binary tree */
void free_tree(node* root) {
    if (root == NULL) return;
    free_tree(root->left);
    free_tree(root->right);
    sem_destroy(&root->data.mutex);
    sem_destroy(&root->data.w);
    Free(root);
}

/* Cleanup: save stock table and free binary tree when server is terminated by signal */
void cleanup(int sig) {
    save_stock_table();
    free_tree(root);
    printf("\n");
    exit(0);
}
 