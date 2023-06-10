#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>	     /* sockets */
#include <sys/socket.h>	     /* sockets */
#include <netinet/in.h>	     /* internet sockets */
#include <unistd.h>          /* read, write, close */
#include <netdb.h>	         /* gethostbyaddr */
#include <pthread.h>

typedef struct{
    char *voterName;
    char *partyName;
}voterAndParty;

int sock;
struct sockaddr_in server;
struct sockaddr *serverptr = (struct sockaddr*)&server;


voterAndParty *fillVoterAndPartyArray(char *inputFile, int *counter) {
    FILE *file = fopen(inputFile, "r");
    if (file == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    char line[50];
    while (fgets(line, sizeof(line), file) != NULL){
        (*counter)++;
    }
    fseek(file, 0, SEEK_SET);
    voterAndParty *array = (voterAndParty *) malloc((*counter) * sizeof(voterAndParty));
    (*counter) = 0;
    while(fgets(line, sizeof(line), file)){
        array[(*counter)].voterName = strtok(line, " ");
        array[(*counter)++].partyName = strtok(NULL, "");
    }
    fclose(file);
    return array;
}

void * clientThread(void * args){
    voterAndParty *vap = (voterAndParty *)args;
    char buffer[50];
    if (connect(sock, serverptr, sizeof(server)) < 0) {
        printf("connect");
        exit(EXIT_FAILURE);
    }
    if (read(sock, buffer, 50) < 0){
        printf("READ error\n");
        exit(EXIT_FAILURE);
    }
    if(strcmp(buffer, "SEND NAME PLEASE\n") !=0){
        printf("ERROR WITH READ MESSAGE\n");
        exit(EXIT_FAILURE);
    }
    size_t n = strlen(vap->voterName);
    write(sock, vap->voterName, n);
    memset(buffer, '\0', sizeof(buffer));
    read(sock, buffer, 50);
    if(strcmp(buffer, "SEND VOTE PLEASE\n") != 0){
        if(strcmp(buffer, "ALREADY VOTED") != 0){
            printf("ERROR WITH VOTE MESSAGE\n");
            exit(EXIT_FAILURE);
        }
    }else{
        size_t n2 = strlen(vap->partyName);
        write(sock, vap->partyName, n2);
        memset(buffer, '\0', sizeof(buffer));
        read(sock, buffer, 50);
    }
    pthread_exit(NULL);
}

int main(int argc, char **argv){
    if(argc != 4){
        printf("Insufficient number of arguments, exiting.\n");
        exit(EXIT_FAILURE);
    }
    char *serverName = argv[1];
    int portNum = atoi(argv[2]);
    char *inputFile = argv[3];

    FILE* file = fopen(inputFile, "r");
    if (file == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    int arraySize = 0;
    voterAndParty *voterAndPartyArray = fillVoterAndPartyArray(inputFile, &arraySize);
    pthread_t *workers = (pthread_t *) malloc(arraySize * sizeof(pthread_t));

    int port;
    struct hostent *rem;
    /* Create socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket error\n");
        exit(EXIT_FAILURE);
    }
    /* Find server address */
    if ((rem = gethostbyname(serverName)) == NULL) {
        printf("Get Host By Name error\n");
        exit(EXIT_FAILURE);
    }
    port = atoi(argv[2]); /*Convert port number to integer*/
    server.sin_family = AF_INET;       /* Internet domain */
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(port);         /* Server port */

    for(int i = 0 ; i < arraySize ; i++){
        pthread_create(&workers[i], 0, clientThread, (void *)&voterAndPartyArray[i]);
    }
    for (int i = 0; i < arraySize ; i++) {
        pthread_join(workers[i], 0);
    }

    return 0;
}