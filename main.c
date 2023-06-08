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

typedef struct {
    char *voterName;
    char *vote;
    struct VoteTuple *next;
} VoteTuple;

typedef struct {
    int bufferSize;
    int counter;
    int start;
    int end;
    int *bufferArray;
    VoteTuple *voterList;
} GlobalData;

int whileLoopBoolean = 0;
pthread_mutex_t mtx;
pthread_cond_t condNotFull;
pthread_cond_t condNotEmpty;
GlobalData globalData;

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


void trimStrings(char *string) {
    uint len = strlen(string);
    for (int i = 0; i < len; i++) {
        if (string[i] == '\n' || string[i] == '\r') {
            string[i] = '\0';
            break; // Assuming there is only one occurrence of newline/carriage return
        }
    }
}

void addVote(VoteTuple **vt, char *name, char *vote){
    VoteTuple * newNode = (VoteTuple *)malloc(sizeof(VoteTuple));
    if (newNode == NULL) {
        perror("Failed to allocate memory for vote");
        exit(1);
    }

    strcpy(newNode->voterName, name);
    strcpy(newNode->vote, vote);
    newNode->next = NULL;

    if (*vt == NULL) {
        *vt = newNode;
    } else {
        VoteTuple * current = *vt;
        while (current->next != NULL) {
            current = (VoteTuple *) current->next;
        }
        current->next = (struct VoteTuple *) newNode;
    }
}

int findName(VoteTuple *vt, const char *name){
    VoteTuple *current = vt;
    while(current != NULL){
        if(strcmp(current->voterName, name) == 0)
            return 1;
        current = (VoteTuple *) current->next;
    }
    return 0;
}

void initializeGlobalData(GlobalData *gd, int bufferSize) {
    gd->bufferSize = bufferSize;
    gd->end = -1;
    gd->start = 0;
    gd->counter = 0;
    gd->bufferArray = malloc(bufferSize * sizeof(int));
    gd->voterList = NULL;
}

void *workerThread(void *args) {
    GlobalData *data = (GlobalData *) args;
    lockMutex(mtx);
    while (data->counter == 0) {
        pthread_cond_wait(&condNotEmpty, &mtx);
    }
    char sendNameMessage[] = "SEND NAME PLEASE\n";
    write(data->bufferArray[data->start], sendNameMessage, strlen(sendNameMessage));
    char bufferName[100];
    read(data->bufferArray[data->start], buffer, 100);
//    printf("Name is %s\n", buffer);
    char sendVoteMessage[] = "SEND VOTE PLEASE\n";
    write(data->bufferArray[data->start], sendVoteMessage, strlen(sendVoteMessage));
    char buffer2[100];
    read(data->bufferArray[data->start], buffer2, 100);
    printf("Name is %s, and Vote is %s\n", bufferName, buffer2);
    trimStrings(bufferName);
    if(!findName(globalData.voterList, bufferName)){
        addVote()
    }

    data->start = (data->start + 1) % data->bufferSize;
    data->counter--;
    unlockMutex(mtx);
    pthread_cond_signal(&condNotFull);
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
    struct sockaddr *clientptr = (struct sockaddr *) &client;
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
    initializeGlobalData(&globalData, bufferSize);

    pthread_mutex_init(&mtx, 0);
    pthread_cond_init(&condNotFull, 0); /* Initialize condition variable */
    pthread_cond_init(&condNotEmpty, 0); /* Initialize condition variable */

    for (int i = 0; i < numOfWorkingThreads; i++) {
        pthread_create(&workers[i], NULL, workerThread, &globalData);
    }

    while (!whileLoopBoolean) {
        lockMutex(mtx);
        while (globalData.counter == bufferSize) {
            pthread_cond_wait(&condNotFull, &mtx);
        }
        if ((newSock = accept(sock, clientptr, &clientlen)) < 0) {
            exit(EXIT_FAILURE);
        }
        globalData.bufferArray[globalData.counter++] = newSock;
        globalData.end = (globalData.end + 1) % bufferSize;
        pthread_cond_broadcast(&condNotEmpty);
        unlockMutex(mtx);
    }

    return 0;
}
