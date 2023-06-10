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
#include <errno.h>
#include <unistd.h>

typedef struct {
    char voterName[30];
    char vote[20];
    struct VoteTuple *next;
} VoteTuple;

typedef struct {
    char partyName[20];
    int votes;
    struct votesPerParty *next;
} votesPerParty;

typedef struct {
    int bufferSize;
    int counter;
    int start;
    int end;
    int *bufferArray;
    char pollLog[20];
    char pollStats[20];
    VoteTuple *voterList;
    votesPerParty *votesPerPartyList;
} GlobalData;


int whileLoopBoolean = 0;
int terminateThreadSIGINT = 0;
pthread_cond_t condNotFull;
pthread_cond_t condNotEmpty;
GlobalData globalData;
pthread_mutex_t mtx;

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

void sigintHandler(int signum) {
    if (signum == SIGINT) {
        terminateThreadSIGINT = 1;
        whileLoopBoolean = 1;
        pthread_cond_broadcast(&condNotEmpty);
    }
}

void trimStrings(char *string) {
    uint len = strlen(string);
    for (int i = 0; i < len; i++) {
        if (string[i] == '\n' || string[i] == '\r') {
            string[i] = '\0';
            break; // Assuming there is only one occurrence of newline/carriage return
        }
    }
}

void printVotesPerParty(){
    votesPerParty *current = globalData.votesPerPartyList;
    FILE *file = fopen(globalData.pollStats, "input.txt");
    char string[50];
    char numStr[20]; // Buffer to hold the string representation of the integer
    while(current != NULL){
        strcpy(string,"");
        strcat(string, current->partyName);
        strcat(string, " ");
        sprintf(numStr, "%d", current->votes);
        strcat(string, numStr);
        strcat(string, "\n");
        fwrite(string, sizeof(char), strlen(string), file);
        current = (votesPerParty *)current->next;
    }
    fclose(file);
}

int findParty(votesPerParty *vpp, char *party) {
    votesPerParty *current = vpp;
    while(current != NULL){
        if(strcmp(current->partyName, party) == 0){
            current->votes ++;
            return 1;
        }
        current = (votesPerParty *)current->next;
    }
    return 0;
}

void addParty(votesPerParty **vpp, char *party) {
    votesPerParty *newNode = (votesPerParty *) malloc(sizeof(votesPerParty));
    if (newNode == NULL) {
        perror("Failed to allocate memory for vote");
        exit(1);
    }

    strcpy(newNode->partyName, party);
    newNode->votes = 1;
    newNode->next = NULL;

    if (*vpp == NULL) {
        *vpp = newNode;
    } else {
        votesPerParty *current = *vpp;
        while (current->next != NULL) {
            current = (votesPerParty *) current->next;
        }
        current->next = (struct votesPerParty *) newNode;
    }
}

void addVote(VoteTuple **vt, char *name, char *vote) {
    VoteTuple *newNode = (VoteTuple *) malloc(sizeof(VoteTuple));
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
        VoteTuple *current = *vt;
        while (current->next != NULL) {
            current = (VoteTuple *) current->next;
        }
        current->next = (struct VoteTuple *) newNode;
    }

    if(!findParty(globalData.votesPerPartyList, vote))
        addParty(&globalData.votesPerPartyList, vote);
}

int findName(VoteTuple *vt, const char *name) {
    VoteTuple *current = vt;
    while (current != NULL) {
        if (strcmp(current->voterName, name) == 0)
            return 1;
        current = (VoteTuple *) current->next;
    }
    return 0;
}

void initializeGlobalData(GlobalData *gd, int bufferSize, char *pollLog, char *pollStats) {
    gd->bufferSize = bufferSize;
    gd->end = -1;
    gd->start = 0;
    gd->counter = 0;
    gd->bufferArray = malloc(bufferSize * sizeof(int));
    strcpy(gd->pollLog, pollLog);
    strcpy(gd->pollStats, pollStats);
    gd->voterList = NULL;
    gd->votesPerPartyList = NULL;
}

