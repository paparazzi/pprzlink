# Reprise pprzlink C++ — 27 septembre 2026

## Demande et décisions conservées

Ajouter le transport XBee API et l’initialisation du modem, en s’inspirant du
`link` OCaml de Paparazzi. Ajouter `-xbeesan` : contrôler la trame binaire avant
l’écriture série, bloquer l’envoi et afficher la raison sur stderr en cas d’erreur.
Le choix du transport se fait dans l’application, indépendamment du XML des messages.

Contraintes de la modernisation précédente : C++23, cible Ubuntu 24.04/GCC 13,
validation avec le GCC 15 installé sans télécharger GCC 13. Ivy 3.18 et son wrapper
C++ natif remplacent `../ivy-c++`, supprimé. Boost.Asio reste obligatoire pour le
pilote série ; Boost.Bimap est conservé. TinyXML2 lit le XML. Les concepts, les
valeurs stockées dans un variant et les conversions binaires/textuelles séparées
sont déjà en place. Recompiler les applications dépendantes après ces changements
d’API/ABI.

## Fonctionnalités réalisées

- `XbeeTransport.h/.cpp` : API historique XBee 802.15.4 **AP=1**, émission TX16/TX64,
  réception RX16/RX64, adresses radio, métadonnées RSSI et événements de statut.
  Charge RF PPRZLINK v2 sans enveloppe série `0x99` imbriquée, limite de 100 octets.
- `XbeeModem.h/.cpp` : initialisation sans attente bloquante dans la logique AT :
  garde, `+++`, garde, `ATMY`, éventuellement `ATCH`, `ATAP1`, `ATCN`.
  Autobaud activé par défaut, cible **57600 bauds** ou valeur de `-s`.
  Détection par `+++` puis lecture de `ATBD` ; si nécessaire : `ATBDn`,
  `ATCN` (OK à l’ancienne vitesse), changement de la vitesse hôte, nouvelle
  entrée AT et vérification BD, puis `ATWR` avec contrôle de son OK.
  Vérification des réponses, détection des erreurs et délais dépassés. Aucun
  envoi de message tant que l’initialisation n’a pas réussi.
  `ATWR` n’est envoyé que si la vitesse change, avant les commandes MY/CH/AP.
  Il sauvegarde aussi les autres paramètres courants du modem ; nos changements
  MY/CH/AP ultérieurs restent temporaires.
  Les écritures utilisent le contrat synchrone de Device ; les délais de réponse
  ne limitent pas la durée d’une écriture série bloquée.
- `-xbeesan` : contrôle complet de l’enveloppe API, type et taille d’en-tête,
  options réservées, checksum, taille RF et décodage du contenu avec le XML chargé.
  Refus avant l’écriture ; erreur sur stderr et retour non nul dans l’exemple.
  Dans la bibliothèque, activer `setSanityChecksEnabled(true)` et gérer l’exception.
- `examples/SerialMessages.cpp` : choix `-transport pprz|xbee`, initialisation
  automatique en mode XBee, options de configuration et de destination radio.
  C’est un exemple de diagnostic série, pas une réimplémentation complète du
  processus OCaml `link` ni une passerelle Ivy/série.
- Intégration CMake et documentation dans [référence C++](../reference.md) ; Make inclut les
  nouveaux fichiers grâce à ses listes automatiques de sources/en-têtes.

Les options principales sont `-xbeesan`, `-xbee_addr` (MY, défaut 0x100), `-ch`
(canal 0x0c..0x17), `-xbee-guard-ms` et `-xbee-timeout-ms` (défaut 2000 chacun),
`-xbee-no-init` pour un modem déjà configuré, `-xbee-dest` ou `-xbee-dest64`.
`-s` fixe la cible (défaut 57600), sans option nécessaire pour activer l’autobaud.
`-xbee-no-autobaud` permet un dialogue AT à vitesse fixe, sans lecture BD ni
écriture flash ; la vitesse hôte doit alors correspondre à celle du modem.
L’autobaud essaie la cible puis 9600, 57600, 115200, 38400, 19200, 4800, 2400,
1200 sans doublon. Les cibles hors de ces huit vitesses sont refusées.
Le PAN et, si nécessaire, la sortie API historique (`AO`) doivent être configurés
correctement. L’initialisation suppose le caractère d’échappement
`+` ; adapter le temps de garde à `GT` s’il a été modifié.

## Validation antérieure à l’autobaud (historique)

Validation normale : **11/11 tests réussis avec GCC 15.2**. Les tests couvrent les
trames exactes TX/RX, les erreurs de format et de XML, les gardes/réponses/délais AT,
le blocage des envois et les échanges du programme réel sur un pseudo-terminal.
L’installation CMake et deux consommateurs externes (liaison partagée et statique)
ont aussi été compilés et exécutés avec succès.

