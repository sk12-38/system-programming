#include "csapp.h"
#define STOCK_NUM 10

/* My structure in task_1 */
typedef struct { /* Represents a pool of connected descriptors */
    int maxfd;        /* Largest descriptor in read_set */
    fd_set read_set;  /* Set of all active descriptors */
    fd_set ready_set; /* Subset of descriptors ready for reading */
    int nready;       /* Number of ready descriptors from select() */
    int maxi;         /* High water index into clientfd[] array */
    int clientfd[FD_SETSIZE];    /* Set of active descriptors */
    rio_t clientrio[FD_SETSIZE]; /* Set of active read buffers */
} pool;

typedef struct { /* Represents a stock info */
    int ID;         /* Stock ID */
    int left_stock; /* The amount of left stock */
    int price;      /* Price of stock */
} stock;

/* Binary Tree Node Structure */
typedef struct node {
    stock data;
    struct node *left;
    struct node *right;
} node;

/* Global variables */
node *root = NULL;

/* My global variables in task_1 */
int byte_cnt = 0; /* Counts total bytes received by server */

void echo(int connfd);
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void handle_request(pool *p);

/* Function prototypes */
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
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;

    /* SIGINT 또는 SIGTERM 시 cleanup 호출 */
    Signal(SIGINT, cleanup);
    Signal(SIGTERM, cleanup);

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    load_stock_table();  /* startup 시 파일에서 트리 로드 */

    while (1) {
        /* 읽기 가능한 descriptor 대기 */
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

        /* listenfd에 이벤트 발생하면 새로운 클라이언트 추가 */
        if (FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            add_client(connfd, &pool);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                        client_port, MAXLINE, 0);
            printf("\033[32mConnected\033[0m to (%s, %s)\n", client_hostname, client_port);
        }

        /* 준비된 각 클라이언트 요청 처리 */
        handle_request(&pool);
    }

    /* 이 코드에는 절대 도달하지 않음 */
    save_stock_table();
    free_tree(root);
    return 0;
}

void init_pool(int listenfd, pool *p) {
    int i;
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++)
        p->clientfd[i] = -1;

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p) {
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++) {
        if (p->clientfd[i] < 0) {
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);
            FD_SET(connfd, &p->read_set);
            if (connfd > p->maxfd)
                p->maxfd = connfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    }
    if (i == FD_SETSIZE)
        app_error("add_client error: Too many clients");
}

void handle_request(pool *p) {
    int i, connfd, n, id, amount;
    char buf[MAXLINE], response[MAXLINE], temp[MAXLINE], cmd[10];

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];

        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--;
            if ((n = Rio_readlineb(&p->clientrio[i], buf, MAXLINE)) != 0) {
                /* 정상적으로 클라이언트로부터 명령어를 받은 경우 */
                byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);

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
                        /* 클라이언트가 exit 명령으로 연결을 종료할 때 */
                        Close(connfd);
                        FD_CLR(connfd, &p->read_set);
                        p->clientfd[i] = -1;

                        // Modified: 모든 clientfd를 검사해서 남아있는 연결이 하나도 없는 경우에만 저장
                        {
                            int j, still_connected = 0;
                            for (j = 0; j < FD_SETSIZE; j++) {
                                if (p->clientfd[j] > 0) {
                                    still_connected = 1;
                                    break;
                                }
                            }
                            if (!still_connected) {
                                save_stock_table();  // Modified: 마지막 클라이언트가 나갔을 때만 호출
                            }
                        }
                        continue;
                    }
                }

                Rio_writen(connfd, response, MAXLINE);
            }
            /* EOF detected, remove descriptor from pool */
            else {
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;

                // Modified: EOF로 인해 연결이 끊길 때도 동일하게 검사
                {
                    int j, still_connected = 0;
                    for (j = 0; j < FD_SETSIZE; j++) {
                        if (p->clientfd[j] > 0) {
                            still_connected = 1;
                            break;
                        }
                    }
                    if (!still_connected) {
                        save_stock_table();  // Modified: 마지막 클라이언트가 나갔을 때만 호출
                    }
                }
            }
        }
    }
}


/* Create a new node */
node* create_node(int id, int left_stock, int price) {
    node* new_node = (node*)Malloc(sizeof(node));
    new_node->data.ID = id;
    new_node->data.left_stock = left_stock;
    new_node->data.price = price;
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
    FILE* fp = fopen("stock.txt", "w");
    if (fp == NULL) {
        fprintf(stderr, "Error opening stock.txt for writing\n");
        exit(1);
    }
    /* In-order traversal하여 정렬된 순서로 저장 */
    void save_node(node* node) {
        if (node == NULL) return;
        save_node(node->left);
        fprintf(fp, "%d %d %d\n", node->data.ID, node->data.left_stock, node->data.price);
        save_node(node->right);
    }
    save_node(root);
    fclose(fp);
}

/* Handle buy request */
void handle_buy(int id, int amount, char* response) {
    node* current = root;
    while (current != NULL) {
        if (id < current->data.ID) {
            current = current->left;
        } else if (id > current->data.ID) {
            current = current->right;
        } else {
            if (current->data.left_stock >= amount) {
                current->data.left_stock -= amount;
                strcpy(response, "[buy] \033[32msuccess\033[0m\n");
            } else {
                strcpy(response, "Not enough left stock\n");
            }
            return;
        }
    }
    strcpy(response, "Stock not found\n");
}

/* Handle sell request */
void handle_sell(int id, int amount, char* response) {
    node* current = root;
    while (current != NULL) {
        if (id < current->data.ID) {
            current = current->left;
        } else if (id > current->data.ID) {
            current = current->right;
        } else {
            current->data.left_stock += amount;
            strcpy(response, "[sell] \033[32msuccess\033[0m\n");
            return;
        }
    }
    strcpy(response, "Stock not found\n");
}

/* Handle show request */
void handle_show(char* response) {
    char temp[MAXLINE];
    response[0] = '\0';

    /* 재고 트리를 In-order 순서로 순회하며 문자열을 누적 */
    void show_node(node* node) {
        if (node == NULL) return;
        show_node(node->left);

        /* 읽기: ID, left_stock, price 출력 */
        snprintf(temp, MAXLINE, "%d %d %d\n", node->data.ID, node->data.left_stock, node->data.price);
        strcat(response, temp);

        show_node(node->right);
    }

    show_node(root);
}

/* Free the binary tree */
void free_tree(node* root) {
    if (root == NULL) return;
    free_tree(root->left);
    free_tree(root->right);
    Free(root);
}

/* Cleanup: save stock table and free binary tree when server is terminated by signal */
void cleanup(int sig) {
    save_stock_table();
    free_tree(root);
    printf("\n");
    exit(0);
}
