#include "./include/pse.h"
#include <time.h>
#include <sys/time.h>

#define CMD         "[SERVER]"
#define NB_WORKERS  5

void creerCohorteWorkers(void);
int chercherWorkerLibre(void);
void *threadWorker(void *arg);
void sessionClient(int canal, int id);
void updateNbConnected(int nb_connected, char ligne[LIGNE_MAX]);
void send_all(DataSpec dS[NB_WORKERS], char message[LIGNE_MAX]);
void send_other(DataSpec dS[NB_WORKERS], char message[LIGNE_MAX], int id);
void *threadTimer(void *arg);
void lockMutexMax(void);
void unlockMutexMax(void);
void lockMutexIdMax(void);
void unlockMutexIdMax(void);
void lockMutexCanal(int numWorker);
void unlockMutexCanal(int numWorker);

DataSpec dataSpec[NB_WORKERS];
float max; //stocke le meilleure offre
int id_max; //stocke l'id du thread worker du client de la meilleure offre
int fin = FAUX;
int enabled; //gère l'activation du timer
struct timeval begin; //fixe le temps de la proposition d'une meilleure offre
sem_t semWorkersLibres;

// Mutex pour acces concurrent à la valeur maximale proposée et au canal des workers
pthread_mutex_t mutexMax;
pthread_mutex_t mutexIdMax;
pthread_mutex_t mutexCanal[NB_WORKERS];

int main(int argc, char *argv[]) {
    short port;
    int ecoute, canal, ret;
    struct sockaddr_in adrEcoute, adrClient;
    unsigned int lgAdrClient;
    int numWorkerLibre;
    pthread_t idThread;

    if (argc != 2)
        erreur("usage: %s port\n", argv[0]);

    port = (short)atoi(argv[1]);

    creerCohorteWorkers();

    ret = sem_init(&semWorkersLibres, 0, NB_WORKERS);
    if (ret == -1)
        erreur_IO("init semaphore workers libres");

    printf("%s: creating a socket\n", CMD);
    ecoute = socket(AF_INET, SOCK_STREAM, 0);
    if (ecoute < 0)
        erreur_IO("socket");

    adrEcoute.sin_family = AF_INET;
    adrEcoute.sin_addr.s_addr = INADDR_ANY;
    adrEcoute.sin_port = htons(port);
    printf("%s: binding to INADDR_ANY address on port %d\n", CMD, port);
    ret = bind(ecoute, (struct sockaddr *)&adrEcoute, sizeof(adrEcoute));
    if (ret < 0)
        erreur_IO("bind");

    printf("%s: listening to socket\n", CMD);
    ret = listen(ecoute, 5);
    if (ret < 0)
        erreur_IO("listen");

    if(pthread_create(&idThread, NULL, threadTimer, &dataSpec)<0)
        erreur_IO("thread timer");

    while (VRAI) {
        printf("%s: accepting a connection\n", CMD);
        lgAdrClient = sizeof(adrClient);
        canal = accept(ecoute, (struct sockaddr *)&adrClient, &lgAdrClient);
        if (canal < 0)
            erreur_IO("accept");

        printf("%s: adr %s, port %hu\n", CMD,
               stringIP(ntohl(adrClient.sin_addr.s_addr)), ntohs(adrClient.sin_port));

        ret = sem_wait(&semWorkersLibres);
        if (ret == -1)
            erreur_IO("wait semaphore workers libres");
        numWorkerLibre = chercherWorkerLibre();

        dataSpec[numWorkerLibre].canal = canal;
        ret = sem_post(&dataSpec[numWorkerLibre].sem);
        if (ret == -1)
            erreur_IO("post semaphore worker");
    }

    if (close(ecoute) == -1)
        erreur_IO("fermeture ecoute");

    exit(EXIT_SUCCESS);
}

void creerCohorteWorkers(void) {
    int i, ret;

    for (i = 0; i < NB_WORKERS; i++) {
        dataSpec[i].canal = -1;
        dataSpec[i].tid = i;

        ret = sem_init(&dataSpec[i].sem, 0, 0);
        if (ret == -1)
            erreur_IO("init semaphore worker");

        ret = pthread_create(&dataSpec[i].id, NULL, threadWorker, &dataSpec[i]);
        if (ret != 0)
            erreur_IO("creation worker");
    }
}

// retourne le no. du worker ou -1 si pas de worker libre
int chercherWorkerLibre(void) {
    int i, canal;

    for (i = 0; i < NB_WORKERS; i++) {
        lockMutexCanal(i);
        canal = dataSpec[i].canal;
        unlockMutexCanal(i);
        if (canal < 0)
            return i;
    }

    return -1;
}

