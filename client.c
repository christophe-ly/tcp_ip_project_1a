#include "./include/pse.h"

#define CMD   "[CLIENT]"

DataSpec dataSpec;
float max;
int sent =1;
int fin = FAUX;

void *threadReception(void *arg);
void *threadEnvoi(void *arg);


int main(int argc, char *argv[]) {
    int clientSocket, ret;
    struct sockaddr_in *adrServ;
    pthread_t idThreadEnvoi, idThreadReception;

    signal(SIGPIPE, SIG_IGN);

    if (argc != 3)
        erreur("usage: %s machine port\n", argv[0]);

    printf("%s: creating a socket\n", CMD);
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0)
        erreur_IO("socket");

    printf("%s: DNS resolving for %s, port %s\n", CMD, argv[1], argv[2]);
    adrServ = resolv(argv[1], argv[2]);
    if (adrServ == NULL)
        erreur("adresse %s port %s inconnus\n", argv[1], argv[2]);

    printf("%s: adr %s, port %hu\n", CMD,
           stringIP(ntohl(adrServ->sin_addr.s_addr)),
           ntohs(adrServ->sin_port));

    printf("%s: connecting the socket\n", CMD);
    ret = connect(clientSocket, (struct sockaddr *)adrServ, sizeof(struct sockaddr_in));
    if (ret < 0)
        erreur_IO("connect");

    dataSpec.canal=clientSocket;

    while (!fin) {
        if(pthread_create(&idThreadReception, NULL, threadReception, &dataSpec)<0)
            erreur_IO("create receive thread");
        pthread_join(idThreadReception, NULL);
        if(sent==1){
            //pour print une seule fois le message de saisir d'offre
            sent=0;
            printf("%s Saisir votre offre.\n", CMD);
        }
        if(pthread_create(&idThreadEnvoi, NULL, threadEnvoi, &dataSpec)<0)
            erreur_IO("create send thread");
    }

    if (close(clientSocket) == -1)
        erreur_IO("fermeture socket");

    exit(EXIT_SUCCESS);
}

void *threadReception(void *arg){
    char buffer[LIGNE_MAX];
    memset(buffer, 0, sizeof(buffer));

    if(recv(dataSpec.canal, buffer, LIGNE_MAX, 0)<0){
        erreur_IO("Receive data from server");
    }

    else{
        printf("%s\n", buffer);
    }

    if(strstr(buffer, "Adjugé vendu\n")!=NULL){
        fin = VRAI;
    }

    return NULL;
}

void *threadEnvoi(void *arg){
    char buffer[LIGNE_MAX];
    //scanf("%s", &buffer);

    if (fgets(buffer, LIGNE_MAX, stdin) == NULL)
        fin = VRAI;
    else {
        sent=1;
        if (send(dataSpec.canal, buffer, strlen(buffer),0) <0)
            erreur_IO("ecriture socket");

        if (strcmp(buffer, "fin\n") == 0)
            fin = VRAI;
    }

    return NULL;
}
