Project2 - Concurrent Stock Server 
=====================


#
#


> #### Manage on a per-node basis 

노드 단위의 세마포어가 동작한다. 즉 종목을 트리 상 하나의 노드로 만들고, 어떤 노드에 대하여 한 시점에 오직 하나의 쓰레드만이 노드를 업데이트할 수 있다. 어떤 시점에 여러 쓰레드가 여러 노드에 접근하여 작업을 수행할 수는 있는 것이다.


#


> #### readers-writers Problem

 어떤 쓰레드는 주식을 읽는 요청(show)을 처리하고, 어떤 쓰레드는 주식을 사거나 파는 요청을 처리한다. 

 주식 정보를 읽는 요청은 공유 변수(shared variable), 즉 주식의 개수와 값을 바꾸지 않으므로 어떤 종목에 대하여 한 시점에 여러 쓰레드가 읽기 요청을 수행할 수 있다. 

 그러나 주식을 사거나 파는 요청은 공유변수의 값을 바꾸므로 반드시 한 시점에 하나의 쓰레드만 업데이트 요청을 수행해야 한다. 

 본 프로그램에서는 readers-writers problem의 첫번째 solution을 채택하여 reader가 writer에 대하여 favor를 얻는다. 즉 어떤 노드에 대하여 읽기 작업이 수행 중이면```(readcnt >= 1)```  writer의 mutex에 락이 걸려```(P(&root->w))``` writer는 해당 노드에 접근할 수 없다. 더 이상 읽기 작업이 존재하지 않으면 그때서야 락이 풀려 쓰기 작업을 수행할 수 있다.


#
#




#### 주식 정보 읽기 (_tree traversal_)
```C
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
```


inorder로 tree를 순회하면서 주식 정보를 response[] 객체에 저장한다. 
노드 단위로 세마포어가 동작하므로 ```readcnt```는 함수를 호출할 때가 아니라 ```root->left```에 대한 재귀함수를 호출한 후 현재 노드에 대한 작업을 수행하기 직전에 업데이트한다. 


#
#


#### 주식 정보 업데이트 함수 
```
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
```

현재 노드의 ```w```세마포어를 락을 걸고 시작한다. 이 노드에 대해서 읽기 작업이 수행 중이면   ```w == 0```인 상태이므로 update함수는 _suspend_ 된다.




