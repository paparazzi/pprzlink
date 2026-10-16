# Remplacer le link OCaml par un link C++

Guide de travail du 29 septembre 2026. Les étapes ci-dessous sont à exécuter
dans l'ordre, en conservant les résultats de validation à chaque étape.
Mise en œuvre logicielle réalisée le 29 septembre 2026 ; voir
[le rapport d'implémentation et de comparaison](../validation/LINK_CPP_VALIDATION.md).
La validation sur les modems physiques et avec une session GCS reste distincte.

## Objectif et référence

Fournir un exécutable capable de remplacer
`sw/ground_segment/tmtc/link` dans Paparazzi, avec **les mêmes options de ligne
de commande, arguments, unités et valeurs par défaut**, sans modifier les
commandes existantes ni les messages attendus par le serveur et la GCS.

Références examinées :

- Paparazzi `55cdd96c46d49d15ddad9814ed24ecc9a4fda914` :
  `sw/ground_segment/tmtc/link.ml`, `sw/lib/ocaml/defivybus.ml`,
  `sw/lib/ocaml/serial.ml`, `conf/tools/data_link.xml` et le link combiner.
- PPRZLINK `756d00d1a88f8b7ad0a2c2a8ac6e98bf6fca34e6` :
  bibliothèque C++, bibliothèque OCaml v2 et transports OCaml communs.
- Ivy 3.18.3 `b0bf831702c5bfd1313e83ded62eb14d17198534`.

Le langage reste C++23, dans le sous-ensemble effectivement compilable avec
**GCC 13 fourni par Ubuntu 24.04**. Une réussite avec GCC 15 ou plus récent
ne remplace pas ce contrôle.

## Répartition du code

| Emplacement | Responsabilité |
| --- | --- |
| Bibliothèque `pprzlink` | Définitions XML, valeurs, codecs, transports, périphériques, métadonnées et compteurs techniques. |
| Agent `apps/link`, intégré aux cibles Make Paparazzi | Options, routage, état des avions, temporisations, rapports, retransmissions et politique de redondance. |
| Exemple `serial_messages` | Diagnostic série et XBee ; conserver son usage et ses tests. |

Développer d'abord `link++` à côté du link OCaml. Prévoir son intégration dans
`sw/ground_segment/tmtc`, puis la sélection de l'implémentation par le système
de construction. Au basculement, le chemin historique `sw/ground_segment/tmtc/link`
doit rester utilisable. Prévoir une cible ou un binaire distinct pour revenir
à OCaml. Le code de l'agent est conservé dans le sous-module, séparé de la
bibliothèque, pour partager sa construction autonome et ses tests. L'intégration
Paparazzi utilise `LINK_IMPL=cpp` ou `LINK_IMPL=ocaml`. Les anciens exécutables
présents n'ont pas été remplacés pendant l'implémentation.

## Contrat de ligne de commande

Les 24 options explicites de `link.ml` doivent toutes être acceptées. Ne pas
renommer les options avec des tirets doubles ni remplacer les underscores.

| Option | Argument / défaut | Effet à conserver |
| --- | --- | --- |
| `-b` | Adresse Ivy ; `IVY_BUS`, sinon `127.255.255.255:2010` sous Linux | Choisir le bus Ivy. |
| `-d` | Chemin ; `/dev/ttyUSB0` | Port série ou fichier/pipe. |
| `-fg` | Drapeau ; désactivé | Statistiques de trafic sur stdout. |
| `-noac_info` | Drapeau ; diffusion ACINFO activée par défaut | Désactiver ACINFO et ACINFO_LLA en uplink. |
| `-nouplink` | Drapeau ; uplink activé par défaut | Désactiver les commandes montantes et les PING. |
| `-s` | Débit en bauds ; **9600** | Configurer le débit série. |
| `-ch` | Entier ; canal non modifié | Configurer le canal XBee. |
| `-hfc` | Drapeau ; désactivé | Activer RTS/CTS. |
| `-local_timestamp` | Drapeau ; désactivé | Préfixer les messages Ivy émis avec un temps local relatif. |
| `-transport` | `pprz` ou `xbee` ; `pprz` | Choisir l'encapsulation radio. |
| `-udp` | Drapeau ; désactivé | Utiliser une socket UDP à la place du périphérique série. |
| `-udp_port` | Port ; 4242 | Port UDP de réception. |
| `-udp_uplink_port` | Port ; 4243 | Port UDP de destination des commandes. |
| `-udp_broadcast` | Drapeau ; désactivé | Émettre vers l'adresse de diffusion configurée. |
| `-udp_broadcast_addr` | Adresse ; `127.255.255.255` sous Linux | Choisir l'adresse de diffusion UDP. |
| `-uplink` | Drapeau historique | Réactiver l'uplink ; conserver cet alias déprécié. |
| `-xbee_addr` | Entier ; `0x100` = 256 | Adresse radio locale MY. |
| `-xbee_retries` | Entier ; intention documentée : 10 | Limiter les tentatives XBee ; voir l'anomalie OCaml ci-dessous. |
| `-xbee_868` | Drapeau ; désactivé | Activer le format XBee 868. |
| `-redlink` | Drapeau ; désactivé | Encapsuler la télémétrie pour le link combiner. |
| `-id` | Entier ; -1 | Identifiant de liaison, distinct des identifiants avion et radio. |
| `-status_period` | Millisecondes ; 1000 | Période de LINK_REPORT. |
| `-ping_period` | Millisecondes ; 5000 | Période de PING. |
| `-ac_timeout` | Millisecondes ; 5000 | Expiration d'un avion connu ; valeur <= 0 pour la désactiver. |

Conserver aussi `-help` et `--help`, ajoutés par `Arg.parse`. Capturer les codes
de sortie, diagnostics, arguments manquants, options inconnues, options répétées,
entiers signés et notations numériques acceptées. L'ordre de `-nouplink` et
`-uplink` compte : la dernière occurrence détermine l'état final. Vérifier le
traitement OCaml des arguments positionnels, actuellement ignorés.

Sous macOS, les valeurs OCaml par défaut utilisent `224.255.255.255` à la place
de `127.255.255.255`. Les consigner, même si la validation initiale cible Linux.

Ne pas rendre `-messages` obligatoire : retrouver `messages.xml` selon la même
priorité que la bibliothèque OCaml : `PPRZLINK_DIR`, puis `PAPARAZZI_HOME/var`,
puis `/usr/share/pprzlink`. Conserver le nom Ivy `Link` et le message `READY`.
Les options supplémentaires du diagnostic ne deviennent pas des prérequis
pour lancer l'agent de remplacement.

### Différences à traiter explicitement

- Le diagnostic C++ utilise 57600 bauds par défaut ; le link compatible doit
  utiliser **9600**. Ne pas reprendre aveuglément les valeurs de `serial_messages`.
- L'autobaud C++ peut modifier puis sauvegarder le débit du modem avec `ATWR`.
  Pour le lancement compatible, garder un débit fixe et l'initialisation AT
  contrôlée (`targetBaudrate = std::nullopt`), sans changement ni sauvegarde
  automatique du débit. Conserver l'autobaud dans la bibliothèque et le diagnostic.
- `-xbee_retries` modifie par erreur `XB.my_addr` dans le code OCaml étudié.
  Conserver le nom et le type de l'option, mais la relier au nombre de tentatives
  dans le C++. Documenter cette correction, vérifier l'absence de changement MY
  et préciser le comptage initial/réémissions dans les tests.
- L'aide OCaml annonce aussi `modem` et `pprz2`, alors que le parseur de transport
  n'accepte que `pprz` et `xbee`. Prendre le comportement exécutable comme référence.
- La branche XBee 868 contient un encodage TX à vérifier : elle définit `0x10`
  mais écrit encore `0x00` dans `api_tx64`. Valider avec la documentation du modem
  et des captures indépendantes avant de définir les fixtures 868.
- Le paramètre de priorité des envois OCaml est inutilisé. Une file avec priorités
  n'est pas nécessaire pour la parité initiale.

La compatibilité doit couvrir les usages existants. Les anomalies corrigées sont
listées dans le rapport de migration et testées séparément ; une fonctionnalité
absente ne doit jamais être présentée comme une option opérationnelle.

## Étape 0 — Valider Ubuntu 24.04 et GCC 13

**État : réalisée pour la bibliothèque actuelle.** Voir
[le rapport Ubuntu 24.04 / GCC 13](../validation/VALIDATION_UBUNTU24_GCC13.md).

1. Identifier la distribution, le compilateur réel et les dépendances.
2. Sélectionner explicitement `/usr/bin/g++-13`, y compris pour Ivy C++.
3. Construire les bibliothèques statique et partagée, les exemples et les tests.
4. Exécuter CTest avec accès aux sockets locales et aux pseudo-terminaux.
5. Vérifier l'installation, les consommateurs externes et le Makefile historique.
6. Vérifier que les binaires utilisent les bibliothèques officielles Noble,
   sans dépendance cachée à une libstdc++ de GCC 15/16.

**Critère de passage :** compilation et validations réussies, versions et
commandes archivées. La CI Ubuntu 24.04/GCC 13 a été ajoutée dans
`.github/workflows/cpp-ubuntu24.yml`, avec Ivy épinglé. Son exécution sur GitHub
reste à déclencher après publication ; les commandes sont validées localement.

## Étape 1 — Figer la référence et le banc de comparaison

1. Recompiler un link OCaml de référence dans un environnement compatible.
   Le vieux binaire présent était incompatible avec le runtime OCaml local.
   Le script `pprzlink/tests/build_ocaml_reference.py` fournit maintenant une
   référence recompilée, avec `link.ml` et ses transports inchangés.
2. Capturer son aide et vérifier les 24 options du tableau, leurs défauts et les
   combinaisons utilisées dans `conf/tools`, les sessions et scripts Paparazzi.
3. Préparer un client Ivy et un simulateur d'avion, utilisables sans matériel :
   pseudo-terminal pour PPRZ/XBee, socket pour UDP, messages XML de référence.
4. Faire émettre une télémétrie au simulateur dès son démarrage, puis répondre
   aux PING par PONG. Le link OCaml refuse les envois ciblés aux avions inconnus.
5. Capturer les trames et messages Ivy avec OCaml, puis rejouer les mêmes
   scénarios contre C++. Tolérer explicitement les variations de temps et
   d'identifiants de trame ; comparer exactement les champs et destinations.

**Critère de passage :** fixtures issues d'une référence indépendante du codec
C++, inventaire CLI complet et écarts OCaml connus consignés.

## Étape 2 — Compléter les métadonnées XML et les statistiques

Dans la bibliothèque :

1. Ajouter à `MessageDefinition` un mode de liaison représentant
   l'absence d'attribut, `forwarded` et `broadcasted`. Charger l'attribut `link`,
   rejeter une valeur inconnue et conserver l'accès aux messages par classe.
2. Exposer des statistiques des transports : trames reçues, erreurs de checksum,
   longueurs invalides et erreurs de décodage, avec des définitions précises.
3. Associer à chaque message reçu sa taille réelle sur le transport. Pour XBee,
   distinguer RX16/RX64 et les trames de statut ; ne pas confondre octets RF et UART.
4. Conserver les métadonnées XBee existantes et séparer adresse radio, sender
   PPRZLINK et identifiant de liaison.
5. Ajouter les tests de métadonnées XML et les fixtures de comptage nécessaires.
6. Conserver les formats XML pour les sorties Ivy compatibles et ajouter les
   champs 64 bits pris en charge par OCaml. Garder l'ancien codec C++ disponible.

Les erreurs d'une trame corrompue ne sont pas forcément attribuables à un avion.
Le link OCaml expose un compteur PPRZ global dans les rapports ; ne pas inventer
une attribution par avion. Définir séparément les améliorations de comptage XBee.

**Critère de passage :** aucun changement de représentation binaire des messages
valides, tests existants conservés, nouvelles données accessibles sans relire
le XML ou redécoder les octets dans l'application.

## Étape 3 — Construire l'agent série et sa CLI compatible

1. Créer `link++` avec un parseur correspondant au contrat ci-dessus.
   L'aide et les erreurs de syntaxe doivent être testables sans ouvrir de modem.
2. Charger le dictionnaire par les chemins d'environnement historiques.
3. Brancher `BoostSerialPortDevice`, `PprzTransport` ou `XbeeTransport` et `IvyLink`.
4. Sérialiser les accès aux transports et aux états de l'application. Si Ivy
   tourne sur son thread, transférer ses commandes vers la boucle Asio avant
   de modifier ces états. Définir un arrêt sans callback sur un objet détruit.
5. Republier les télémétries avec le sender PPRZLINK, indépendamment de l'adresse
   radio source. Traiter les messages invalides sans arrêter le bus Ivy.
6. S'abonner aux messages datalink marqués dans le XML. Pour `forwarded`, utiliser
   le champ `ac_id` comme destinataire ; pour `broadcasted`, utiliser 255.
   L'expéditeur binaire sol vaut 0. Appliquer les exceptions ACINFO/ACINFO_LLA.
7. Respecter `-nouplink`/`-uplink`, les paramètres série et la fermeture du
   périphérique. Une reconnexion automatique n'est pas requise par OCaml.

**Critère de passage :** les mêmes commandes de lancement ouvrent les mêmes
liaisons et les scénarios Ivy ↔ PPRZ/XBee passent dans les deux directions.
Cette étape produit une passerelle, pas encore le remplacement complet.

## Étape 4 — Ajouter le suivi des avions et LINK_REPORT

1. Créer un état par avion à la réception de sa première télémétrie valide.
2. Implémenter l'expiration contrôlée par `-ac_timeout` et le filtrage des envois
   ciblés. Désactiver l'expiration ne rend pas un avion inconnu automatiquement connu.
3. Envoyer les PING selon `-ping_period`, avec le décalage initial OCaml de 500 ms ;
   calculer le temps de réponse à la réception des PONG. Aucun PING si l'uplink
   est désactivé.
4. Publier LINK_REPORT selon `-status_period`, avec les noms, types et unités du
   XML : identifiants, durée, inactivité, compteurs RX/TX, erreurs, débits et ping.
5. Utiliser une horloge monotone pour les échéances ; réserver l'heure civile
   aux sorties qui la demandent. Tester le rapport avant la première réponse PONG,
   les avions expirés, plusieurs avions et le comptage des diffusions.
6. Reproduire `-fg` et préciser les différences intentionnelles par rapport
   aux approximations de temporisation/comptage de la référence OCaml.

**Critère de passage :** rapports acceptés par Paparazzi, absence de commandes
vers un avion expiré, temporisations et reprise après réception vérifiées.

## Étape 5 — Ajouter UDP et fichiers/pipes

1. Ajouter un adaptateur de flux pour les fichiers/descripteurs utilisés par
   `-d`, sans leur imposer les réglages termios d'un port série.
2. Réutiliser le framing PPRZ pour UDP. Séparer si nécessaire le codec de trames
   de l'accès `Device` ; ne pas recopier le parseur dans l'agent.
3. Préserver l'adresse source de chaque datagramme et l'association avec l'avion.
   Ne jamais fusionner les fragments de deux émetteurs dans un tampon commun.
4. Respecter réception sur `-udp_port`, émission vers `-udp_uplink_port`, choix
   de l'adresse du pair ou de `-udp_broadcast_addr` et option SO_BROADCAST.
5. Reproduire la diffusion aux avions connus et actifs ; caractériser les doublons
   historiques lorsque plusieurs avions partagent la même adresse de diffusion.
6. Tester plusieurs avions/adresses, datagrammes incomplets ou concaténés,
   expiration, trames invalides, fichiers/pipes et fermeture des descripteurs.

**Critère de passage :** mêmes lignes de commande UDP et fichier/pipe que la
référence, destinations correctes et aucune contamination entre pairs.

## Étape 6 — Finaliser la fiabilité XBee et le mode 868

1. Exploiter les statuts TX existants pour gérer les envois en attente.
2. Sur « no ACK », programmer une réémission bornée par `-xbee_retries` avec un
   délai comparable à OCaml (10 à 209 ms). Ne pas confondre ces tentatives côté
   hôte avec les retries internes du modem.
3. Empêcher la réutilisation d'un frame ID tant qu'un envoi attend son statut.
   Définir la gestion des statuts absents, tardifs, dupliqués et de la saturation.
   Étendre l'API de transport si le contrôle des IDs ou la réémission exacte
   l'exige ; la politique et les timers restent dans l'agent.
4. Conserver l'initialisation AT contrôlée et le blocage des envois avant succès.
5. Identifier le matériel/firmware 868 visé, vérifier sa spécification, ajouter
   les formats TX/RX/statut correspondants dans la bibliothèque et les tester
   avec des fixtures indépendantes. Le TX64 legacy actuel ne suffit pas.

**Critère de passage :** scénario de perte d'ACK validé, tentatives bornées,
identifiants correctement corrélés et `-xbee_868` effectivement opérationnel.
Si le matériel 868 n'est pas disponible, distinguer validation du format et
validation radio ; ne pas annoncer une parité matérielle complète.

## Étape 7 — Horodatage et liens redondants

1. Reproduire le préfixe temporel optionnel des télémétries et de LINK_REPORT.
2. Reproduire l'enveloppe Ivy
   `redlink TELEMETRY_MESSAGE <ac_id> <link_id> <message avec espaces remplacés par ;>`.
   Respecter l'ordre horodatage/encapsulation de la référence.
3. Tester `-redlink`, `-id` et leur combinaison avec `-local_timestamp`, y compris
   les chaînes, tableaux et messages sans champ. Conserver l'avertissement si
   un ID est fourni sans `-redlink`.
4. Vérifier la réception par le link combiner existant et le maintien de
   LINK_REPORT sous son format ground habituel.

**Critère de passage :** deux instances C++ sur des liaisons distinctes sont
acceptées par les outils de redondance sans adaptation de leurs abonnements.

## Étape 8 — Valider le remplacement et l'intégrer

1. Exécuter les tests de bibliothèque et d'agent sous Ubuntu 24.04/GCC 13 ;
   compiler aussi avec le compilateur récent utilisé pour le développement.
2. Rejouer le banc de comparaison complet OCaml/C++, en PPRZ, XBee et UDP.
3. Tester les commandes réelles des sessions Paparazzi, le serveur, la GCS et
   le link combiner. Vérifier les ports, environnements et messages par défaut.
4. Tester sur deux modems compatibles et un simulateur avion, puis sur le
   matériel prévu : réception, commandes, ACK, coupure et remise sous tension.
   Les tests sur pseudo-terminal ne valident pas la radio ni la flash physique.
5. Installer via les cibles de construction Paparazzi et conserver le chemin
   `sw/ground_segment/tmtc/link`, avec un retour possible à OCaml.
6. Mettre à jour README, guide d'installation, matrice de compatibilité et liste
   des différences intentionnelles ; relever les révisions exactes validées.

**Critère final :** un utilisateur remplace l'implémentation de `link`, conserve
sa ligne de commande et obtient les fonctions attendues par Paparazzi, sur un
programme construit avec le GCC 13 d'Ubuntu 24.04. Aucun mode manquant ne se
cache derrière une option acceptée sans effet.

## Suivi

- [x] Comparaison des sources et contrat CLI initial.
- [x] Validation de la bibliothèque actuelle sur Ubuntu 24.04 / GCC 13.
- [x] Référence OCaml exécutable et banc de comparaison.
- [x] Métadonnées XML, formats Ivy et statistiques des transports.
- [x] Agent série et CLI compatible.
- [x] Suivi des avions, PING/PONG et LINK_REPORT.
- [x] UDP et adaptateur de fichiers/pipes ; scénario comparatif sur descripteur.
- [x] Retransmissions XBee et formats 868, validés par simulation et fixtures.
- [x] Horodatage et format de redondance comparés à OCaml.
- [x] Cibles Make Paparazzi pour `link++`, `link_ocaml` et `LINK_IMPL`.
- [x] Définition CI Ubuntu 24.04/GCC 13.
- [ ] Exécution de la nouvelle CI sur GitHub après publication.
- [ ] Session complète serveur/GCS et link combiner avec les nouveaux binaires.
- [ ] Validation sur les modems physiques, y compris 868 si utilisé.

Pour chaque étape, noter les fichiers modifiés, commandes de validation,
résultats, différences de comportement et éventuelles limites matérielles.
Ne cocher une étape qu'après ses critères de passage.
