#include "csapp.h"
#include <time.h>

#define MAX_CLIENT 100
#define ORDER_PER_CLIENT 10000000
#define STOCK_NUM 10
#define BUY_SELL_MAX 10

int main(int argc, char **argv)
{
	pid_t pids[MAX_CLIENT];
	int runprocess = 0, status, i;
	int option;
	int clientfd, num_client;
	char *host, *port, buf[MAXLINE], tmp[3];
	rio_t rio;

	/*
	Performance Test
	*/

	// FILE *fp = fopen("../test_result/num_client.txt", "a");
	//  FILE *fp = fopen("test_result/num_thread.txt", "a");
	//  int num_thread = atoi(argv[4]);
	/*
	Performance Test
	*/

	if (argc != 4)
	{
		fprintf(stderr, "usage: %s <host> <port> <client#>\n", argv[0]);
		exit(0);
	}

	host = argv[1];
	port = argv[2];
	num_client = atoi(argv[3]);

	struct timeval startTime, endTime;
	// unsigned long e_usec;

	gettimeofday(&startTime, NULL);

	/*	fork for each client process	*/
	while (runprocess < num_client)
	{
		// wait(&state);
		pids[runprocess] = fork();

		if (pids[runprocess] < 0)
			return -1;
		/*	child process		*/
		else if (pids[runprocess] == 0)
		{
			// printf("child %ld\n", (long)getpid());

			clientfd = Open_clientfd(host, port);
			Rio_readinitb(&rio, clientfd);
			srand((unsigned int)getpid());

			for (i = 0; i < ORDER_PER_CLIENT; i++)
			{
				option = rand() % 3; // 1. 요청을 섞어서 요청하는 경우
				// option = 0; // 2. 모든 클라이언트가 show만 요청하는 경우
				// option = rand() % 2 + 1; // 3. 모든 클라이언트가 buy 또는 sell 만 요청하는 경우

				if (option == 0)
				{ // show
					strcpy(buf, "show\n");
				}
				else if (option == 1)
				{ // buy
					int list_num = rand() % STOCK_NUM + 1;
					int num_to_buy = rand() % BUY_SELL_MAX + 1; // 1~10

					strcpy(buf, "buy ");
					sprintf(tmp, "%d", list_num);
					strcat(buf, tmp);
					strcat(buf, " ");
					sprintf(tmp, "%d", num_to_buy);
					strcat(buf, tmp);
					strcat(buf, "\n");
				}
				else if (option == 2)
				{ // sell
					int list_num = rand() % STOCK_NUM + 1;
					int num_to_sell = rand() % BUY_SELL_MAX + 1; // 1~10

					strcpy(buf, "sell ");
					sprintf(tmp, "%d", list_num);
					strcat(buf, tmp);
					strcat(buf, " ");
					sprintf(tmp, "%d", num_to_sell);
					strcat(buf, tmp);
					strcat(buf, "\n");
				}
				// strcpy(buf, "buy 1 2\n");

				Rio_writen(clientfd, buf, strlen(buf));
				Rio_readnb(&rio, buf, MAXLINE);
				Fputs(buf, stdout);
				usleep(1000000);
			}

			Close(clientfd);
			exit(0);
		}
		/*	parten process		*/
		/*else{
			for(i=0;i<num_client;i++){
				waitpid(pids[i], &status, 0);
			}
		}*/
		runprocess++;
	}
	for (i = 0; i < num_client; i++)
	{
		waitpid(pids[i], &status, 0);
	}

	gettimeofday(&endTime, NULL);

	// e_usec = (endTime.tv_sec * 1000000 + endTime.tv_usec) -
	//(startTime.tv_sec * 1000000 + startTime.tv_usec);

	// fprintf(fp, "%lu\n", e_usec);
	//  fprintf(fp, "%d\t%lu\n", num_thread, e_usec);

	// fclose(fp);

	return 0;
}
