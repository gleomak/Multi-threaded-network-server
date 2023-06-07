#include <stdio.h>
#include <sys/wait.h>         /* sockets */
#include <sys/types.h>         /* sockets */
#include <sys/socket.h>         /* sockets */
#include <netinet/in.h>         /* internet sockets */
#include <netdb.h>             /* gethostbyaddr */
#include <pthread.h>
#include <stdlib.h>             /* exit */
#include <signal.h>          /* signal */
#include <string.h>
#include <unistd.h>

#define lockMutex(mtx) \
    do { \
        int lockErr; \
        if ((lockErr = pthread_mutex_lock(&(mtx))) != 0) { \
            fprintf(stderr, "Failed to lock mutex: %s\n", strerror(lockErr)); \
            exit(1); \
        } \
    } while (0)

#define unlockMutex(mtx) \
    do { \
        int lockErr; \
        if ((lockErr = pthread_mutex_unlock(&(mtx))) != 0) { \
            fprintf(stderr, "Failed to unlock mutex: %s\n", strerror(lockErr)); \
            exit(1); \
        } \
    } while (0)


int whileLoopBoolean = 0;

typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t condNotFull;
    pthread_cond_t condNotEmpty;
    int bufferSize;
    int counter;
    int start;
    int end;
    int *bufferArray;

} GlobalData;

void * workerThread(void *args) {
    GlobalData * globalData = (GlobalData *) args;
    lockMutex(globalData->mtx);
    while(globalData->counter == 0){
        pthread_cond_wait(&globalData->condNotEmpty, &globalData->mtx);
    }
    char* sendNameMessage= "SEND NAME PLEASE";
    write(globalData->bufferArray[globalData->start], sendNameMessage, strlen(sendNameMessage));
    char buffer[100];
    read(globalData->bufferArray[globalData->start], buffer, 100);
    printf("Name is %s\n",buffer);
    globalData->start = (globalData->start + 1) % globalData->bufferSize;
    globalData->counter --;
    unlockMutex(globalData->mtx);
    pthread_cond_signal(&globalData->condNotFull);
//    return (void *) 1;
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 6) {
        printf("Insufficient number of arguments, exiting");
        exit(EXIT_FAILURE);
    }

    int portnum = atoi(argv[1]);
    int numOfWorkingThreads = atoi(argv[2]);
    int bufferSize = atoi(argv[3]);
    char *pollLog = argv[4];
    char *pollStats = argv[5];

    int sock, newSock;
    struct sockaddr_in server, client;
    socklen_t clientlen = sizeof(client);
    struct sockaddr *clientptr=(struct sockaddr *)&client;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        exit(EXIT_FAILURE);
    server.sin_family = AF_INET;       /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(portnum);      /* The given port */
    /* Bind socket to address */
    if (bind(sock, (struct sockaddr *) &server, sizeof(server)) < 0)
        exit(EXIT_FAILURE);
    /* Listen for connections */
    if (listen(sock, 128) < 0)
        exit(EXIT_FAILURE);
    printf("Listening for connections to port %d\n", portnum);

    pthread_t *workers = (pthread_t *) malloc(numOfWorkingThreads * sizeof(pthread_t));
    GlobalData globalData;
    pthread_mutex_init(&globalData.mtx, NULL);
    pthread_cond_init(&globalData.condNotEmpty, NULL); /* Initialize condition variable */
    pthread_cond_init(&globalData.condNotFull, NULL); /* Initialize condition variable */
    globalData.bufferSize = bufferSize;
    globalData.end = -1;
    globalData.start = 0;
    globalData.counter = 0;
    globalData.bufferArray = malloc(bufferSize * sizeof(int));


    for (int i = 0; i < numOfWorkingThreads; i++) {
        pthread_create(&workers[i], NULL, workerThread, &globalData);
    }
    while (!whileLoopBoolean) {
        lockMutex(globalData.mtx);
        while(globalData.counter == bufferSize){
            pthread_cond_wait(&globalData.condNotFull, &globalData.mtx);
        }
        if ((newSock = accept(sock, clientptr, &clientlen)) < 0){
            printf("accept");
            exit(EXIT_FAILURE);
        }
        globalData.bufferArray[globalData.counter++] = newSock;
        globalData.end = (globalData.end + 1) % bufferSize;
        unlockMutex(globalData.mtx);
        pthread_cond_broadcast(&globalData.condNotEmpty);
    }

    return 0;
}