void *workerThread(void *args) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);


    while(!terminateThreadSIGINT) {
        lockMutex(mtx);
        GlobalData *data = (GlobalData *) args;
        while (data->counter == 0 && !terminateThreadSIGINT) {
            pthread_cond_wait(&condNotEmpty, &mtx);
        }
        if(terminateThreadSIGINT)
            break;
        char sendNameMessage[] = "SEND NAME PLEASE\n";
        write(data->bufferArray[data->start], sendNameMessage, strlen(sendNameMessage));
        char bufferName[100];
        read(data->bufferArray[data->start], bufferName, 100);
        char sendVoteMessage[] = "SEND VOTE PLEASE\n";
        write(data->bufferArray[data->start], sendVoteMessage, strlen(sendVoteMessage));
        char bufferVote[50];
        read(data->bufferArray[data->start], bufferVote, 100);
        trimStrings(bufferName);
        trimStrings(bufferVote);

        if (!findName(globalData.voterList, bufferName)) {
            addVote(&globalData.voterList, bufferName, bufferVote);
            strcat(bufferName, " ");
            strcat(bufferName, bufferVote);
            strcat(bufferName, "\n");
            printf("Name and Vote is %s", bufferName);
            FILE *file = fopen(data->pollLog, "input.txt");
            fwrite(bufferName, sizeof(char), strlen(bufferName), file);
            fclose(file);
        } else {
            char alreadyVotedMsg[] = "ALREADY VOTED\n";
            write(data->bufferArray[data->start], alreadyVotedMsg, strlen(alreadyVotedMsg));
        }
        close(data->bufferArray[data->start]);
        data->start = (data->start + 1) % data->bufferSize;
        data->counter--;
        pthread_cond_signal(&condNotFull);
        unlockMutex(mtx);
    }
    unlockMutex(mtx);
    pthread_exit(NULL);
}



int main(int argc, char **argv) {
    if (argc != 6) {
        printf("Insufficient number of arguments, exiting");
        exit(EXIT_FAILURE);
    }

    static struct sigaction sa2;
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = sigintHandler;
    sa2.sa_flags = 0;
    sigaction(SIGINT, &sa2, NULL);

    int portnum = atoi(argv[1]);
    int numOfWorkingThreads = atoi(argv[2]);
    int bufferSize = atoi(argv[3]);
    char *pollLog = argv[4];
    char *pollStats = argv[5];

    int sock, newSock;
    struct sockaddr_in server, client;
    socklen_t clientlen = sizeof(client);
    struct sockaddr *clientptr = (struct sockaddr *) &client;
    struct sockaddr *serverptr = (struct sockaddr *) &server;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        exit(EXIT_FAILURE);
    server.sin_family = AF_INET;       /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(portnum);      /* The given port */
    /* Bind socket to address */
    if (bind(sock, serverptr, sizeof(server)) < 0)
        exit(EXIT_FAILURE);
    /* Listen for connections */
    if (listen(sock, 128) < 0)
        exit(EXIT_FAILURE);

    printf("Listening for connections to port %d\n", portnum);

    pthread_t *workers = (pthread_t *) malloc(numOfWorkingThreads * sizeof(pthread_t));

    initializeGlobalData(&globalData, bufferSize, pollLog, pollStats);

    pthread_mutex_init(&mtx, 0);
    pthread_cond_init(&condNotFull, 0); /* Initialize condition variable */
    pthread_cond_init(&condNotEmpty, 0); /* Initialize condition variable */

    for (int i = 0; i < numOfWorkingThreads; i++) {
        pthread_create(&workers[i], NULL, workerThread, &globalData);
    }
    while (!whileLoopBoolean) {
        newSock = accept(sock, clientptr, &clientlen);
        if (newSock < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                exit(EXIT_FAILURE);
            }
        }

        lockMutex(mtx);
        while (globalData.counter == globalData.bufferSize) {
            pthread_cond_wait(&condNotFull, &mtx);
        }
        globalData.end = (globalData.end + 1) % bufferSize;
        globalData.bufferArray[globalData.end] = newSock;
        globalData.counter++;
        pthread_cond_signal(&condNotEmpty);
        unlockMutex(mtx);
    }

    int lastInsertions;
    lockMutex(mtx);
    lastInsertions = globalData.counter;
    unlockMutex(mtx);
    while (lastInsertions > 0) {
        pthread_cond_signal(&condNotEmpty);
        lastInsertions--;
    }

    lockMutex(mtx);
    printVotesPerParty();
    unlockMutex(mtx);

    for (int i = 0; i < numOfWorkingThreads; i++) {
        pthread_join(workers[i], 0);
    }
    pthread_cond_destroy(&condNotEmpty);
    pthread_cond_destroy(&condNotFull);
    pthread_mutex_destroy(&mtx);
    return 0;
}