void *threadWorker(void *arg) {
    DataSpec *dataSpec = (DataSpec *)arg;
    int ret;

    while (VRAI) {
        ret = sem_wait(&dataSpec->sem);
        if (ret == -1)
            erreur_IO("wait semaphore worker");

        printf("worker %d: reveil\n", dataSpec->tid);

        sessionClient(dataSpec->canal, dataSpec->tid);

        lockMutexCanal(dataSpec->tid);
        dataSpec->canal = -1;
        unlockMutexCanal(dataSpec->tid);

        printf("worker %d: sommeil\n", dataSpec->tid);

        ret = sem_post(&semWorkersLibres);
        if (ret == -1)
            erreur_IO("post semaphore workers libres");
    }

    pthread_exit(NULL);
}

void sessionClient(int canal, int id) {
    char offreMax[LIGNE_MAX];
    char idMax[10];
    char ligne[LIGNE_MAX];
    int lgLue;
    int nb_connected=0;
    float value;

    memset(ligne, 0, sizeof(ligne));
    strcat(ligne, "Bienvenue sur la plateforme d'enchère, ID_CLIENT");
    sprintf(idMax, "%d", id);
    strcat(ligne, idMax);
    strcat(ligne, "\n");
    strcat(ligne, "L'offre du jour est un tableau de De Vinci.\n");
    updateNbConnected(nb_connected, ligne);

    if(send(canal, ligne, strlen(ligne), 0)<0)
        erreur_IO("intro auction");
    memset(ligne, 0, sizeof(ligne));

    gettimeofday(&begin, NULL);

    while (!fin) {
        lgLue = lireLigne(canal, ligne);
        if (lgLue == -1)
            erreur_IO("lecture canal");

        else if (lgLue == 0) {  // arret du client (CTRL-D, interruption)
            fin = VRAI;
            printf("%s: arret du client\n", CMD);
        }
        else {  // lgLue > 0
            if (strcmp(ligne, "fin") == 0) {
                fin = VRAI;
                printf("%s: fin session client\n", CMD);
                memset(ligne, 0, sizeof(ligne));
            }

            else {
                value = strtof(ligne, NULL);
                printf("%s Une proposition de %f EUR a été effectuée par ID_CLIENT%d.\n", CMD, value, id);
                if (value>max){
                    //l'offre proposée est la plus élevée
                    enabled=1;
                    gettimeofday(&begin, NULL);
                    lockMutexMax();
                    max = value;
                    unlockMutexMax();

                    lockMutexIdMax();
                    id_max=id;
                    unlockMutexIdMax();

                    memset(offreMax, 0, sizeof(offreMax));
                    sprintf(offreMax, "%f", max);

                    strcpy(ligne, CMD);
                    strcat(ligne, " Votre offre de ");
                    strcat(ligne, offreMax);
                    strcat(ligne, " EUR est la plus élevée.\n");
                    updateNbConnected(nb_connected, ligne);
                    if(send(canal, ligne, strlen(ligne),0)<0)//on envoie un message à l'enchérisseur de l'offre
                        erreur_IO("send message");
                    memset(ligne, 0, sizeof(ligne));
                    sprintf(idMax, "%d", id_max);
                    strcpy(ligne, CMD);
                    strcat(ligne, " Le client n°");
                    strcat(ligne, idMax);
                    strcat(ligne, " propose ");
                    strcat(ligne, offreMax);
                    strcat(ligne, " EUR.\n");
                    send_other(dataSpec,ligne, id);//on informe les autres participants
                }
                else {
                    //il existe une offre plus élevée
                    memset(offreMax, 0, sizeof(offreMax));
                    sprintf(offreMax, "%f", max);

                    sprintf(idMax, "%d", id_max);

                    strcpy(ligne, CMD);
                    strcat(ligne, " L'offre la plus élevée est actuellement de ");
                    strcat(ligne, offreMax);
                    strcat(ligne, " EUR (client n°");
                    strcat(ligne,idMax);
                    strcat(ligne,").\n");
                    updateNbConnected(nb_connected, ligne);
                    if(send(canal, ligne, strlen(ligne), 0)<0)
                        erreur_IO("send message");
                }
            }
        }

    }
    fin = FAUX;
    if (close(canal) == -1)
        erreur_IO("fermeture canal");

}

