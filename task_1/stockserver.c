/*
 *- An Event-driven echo server
 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *								 *
 * Sogang University						 *
 * Computer Science and Engineering				 *
 * System Programming						 *
 *								 *
 * Project name : Concurrent Stockserver(event-driven)					 *
 * FIle name    : stockserver.c     				 *
 * Author       : 20160051 Park JungHwan				 *
 * Date         : 2022 - 05 - 20				 *
 *								 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "csapp.h"

#define BUY 0
#define SELL 1
#define SHOW 2
#define EXIT 3

struct Node
{
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
    sem_t w;
    struct Node *left;
    struct Node *right;
    int height;
};
typedef struct
{
    int maxfd; // largest descriptor in read_set
    fd_set read_set;
    fd_set ready_set;
    int nready; // Number of ready descriptor
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;

void echo_cnt(int connfd);

void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);
void handle_msg(char *msg, char *response);

void echo(int connfd, struct Node *root);
void readStockFile(struct Node **root);
void getStockData(char *buffer, int stock_data[]);
void saveData(struct Node *root, FILE *fp);
void updateStock(char *response, struct Node *root, int option, int ID, int cnt);
struct Node *findNode(struct Node *root, int ID);
void decodeCommand(char *command, int order[]);
void readStockList(char *response, struct Node *root);
void printPreOrder(struct Node *root);
void sigint_handler(int sig);

struct Node *insertNode(struct Node *node, int stock_data[]);
struct Node *root = NULL;

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; /* Enough space for any address */ // line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;

    /*
    성능 분석을 위해 일시적으로 바꾼 코드

    */

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    signal(SIGINT, sigint_handler);

    readStockFile(&root);

    listenfd = Open_listenfd(argv[1]);

    init_pool(listenfd, &pool);

    while (1)
    {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &pool.ready_set))
        {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // Accept한다.
            Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                        client_port, MAXLINE, 0);
            // printf("Connected to (%s, %s)\n", client_hostname, client_port);

            add_client(connfd, &pool);
        }

        check_clients(&pool);
    }
}
/* $end echoserverimain */

void sigint_handler(int sig)
{
    sigset_t mask_all, prev_one;
    Sigfillset(&mask_all);

    Sigprocmask(SIG_BLOCK, &mask_all, &prev_one);

    FILE *fp = fopen("stock.txt", "w");
    saveData(root, fp);
    fclose(fp);

    Sigprocmask(SIG_BLOCK, &prev_one, NULL);

    exit(0);
}

void init_pool(int listenfd, pool *p)
{
    int i;
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++)
    {
        p->clientfd[i] = -1;
    }

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}
void add_client(int connfd, pool *p)
{
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++)
    {
        if (p->clientfd[i] < 0)
        {
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
        app_error("add_client error: Too many clients\n");
}
void check_clients(pool *p)
{
    int i, connfd, n;
    char msg[MAXLINE], response[MAXLINE];
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++)
    {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        if ((connfd > 0) && FD_ISSET(connfd, &p->ready_set))
        {
            p->nready--;
            if ((n = Rio_readlineb(&rio, msg, MAXLINE)) != 0)
            {
                handle_msg(msg, response);
                Rio_writen(connfd, response, MAXLINE);
            }
            else
            {
                Close(connfd);
                // printf("Disconnected with fd %d\n", connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
            }
        }
    }
}
void handle_msg(char *msg, char *response)
{
    int order[3];
    decodeCommand(msg, order);

    switch (order[0])
    {
    case (BUY):
    case (SELL):
        updateStock(response, root, order[0], order[1], order[2]); // writers
        break;
    case (SHOW):
        response[0] = '\0';
        readStockList(response, root); // readers
        break;
    case (EXIT):
        break;
    }
}
void readStockList(char response[], struct Node *root)
{
    int MAXLEN = 100;
    char tmp[MAXLEN];

    if (root != NULL)
    {
        readStockList(response, root->left);

        sprintf(tmp, "%d %d %d\n", root->ID, root->left_stock, root->price);
        strcat(response, tmp);

        readStockList(response, root->right);
    }
}
int max(int a, int b);

