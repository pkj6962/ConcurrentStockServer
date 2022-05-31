/*
 * echoserveri.c - An iterative echo server
 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *								 *
 * Sogang University						 *
 * Computer Science and Engineering				 *
 * System Programming						 *
 *								 *
 * Project name : Concurrent Stockserver(thread-driven)					 *
 * FIle name    : stockserver.c     				 *
 * Author       : 20160051 Park JungHwan				 *
 * Date         : 2022 - 05 - 20				 *
 *								 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "csapp.h"

#define SBUFSIZE 100
#define NTHREADS 20
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
    int *buf;    // connfd array
    int n;       // Maximum number of slots
    int front;   // buf[(front +1) % n] is first item
    int rear;    // buf[rear % n] is last item
    sem_t mutex; // protect accesses to buf:
    sem_t slots; // counts availbable slots
    sem_t items; // count available items;
} sbuf_t;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

void *thread(void *vargp);
void echo_cnt(int connfd);

void echo(int connfd, struct Node *root);
void readStockFile(struct Node **root);
void getStockData(char *buffer, int stock_data[]);
void saveData(struct Node *root, FILE *fp);
void updateStock(char *response, struct Node *root, int option, int ID, int cnt);
struct Node *findNode(struct Node *root, int ID);
void decodeCommand(char *command, int order[]);
void readStockList(char *response, struct Node *root);
void printPreOrder(struct Node *root);

struct Node *insertNode(struct Node *node, int stock_data[]);
void sigint_handler(int sig);

sbuf_t sbuf;
struct Node *root = NULL;

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; /* Enough space for any address */ // line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    /*
    성능 분석을 위해 일시적으로 바꾼 코드

    */

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    signal(SIGINT, sigint_handler);

    // int NTHREADS = atoi(argv[2]);

    readStockFile(&root);

    listenfd = Open_listenfd(argv[1]);

    sbuf_init(&sbuf, SBUFSIZE);
    for (int i = 0; i < NTHREADS; i++)
    {
        Pthread_create(&tid, NULL, thread, NULL);
    }

    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                    client_port, MAXLINE, 0);
        // printf("Connected to (%s, %s)\n", client_hostname, client_port);

        sbuf_insert(&sbuf, connfd);
    }

    FILE *fp = fopen("stock1.txt", "w");
    saveData(root, fp);
    fclose(fp);
    exit(0);
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

void *thread(void *vargp)
{
    Pthread_detach(pthread_self());
    while (1)
    {
        int connfd = sbuf_remove(&sbuf);
        echo_cnt(connfd);
        Close(connfd);
    }
}
void echo_cnt(int connfd)
{
    int n;
    int order[3];
    char message[MAXLINE], response[MAXLINE] = {0};

    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, message, MAXLINE)) != 0)
    {
        // printf("thread %d received %d bytes on fd %d\n", (int)pthread_self(), n, connfd);

        decodeCommand(message, order);

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

        Rio_writen(connfd, response, MAXLINE);
    }
}

void echo(int connfd, struct Node *root)
{
    int n;
    int order[3];
    char message[MAXLINE];
    char response[MAXLINE] = {0};
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, message, MAXLINE)) != 0)
    {
        printf("server received %d bytes\n", n);

        decodeCommand(message, order);

        switch (order[0])
        {
        case (BUY):
        case (SELL):
            updateStock(response, root, order[0], order[1], order[2]);
            break;
        case (SHOW):
            response[0] = '\0';
            readStockList(response, root);
            break;
        case (EXIT):
            break;
        }

        Rio_writen(connfd, response, MAXLINE);
    }
}

/************************
 * sbuf Package
 * *********************/

void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = (int *)malloc(sizeof(int) * n);
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}
void sbuf_insert(sbuf_t *sp, int item)
{

    P(&sp->slots); // 슬롯이 더 이상 남아 있지 않을 떄 다른 Producer Thread 를 블록하기 위해서
    P(&sp->mutex); // 다른 Threads 들의 버퍼에 대한 접근을 막기 위해서. Consumer Thread의 접근은 허용해도 되지 않나 싶은데...

    sp->buf[(++sp->rear) % (sp->n)] = item;

    V(&sp->mutex);
    V(&sp->items);
}
int sbuf_remove(sbuf_t *sp)
{

    int item;
    P(&sp->items);
    P(&sp->mutex);

    item = sp->buf[(++sp->front) % sp->n];

    V(&sp->mutex);
    V(&sp->slots);
    return item;
}

void readStockList(char response[], struct Node *root)
{
    int MAXLEN = 100;
    char tmp[MAXLEN];

    if (root != NULL)
    {
        readStockList(response, root->left);

        P(&root->mutex);
        root->readcnt++;
        if (root->readcnt == 1)
        {
            P(&root->w);
        }
        V(&root->mutex);

        sprintf(tmp, "%d %d %d\n", root->ID, root->left_stock, root->price);
        strcat(response, tmp);

        P(&root->mutex);
        root->readcnt--;
        if (root->readcnt == 0)
            V(&root->w);
        V(&root->mutex);

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
    P(&node->w);
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
    V(&node->w);
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