void send_all(DataSpec dS[NB_WORKERS], char message[LIGNE_MAX]){
    int nb_connected;

    if(sem_getvalue(&semWorkersLibres, &nb_connected)<0){
        erreur_IO("get sem value");
    }
    nb_connected = -nb_connected + NB_WORKERS;

    for(int i=0; i<nb_connected; i++){
        if(send(dS[i].canal, message, strlen(message), 0)<0)
            erreur_IO("send message in send_all");
    }
}

void send_other(DataSpec dS[NB_WORKERS], char message[LIGNE_MAX], int id){
    int nb_connected;

    if(sem_getvalue(&semWorkersLibres, &nb_connected)<0){
        erreur_IO("get sem value");
    }
    nb_connected = -nb_connected + NB_WORKERS;

    for(int i=0; i<nb_connected; i++){
        if(i!=id)
            if(send(dS[i].canal, message, strlen(message), 0)<0)
                erreur_IO("send message in send_other");
    }
}

void *threadTimer(void *arg){
    //variables permettant de print une seule fois les etapes de l'enchère
    char message[LIGNE_MAX];
    char offreMax[30];
    int intervalle;
    struct timeval current;
    struct timeval old_begin=begin;

    printf("%s Saisir l'intervalle de temps des messages annonçant la fin de l'enchère en secondes :", CMD);
    scanf("%d", &intervalle);

    while(VRAI){
        if(enabled==1){
            if(old_begin.tv_sec != begin.tv_sec){
                old_begin=begin;
            }
            gettimeofday(&current, NULL);
            memset(offreMax, 0, sizeof(offreMax));
            memset(message, 0, sizeof(message));

            //printf("Timestamp : %ld\n", current.tv_sec - begin.tv_sec);
            if(current.tv_sec - begin.tv_sec==intervalle){
                sprintf(offreMax, "%f", max);
                strcpy(message, CMD);
                strcat(message, offreMax);
                strcat(message, " EUR. Une fois \n");
                printf("%s\n", message);
                send_all(dataSpec, message);
            }

            else if(current.tv_sec - begin.tv_sec==2*intervalle){
                sprintf(offreMax, "%f", max);
                strcpy(message, CMD);
                strcat(message, offreMax);
                strcat(message, " EUR. Deux fois \n");
                printf("%s\n", message);
                send_all(dataSpec, message);
            }

            else if(current.tv_sec - begin.tv_sec==3*intervalle){
                sprintf(offreMax, "%f", max);
                strcpy(message, CMD);
                strcat(message, offreMax);
                strcat(message, " EUR. Trois fois. Adjugé vendu\n");
                printf("%s\n", message);
                send_all(dataSpec, message);
                fin = VRAI;
                enabled=0;
                max=0;
                //return NULL;
            }
            sleep(1);
        }
    }

    return NULL;
}

void updateNbConnected(int nb_connected, char ligne[LIGNE_MAX]){
    char nb[10];

    if(sem_getvalue(&semWorkersLibres, &nb_connected)<0){
        erreur_IO("get sem value");
    }
    nb_connected = -nb_connected + NB_WORKERS;
    sprintf(nb, "%d", nb_connected);
    strcat(ligne,nb);
    if (nb_connected==0 || nb_connected ==1)
        strcat(ligne, " personne connectée.\n");
    else
        strcat(ligne, " personnes connectées.\n");

}

void lockMutexMax(void)
{
    int ret;

    ret = pthread_mutex_lock(&mutexMax);
    if (ret != 0)
        erreur_IO("lock mutex maximum offer");
}

void unlockMutexMax(void)
{
    int ret;

    ret = pthread_mutex_unlock(&mutexMax);
    if (ret != 0)
        erreur_IO("unlock mutex maximum offer");
}

void lockMutexIdMax(void)
{
    int ret;

    ret = pthread_mutex_lock(&mutexIdMax);
    if (ret != 0)
        erreur_IO("lock mutex maximum offer");
}

void unlockMutexIdMax(void)
{
    int ret;

    ret = pthread_mutex_unlock(&mutexIdMax);
    if (ret != 0)
        erreur_IO("unlock mutex maximum offer");
}


void lockMutexCanal(int numWorker)
{
    int ret;

    ret = pthread_mutex_lock(&mutexCanal[numWorker]);
    if (ret != 0)
        erreur_IO("lock mutex canal");
}

void unlockMutexCanal(int numWorker)
{
    int ret;

    ret = pthread_mutex_unlock(&mutexCanal[numWorker]);
    if (ret != 0)
        erreur_IO("unlock mutex canal");
}
