### Rapport de projet AISE
### Membres du groupe
-1 Ahmed taleb BECHIR

-2 Valentin DENIS

### Choix d'implementation

## structure du projet

nous créons un serveur TCP sur le port 6379, le serveur prend également un argument d'intervalle de réplication de la part de l'utilisateur, qu'il utilise pour définir une variable globale destinée à indiquer la taille à laquelle le serveur enregistre la Hashtable sous forme de fichier JSON dans un répertoire appelé htDB 

le serveur le fait à l'aide de signaux, dont le signal SIGUSR1,
SIGUSR1 est déclenché par la fonction client_handler, qui lève le signal lorsque HashINSERTS atteint le seuil de réplication.
La fonction de gestion des signaux définit alors DATABASE_SAVE_FLAG à 1.

Le serveur fonctionne sur deux boucles, l'une infinie et l'autre qui vérifie si DATABASE_SAVE_FLAG est à 1 ou à 0.
Nous avons choisi cette implémentation parce que nous ne voulions pas déclarer notre hashtable comme une variable globale et utiliser ensuite la fonction de gestion du signal pour la sauvegarder, ce qui aurait eu un comportement indéfini, et nous devions nous assurer que notre hashtable est à l'abri des interférences et des débordements.

En ce qui concerne le multiplexage, nous avons choisi d'utiliser select pour pouvoir gérer plusieurs clients. Select nous a permis d'éviter d'utiliser des threads et d'éviter une dégradation des performances dans le cas de plusieurs clients simultanés. 


En ce qui concerne l'atomicité des requêtes, nous avons défini deux objets, un objet de requête et un objet de réponse. Au début du serveur, nous créons les objets et une fois que nous démarrons la connexion sur le socket et commençons à accepter des clients, si les clients envoient une requête, nous réinitialisons d'abord l'objet, puis nous lisons la requête du client et l'analysons à l'aide de notre analyseur RESP et définissons l'objet de requête avec la réponse correspondante, puis à son tour, nous envoyons la réponse appropriée.

En ce qui concerne la réplication des données, nous avons mis en œuvre un mécanisme permettant à l'utilisateur, au démarrage du serveur, de déterminer s'il souhaite restaurer la dernière sauvegarde de la base de données et, dans la négative, de créer une hashtable vide.


# include folder 

-1 client.h
-2 server.h
-3 resp.h
-4 hastable.h
-5 replication.h

# src folder

-1 client.c
-2 server.c
-3 resp.c
-4 hastable.c
-5 replication.c

# htDB folder

contient les fichiers de la base de données souvegardé

# Resource folder
contains help text files

# Makefile

make server : pour compiler le serveur
make client : pour compiler le client
make clean : pour supprimer les fichiers objets et les fichiers exécutables
make DB : créer le répertoire htDB


# Commandes implementées
set      save

get      copy

ping    Exists

del      Help

quit     Echo

time    INFO

save   



#### References

https://h-digitalbusiness.com/multi-threading-and-multiplexing-in-c/
https://beej.us/guide/bgnet/html/
https://github.com/interma/RESP/tree/master
https://amitshekhar.me/blog/resp-redis-serialization-protocol
https://dzone.com/articles/parallel-tcpip-socket-server-with-multi-threading
https://redis.io/docs/reference/modules/
https://medium.com/@daijue/the-basics-of-replication-in-redis-4b92a3b275bd



```