Validation ASan/UBSan/LeakSanitizer : **11/11 tests réussis**, avec détection des
fuites activée, pprzlink et Ivy corrigé instrumentés. Aucune erreur mémoire, fuite
ou erreur de comportement indéfini signalée pendant ces tests.
`git diff --check` est également passé.

Les fichiers temporaires de cette reprise sont dans `/tmp/pprzlink-resume` :
`build` (normal), `sanitized` (détecteurs mémoire), `deps` (dépendances extraites),
`ivy-src`/`ivy-fixed` (copie et compilation instrumentée d’Ivy corrigé),
`install` et `consumer` (vérification de l’installation).
Ces répertoires peuvent disparaître au redémarrage.

Depuis, Ivy **3.18.2** a été publié sur la branche `FEATURE/cpp_wrapper` du
GitHub d’Alex (commit `933e5a0fac1f18b7fdbbfe431c6df7dd570c5498`).
Paquets : `/home/alex/DEV/LIB/IVY/libivy-c/build/debian/3.18.2/`.
La conversion locale supplémentaire IvyContext/unique_ptr n’est pas dans cette
publication. Les tests instrumentés utilisent une compilation privée qui l’inclut.
Ne pas confondre ces installations de test avec les paquets installés sur le système.

La propriété exclusive des Device par les transports a été vérifiée séparément :
11/11 tests sous GCC 15, ASan/UBSan/LeakSanitizer et TSan. Les transports reçoivent
un `unique_ptr<Device>` et empruntent leur dictionnaire ; `setDevice()` est supprimé.

Pour l’autobaud, les constructions sont réutilisées dans
`/tmp/pprz-device-owner-5c7ao0j6/{gcc,asan,tsan}`. Le script, les environnements et
les journaux de cette nouvelle validation sont dans
`/tmp/pprz-xbee-autobaud-n1e2i57v/` :

```sh
python3 /tmp/pprz-xbee-autobaud-n1e2i57v/validate.py
```

Le script configure, compile et teste les trois variantes, puis vérifie le
chargement d’Ivy avec ldd. GCC utilise les headers et bibliothèques du paquet
3.18.2 extrait ; les variantes Clang utilisent Ivy instrumenté privé.
Résultat : **12/12 tests réussis dans chacune des trois variantes**, sans alerte
ASan/UBSan/fuite ni TSan. Voir [VALIDATION_CLANG22.md](../validation/VALIDATION_CLANG22.md) pour les résultats détaillés.
Si les répertoires temporaires ont disparu, reconstruire selon [référence C++](../reference.md)
et recréer les préfixes instrumentés pour les tests SAN.
Aucun autre compilateur n’a été téléchargé.

## Prochaine validation : matériel réel

Identifier la référence et le firmware du XBee. « 2,4 GHz » ne suffit pas : cette
implémentation cible l’API historique 802.15.4 AP=1, pas Zigbee, DigiMesh, 868 MHz
ou AP=2. Aucun test matériel ni essai GCC 13/Ubuntu 24.04 n’a été effectué.

Exemple à adapter au vrai port et fichier XML (autobaud vers 57600 par défaut) :

```sh
/tmp/pprz-device-owner-5c7ao0j6/gcc/serial_messages \
  -messages /chemin/vers/messages.xml -d /dev/ttyUSB0 \
  -transport xbee -xbeesan
```

Pour émettre un PING défini dans le XML vers l’avion/radio 42, ajouter
`-send "PING" -sender 0 -receiver 42 -duration 5`.
Vérifier les réponses AT, le passage depuis une autre vitesse vers 57600, puis
la conservation de la vitesse après coupure/remise sous tension. Vérifier ensuite
la réception et les statuts de transmission sur le modem. Les simulations sur
pseudo-terminal ne prouvent ni la persistance physique en flash ni les timings UART.
`-xbeesan` valide le format pris en charge ; il ne garantit ni la livraison radio,
ni une limite de charge propre à un autre firmware (pas de lecture de `NP`).

Le blocage historique du XBee sol nécessitait une coupure d’alimentation. Sa cause
n’est pas établie ; une trame invalide n’a pas été démontrée comme responsable.

## Partage de la branche

Les changements de cette reprise sont regroupés sur `FEATURE/ivycpp_native`
dans `https://github.com/alex31/pprzlink`. Paparazzi utilise la branche du même
nom sur `https://github.com/alex31/paparazzi`, avec ce fork comme URL du sous-module.
Les instructions reproductibles sont dans le README C++ et dans
`doc/pprzlink_cpp23.md` du dépôt Paparazzi.

Ivy **3.18.3** est publié sur `FEATURE/cpp_wrapper`, commit
`b0bf831702c5bfd1313e83ded62eb14d17198534`. Il inclut désormais IvyContext/unique_ptr.
pprzlink a été recompilé avec les headers et bibliothèques de ces paquets :
**12/12 tests réussis**. Les chemins temporaires ci-dessus documentent les
validations précédentes ; ils ne sont pas nécessaires sur une autre machine.
