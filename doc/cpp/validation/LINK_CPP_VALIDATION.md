# link++ : implémentation et comparaison avec OCaml

Rapport du 29 septembre 2026. Le code est implémenté et les comparaisons
logicielles sont exécutées sur Ubuntu 24.04 x86_64 avec GCC 13.3.
Les essais radio physiques et la session complète serveur/GCS restent à faire.

L'itération suivante de l'API est décrite dans [API_USAGE.md](../API_USAGE.md) :
réception avec métadonnées, abonnements Ivy possédés par l'appelant, conversions
contrôlées et SDK modulaire. Ses 19 tests incluent les mêmes 17 scénarios de
comparaison décrits ici. Les résultats à 15 tests ci-dessous décrivent la
validation initiale de l'agent.

## Ce qui est livré

- Agent [`apps/link`](https://github.com/alex31/pprzlink/tree/FEATURE/reactive_reception/lib/v2.0/C%2B%2B/apps/link), construit par CMake et installé sous `bin/link++`.
- Les 24 options OCaml, leurs défauts et `-help`/`--help`. La syntaxe
  `-option=valeur`, les options répétées et les arguments positionnels sont pris
  en charge comme dans `Arg.parse`.
- Passerelle Ivy ↔ série/UDP, routage XML, suivi des avions, PING/PONG,
  LINK_REPORT, statistiques stdout, horodatage et enveloppe de redondance.
- XBee AP=1 legacy et formats 868, initialisation AT contrôlée à débit fixe,
  statuts, retransmissions bornées et gestion des identifiants en attente.
- Métadonnées XML, formats numériques Ivy, champs 64 bits, décodeur PPRZ sans
  I/O, statistiques et adaptateur de fichiers/FIFO dans la bibliothèque.
- Intégration dans `sw/ground_segment/tmtc/Makefile` et `link_cpp.mk` du dépôt
  Paparazzi parent ; workflow Ubuntu 24.04/GCC 13 dans le sous-module.

Les responsabilités sont séparées en composants nommés : `Options`,
`AircraftRegistry`, `IvyBridge`, `XbeeTransmitter`, `LinkAgent`.
Le transport UDP est désormais fourni par `pprzlink::UdpTransport` dans la bibliothèque.
Le routage et la politique des avions restent dans l'application. La
bibliothèque fournit les données et les mécanismes réutilisables.

## Référence indépendante

La comparaison utilise le vrai `link.ml` de Paparazzi
`55cdd96c46d49d15ddad9814ed24ecc9a4fda914`, recompilé avec OCaml 4.14.1.
`link.ml`, `pprzLink.ml`, les transports OCaml, les temporisations GLib et le
pilote série C de Paparazzi ne sont pas remplacés par des simulations.

Les bindings Ivy OCaml locaux sont anciens : deux fonctions de conversion
Marshal de `ivy.ml` sont migrées mécaniquement de `String.create` vers `Bytes`
pour compiler avec OCaml 4.14. Ces fonctions ne servent pas au link. Le script
de construction conserve les empreintes SHA-256 de ses sources d'entrée.
Les répertoires de construction sont privés ; les anciens binaires Paparazzi
ne sont pas écrasés.

Le banc Python encode les trames indépendamment de la bibliothèque C++.
Chaque scénario lance successivement un processus OCaml et un processus C++,
avec un véritable pair Ivy, un pseudo-terminal ou une socket UDP.
Il compare les valeurs de retour des scénarios et vérifie les octets émis.

## Scénarios comparatifs

Le banc contient **17 scénarios**, dont plusieurs exécutent plusieurs variantes :

| Scénario | Contrôle |
| --- | --- |
| CLI | Ensemble des 26 noms incluant l'aide, codes de sortie, argument absent/invalide, syntaxe `=`. |
| XML de production | Chargement du vrai `messages.xml`, réception PONG et émission SETTING. |
| Série PPRZ | Débit 9600, avion inconnu, réception fragmentée, commande ciblée, diffusion, message non routé, checksum erroné et compteurs. |
| Tableaux et chaînes | Tableau signé, chaîne avec espaces, char fixe, précision flottante/double issue du XML. |
| Entiers | Limites int64/uint64, tableau 64 bits, destination hexadécimale et espaces Ivy. |
| Reprise binaire | Message inconnu et contenu tronqué suivis d'une trame valide. |
| Commande invalide | Reprise C++ après un champ non numérique ; anomalie OCaml consignée ci-dessous. |
| Descripteur de fichier | Chemin hors `/dev`, réception et émission sans configuration termios. |
| FIFO | Réception depuis une vraie FIFO nommée. |
| Options de flux | Ordre de `-uplink`/`-nouplink`, `-noac_info`, RTS/CTS et débit 57600. |
| Expiration | Timeout positif, nul et négatif, puis reprise après une nouvelle télémétrie. |
| PING/PONG | Trame PING, réponse PONG, volume RX et délai de réponse borné. |
| Redondance | `-redlink`, `-id=0x7`, horodatage, enveloppe TELEMETRY_MESSAGE et rapport ground. |
| UDP | Deux adresses avion, port montant distinct, unicast et diffusion aux deux avions. |
| Diffusion UDP | SO_BROADCAST et adresse `-udp_broadcast_addr`. |
| XBee legacy | Dialogue AT, RX16, format des simulateurs OCaml, TX16, ACK négatif, réémission identique puis ACK positif. |
| XBee 868 | RX 0x90, adresse/réserves TX, statut 0x8b et réémission ; correction du type TX consignée. |

Les temps réels, débits instantanés dépendant de l'ordonnancement, ordre
des rapports de plusieurs avions et numéros de ports ne sont pas comparés
octet par octet. Le banc vérifie les unités, les bornes et les champs stables.
Les règles de comptage, arrondis des rapports et bornes d'expiration sont
également vérifiées avec des temps injectés dans les tests de politique.

## Différences intentionnelles

La compatibilité est fonctionnelle sur les scénarios ci-dessus. Elle ne
signifie pas que tous les défauts de l'ancienne implémentation sont reproduits.

1. **`-xbee_retries`** : OCaml relie l'option à `XB.my_addr`. C++ la relie au
   nombre maximal de tentatives, premier envoi compris. Le défaut reste 10.
   Le test de politique vérifie la limite, les statuts dupliqués et la
   saturation des 255 identifiants ; aucun identifiant en attente n'est réutilisé.
2. **Trame TX 868** : OCaml ajoute les champs 868 mais écrit le type `0x00`.
   C++ écrit `0x10`. La différence est explicitement attendue par le banc,
   qui compare aussi le reste de l'enveloppe et la charge PPRZLINK. Le format
   de référence est le [manuel Digi XBee/XBee-PRO 868, pages 54–55 et 61–62](https://ftp1.digi.com/support/documentation/90001020_F.pdf).
3. **Commande Ivy malformée** : la référence reconstruite peut interrompre
   le traitement ou sortir après une exception du callback. C++ rejette la
   commande sur stderr et continue. Le banc observe cette différence et exige
   la reprise de C++.
4. **Initialisation XBee** : C++ attend les réponses AT et détecte les délais
   dépassés. Le lancement compatible reste à débit fixe : aucun ATBD/ATWR,
   même si le diagnostic `serial_messages` conserve son autobaud.
   En mode XBee, `-hfc` configure et vérifie ensemble le TTY et `D6`/`D7` ;
   sans ce commutateur, les deux côtés ont le contrôle de flux désactivé.
5. **Statut TX absent** : un envoi expire après cinq secondes, sans réémission
   automatique d'une commande dont la livraison est inconnue. Les retries sur
   erreur d'ACK restent bornés. Une trame ne confirme pas l'exécution de la
   commande par l'avion.
   L'extension XML `radio_retry="false"` désactive les ACK/réessais radio et
   les réémissions côté PC. Une valeur `true` ou absente conserve la politique
   de tentatives bornées par `-xbee_retries`, premier envoi compris.
6. **Entrées incorrectes** : C++ refuse les ports/adresses hors plage, périodes
   invalides (`status_period < 3 ms`, `ping_period <= 0`), valeurs numériques
   hors type et combinaison UDP/XBee incohérente. Les textes d'erreur et de
   l'aide ne cherchent pas à reproduire les fautes de frappe d'OCaml.
7. **UDP** : un datagramme incomplet est écarté à sa frontière. Il n'est jamais
   concaténé au datagramme suivant, potentiellement envoyé par un autre avion.
8. **Temps** : les échéances et l'horodatage relatif utilisent une horloge
   monotone. Les sorties historiques qui demandent l'heure civile la conservent.

Les choix historiques de LINK_REPORT restent conservés : compteur PPRZ global
pour `rx_err` (zéro en mode XBee), diffusions comptées pour les avions connus,
et différence dernier PONG/dernier PING pouvant devenir négative entre réponses.
Les statistiques détaillées de la bibliothèque restent disponibles séparément.

Le test C++ `link_xbee_configuration` vérifie séparément cette extension XML
et le contrôle de flux couplé, avec un pair Ivy et un modem simulé sur PTY.
Il couvre les attributs absent/true/false, les modes legacy/868, les commandes
AT et le réglage Linux `CRTSCTS`, la limite de tentatives et la poursuite des
commandes après un échec final. Les scénarios comparatifs historiques gardent
leur XML sans cet attribut.

## Validation de compilation et d'exécution

Environnement GCC 13 et dépendances Noble :
[rapport de préparation](VALIDATION_UBUNTU24_GCC13.md).
La bibliothèque et l'agent sont construits sans avertissement de compilation
avec `-Wall -Wextra -Werror`.

| Vérification | Résultat |
| --- | --- |
| GCC 13.3, CMake Debug, bibliothèque statique/partagée et agent | Réussi |
| CTest GCC 13 avec référence OCaml | 15/15 tests réussis |
| Construction Release de l'agent via `link_cpp.mk` | Réussie |
| Makefile historique de bibliothèque, GCC 13, `-O2 -Werror` | Bibliothèques statique/partagée construites |
| Installation CMake et consommateurs externes statique/partagé | Réussis |
| ASan + UBSan + détection des fuites, GCC 13 | 15/15 réussis, sans diagnostic |
| GCC 16.1 avec Ivy C++ reconstruit par GCC 16 | 15/15 réussis |

ASan/UBSan instrumentent le code pprzlink, l'agent, les exemples et les tests.
Ivy et les dépendances système ne sont pas réinstrumentés dans cette session.
La CI est ajoutée mais n'a pas été exécutée sur GitHub depuis cette session.

Le test GCC 16 a d'abord révélé deux problèmes de mélange d'installations :
un RPATH référençait encore la libstdc++ Noble prévue pour GCC 13, puis le
wrapper Ivy compilé par GCC 13 provoquait des échecs dans les appels C++.
Avec son runtime GCC 16 et Ivy C++ recompilé par GCC 16, les 15 tests passent.
Pour ces deux configurations validées, conserver une chaîne cohérente :
compilateur, runtime, wrapper Ivy et pprzlink. Le seul numéro de version Ivy
ne garantit pas la compatibilité de ses binaires C++ entre compilateurs.

L'exécutable installé possède un chemin relatif vers la bibliothèque pprzlink
du même préfixe. Les dépendances externes installées dans un autre préfixe,
notamment Ivy, restent à déclarer au chargeur comme dans le README.

## Reproduire la comparaison

Après installation d'OCaml 4.14, de `libxml-light-ocaml-dev`,
`liblablgtk3-ocaml-dev`, de leurs dépendances et d'Ivy C/GLib/C++ 3.18.3 :

```sh
# Depuis lib/v2.0/C++ ; IVY_PREFIX et PAPARAZZI_SRC désignent les installations.
python3 pprzlink/tests/build_ocaml_reference.py \
  --paparazzi "$PAPARAZZI_SRC" \
  --ivy-ocaml /path/to/ivy-ocaml-sources \
  --ivy-prefix "$IVY_PREFIX" \
  --output /tmp/link-ocaml-reference

export PKG_CONFIG_PATH="$IVY_PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$IVY_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cmake -S . -B build-gcc13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPPRZLINK_OCAML_REFERENCE=/tmp/link-ocaml-reference/link_ocaml
cmake --build build-gcc13 -j4
ctest --test-dir build-gcc13 --output-on-failure

python3 pprzlink/tests/LinkParityTest.py \
  --cpp build-gcc13/apps/link/link++ \
  --ocaml /tmp/link-ocaml-reference/link_ocaml \
  --bus build-gcc13/link_test_bus --output /tmp/link-comparison.json
```

`--ocaml-packages` permet au constructeur de référence d'utiliser des paquets
OCaml extraits dans un préfixe privé. `--only` sélectionne un scénario du banc.
Sans `--ocaml`, les mêmes scénarios vérifient les résultats attendus du C++.
Les tests nécessitent des sockets locales UDP/TCP et des pseudo-terminaux.

Les constructions et traces locales sont dans :

- `/tmp/pprzlink-ubuntu24-gcc13-bJLxvG/build` : CMake GCC 13 ; dépendances privées
  dans `deps` et `ivy`, consommateurs installés dans `consumer`.
- `/tmp/pprzlink-link-parity/ocaml` : référence et empreintes des sources.
- `/tmp/pprzlink-link-parity/paparazzi-build` : construction via Make en Release.
- `/tmp/pprzlink-link-parity/asan` : construction instrumentée.
- `/tmp/pprzlink-link-parity/gcc16` et `ivy-gcc16` : contrôle avec le compilateur récent.
- `/tmp/pprzlink-link-parity/*-ctest*.log` et `parity-*.log` : résultats.

## Construire et choisir l'implémentation dans Paparazzi

Les dépendances C++ doivent être installées ou décrites par les variables de
préfixe habituelles. Depuis la racine Paparazzi :

```sh
export PAPARAZZI_SRC="$PWD"
export PAPARAZZI_HOME="$PWD"

# Construire le nouvel exécutable à côté du link existant.
make -C sw/ground_segment/tmtc link++

# Sélectionner C++ au chemin historique utilisé par les sessions.
make -C sw/ground_segment/tmtc LINK_IMPL=cpp link

# Revenir à OCaml (avec ses dépendances de construction installées).
make -C sw/ground_segment/tmtc LINK_IMPL=ocaml link
```

`PPRZLINK_CXX` vaut `g++-13` par défaut. `PPRZLINK_CPP_BUILD`,
`PPRZLINK_BUILD_JOBS` et `PPRZLINK_CMAKE_OPTIONS` permettent de configurer la
construction. L'intégration a été testée dans un répertoire privé et les deux
cibles C++ ont été vérifiées avec `make -n` dans Paparazzi ; le binaire OCaml
présent au chemin historique n'a pas été remplacé.

## Limites encore à valider

- Modems physiques : débit UART, paramètres propres au firmware, livraison RF,
  coupures d'alimentation et comportement 868. Les formats AP=1 sont testés,
  pas la configuration de tous les produits Digi.
- Session complète serveur/GCS et link combiner en fonctionnement. Les messages
  et enveloppes attendus sont comparés au link OCaml ; cela ne remplace pas
  l'exécution de ces applications avec un avion.
- Les autres architectures et systèmes, AP=2, et la gestion réseau Zigbee/DigiMesh
  ne font pas partie de cette validation.

La bibliothèque publique a changé d'ABI : recompiler ses consommateurs lors
du passage à cette version.