void readStockFile(struct Node **root)
{

    // 파일의 데이터를 읽어서 트리에 저장

    char buffer[MAXLINE];
    int stock_data[3];
    FILE *fp = fopen("stock.txt", "r");

    while (fgets(buffer, MAXLINE, fp) != 0)
    {
        getStockData(buffer, stock_data);
        *root = insertNode(*root, stock_data);
    }
    fclose(fp);
}
void getStockData(char *buffer, int stock_data[])
{

    char *temp = buffer;

    stock_data[0] = atoi(temp);

    for (int i = 1; i < 3; i++)
    {
        temp = strchr(temp, ' ');
        temp += 1;
        stock_data[i] = atoi(temp);
    }
}
void saveData(struct Node *root, FILE *fp)
{
    // 전위 탐색하면서 파일에 노드의 정보를 저장

    if (root != NULL)
    {
        saveData(root->left, fp);
        fprintf(fp, "%d %d %d\n", root->ID, root->left_stock, root->price);
        saveData(root->right, fp);
    }
}
void updateStock(char *response, struct Node *root, int option, int ID, int cnt)
{

    // option == 0: buy | option == 1: sell

    struct Node *node = findNode(root, ID);
    if (option == BUY)
    {
        if (node->left_stock < cnt)
            strcpy(response, "Not enough left stocks\n"); // 클라이언트에게 보내는 함수로 수정해야함.
        else
        {
            node->left_stock -= cnt;
            strcpy(response, "[buy] success\n");
        }
    }
    else
    {
        node->left_stock += cnt;
        strcpy(response, "[sell] success\n");
    }
}
struct Node *findNode(struct Node *root, int ID)
{
    if (root->ID == ID)
        return root;
    else if (ID > root->ID)
        return findNode(root->right, ID);
    else
        return findNode(root->left, ID);
}
void decodeCommand(char *command, int order[])
{
    // "buy 1 6" "show" "sell 2 3" "exit"

    if (strstr(command, "buy") || strstr(command, "sell"))
    {
        order[0] = (command[0] == 'b') ? BUY : SELL;
        char *temp = command;
        for (int i = 1; i < 3; i++)
        {
            temp = strchr(temp, ' ');
            temp += 1;
            order[i] = atoi(temp);
        }
    }
    else
    {
        order[0] = (command[0] == 's') ? SHOW : EXIT;
    }
}

/**********************
 * AVL Tree Function
 * ********************/

int height(struct Node *N)
{
    if (N == NULL)
        return 0;
    else
        return N->height;
}
int max(int a, int b)
{
    return (a > b) ? a : b;
}

// create a New Node
struct Node *newNode(int stock_data[])
{
    struct Node *node = (struct Node *)malloc(sizeof(struct Node));

    node->left = node->right = NULL;
    node->height = 1;
    node->readcnt = 0;

    node->ID = stock_data[0];
    node->left_stock = stock_data[1];
    node->price = stock_data[2];

    Sem_init(&node->mutex, 0, 1);
    Sem_init(&node->w, 0, 1);

    return node;
}

int getBalance(struct Node *N)
{
    if (N == NULL)
        return 0;
    return height(N->left) - height(N->right);
}

struct Node *rightRotate(struct Node *y)
{
    struct Node *x = y->left;
    struct Node *T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = max(height(y->left), height(y->right)) + 1;
    x->height = max(height(x->left), height(x->right)) + 1;

    return x;
}
struct Node *leftRotate(struct Node *x)
{
    struct Node *y = x->right;
    struct Node *T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = max(height(x->left), height(x->right)) + 1;
    y->height = max(height(y->left), height(y->right)) + 1;

    return y;
}
struct Node *insertNode(struct Node *node, int stock_data[])
{
    int ID = stock_data[0];

    if (node == NULL)
        return newNode(stock_data);
    if (ID < node->ID)
        node->left = insertNode(node->left, stock_data);
    else if (ID > node->ID)
        node->right = insertNode(node->right, stock_data);
    else
        return node;

    node->height = 1 + max(height(node->left), height(node->right));
    int balance = getBalance(node);

    if (balance > 1 && ID < node->left->ID) // 왼쪽으로 치우침 && 새로 삽입한 노드가 자식의 왼쪽 자식으로 삽입됨
        return rightRotate(node);

    if (balance < -1 && ID > node->right->ID)
        return leftRotate(node);

    if (balance > 1 && ID > node->left->ID)
    {
        node->left = leftRotate(node->left);
        return rightRotate(node);
    }

    if (balance < -1 && ID < node->right->ID)
    {
        node->right = rightRotate(node->right);
        return leftRotate(node);
    }
    return node; // well balanced
}
void printPreOrder(struct Node *root)
{
    if (root != NULL)
    {
        printPreOrder(root->left);
        printf("%d %d %d\n", root->ID, root->left_stock, root->price);
        printPreOrder(root->right);
    }
}
