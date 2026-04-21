/*
 * echo - read and echo text lines until client closes connection
 */
/* $begin echo */
#include "csapp.h"

void save_stock_table(void);
void handle_buy(int id, int amount, char* response);
void handle_sell(int id, int amount, char* response);
void handle_show(char* response);

void echo(int connfd) {
    size_t n;
    char buf[MAXLINE], response[MAXLINE], temp[MAXLINE], cmd[10];
    rio_t rio;
    int id, amount;

    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("Server received %d bytes\n", (int)n);
        
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
}
/* $end echo */

