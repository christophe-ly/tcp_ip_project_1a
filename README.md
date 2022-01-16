
# PROJET PROGRAMMATION SYSTEME - LY Christophe et LI Joey

--------------------------------------------------------

# I. Présentation du sujet 

Au cours de ce projet, nous avons été amené à mettre en place une application
client/serveur multithread communiquant via des sockets TCP/IP. Notre choix
d'application s'est porté sur une plateforme d'enchère où différents clients
peuvent se connecter afin de saisir des offres d'enchères. Les offres saisies 
sont envoyées au serveur qui fait office d'intermédiaire entre les clients. 

--------------------------------------------------------

# II. Mode opératoire

	# 1. Initialisation

Avant de commencer, un makefile est fourni et permet de compiler les différents
scripts qui composent l'application.

Pour lancer le serveur :	./serveur NUMERO_DE_PORT

Renseigner un entier pour le serveur qui correspond à l'intervalle de temps
signalant l'approche de la validation d'une offre en SECONDES. 
Il y a 3 appels sous la forme : ("OFFRE EUR. X fois." jusqu'à 3 fois)


Pour lancer un client :	./client localhost NUMERO_DE_PORT

Les clients étant gérés par les threads workers de la cohorte créé par le serveur,
il ne peut y avoir que 5 clients simultanément (nous avons fixé la taille de la
cohorte à 5).


	# 2. Fonctionnement

La session du client s'ouvre, il peut saisir une offre d'enchère.
	Si l'offre est la meilleure, le serveur le lui dit.
	Si elle n'est pas la meilleure, la meilleure offre est affichée.

D'autres clients peuvent saisir des offres, elles sont envoyées à chaque client
connecté.

A partir de la saisie d'une meilleure offre, un timer est remis à zéro et compte
tous les intervalles de temps renseigné à l'initialisation du serveur. Il fait
des appels à chacun de ces intervalles, qui sont envoyés à chaque client.

Si aucune meilleure offre n'est envoyée, le timer continue ses appels jusqu'au 
3e appel qui marque la fin de l'enchère et la victoire par le détenteur de l'offre.

A la fin de ce dernier appel, tous les clients sont déconnectés. L'enchère est 
finie.

--------------------------------------------------------

# III. Organisation du programme

L'application est composée de 2 scripts :

Un script client.c : qui comporte les éléments essentiels au fonctionnement du
client. Il y a notamment les 2 threads d'envoi / réception.

Un script serveur.c : qui comporte les éléments nécessaires au fonctionnement du
serveur. Il y a par exemple les threads worker, la session client ou le thread 
de timer d'une offre.

