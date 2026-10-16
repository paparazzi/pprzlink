# Validation C++ et Ivy — 27 septembre 2026

Machine : Ubuntu 26.04.1 amd64 ; Clang 22.1.2 et GCC 15.2.0.
Ivy 3.18.2 : commit 933e5a0fac1f18b7fdbbfe431c6df7dd570c5498, poussé sur origin/FEATURE/cpp_wrapper.

## Résultats après correction d'Ivy

| Périmètre | Résultat |
| --- | --- |
| pprzlink, ASan + UBSan + détection des fuites | 11/11 |
| pprzlink, ThreadSanitizer | 11/11 |
| pprzlink, GCC 15 avec les headers et bibliothèques des paquets 3.18.2 extraits | 11/11 |
| Ivy, ASan/UBSan/LSan | 22 scripts réussis |
| Ivy, ThreadSanitizer | 21 scripts et le test de nettoyage des bindings réussis |
| Paquets Ivy C/C++, natif/GLib, statique/partagé | 8 programmes consommateurs compilés et exécutés |
| Transports Ivy natif, GLib et OpenMP avec GCC 15 | Réussis |

Aucune alerte sanitizer sur les tests de validation. Une course volontaire,
exécutée séparément, produit bien le rapport TSan et le code 66 attendus.
Les paquets 3.18.2 ont été extraits et testés sans installation système ;
la machine utilise encore les paquets 3.18.1 tant que la commande sudo n'est
pas exécutée.

## Défauts Ivy détectés en 3.18.1 et corrigés en 3.18.2

- Fuite de la copie de l'adresse du bus dans ivythroughput.
- Ressources de l'API historique conservées après IvyTerminate : sockets,
  abonnements, timers et contrôles en attente. Le contexte reste désormais
  valide pendant les callbacks de déconnexion ; il peut être réinitialisé
  proprement ensuite.
- Libération complète du dictionnaire des expressions régulières.
- Course entre les demandes d'écriture provenant d'un autre thread et la
  construction du tableau de poll de GLib. Le masque est désormais modifié
  dans le callback prepare de la source GLib.

La nouvelle régression de fermeture/réinitialisation échoue avec la 3.18.1
installée et passe vingt cycles avec chaque backend corrigé.
Le test GLib répète cent demandes d'écriture depuis un autre thread.

Le rapport détaillé se trouve dans :
/home/alex/DEV/LIB/IVY/libivy-c/VALIDATION_3.18.2.md.

## Correction Clang effectuée dans pprzlink

Clang rejetait la contrainte de FieldValue::getValue<T>(), qui accédait à
FieldValue avant la fin de sa définition. Le concept
detail::ReadableFieldValue<Field, Output> rend le type du récepteur dépendant
jusqu'à l'instanciation et conserve l'API et ses contraintes.
tests/FieldValueTest.cpp vérifie la compilation et les lectures par valeur.

## Instrumentation et traces

Le cœur C Ivy, son wrapper C++ et pprzlink sont instrumentés ensemble dans
deux constructions séparées : -fsanitize=address,undefined et -fsanitize=thread.
Les options activent l'arrêt sur erreur et la détection des fuites avec ASan.
GLib 2.88.0 est elle-même reconstruite avec TSan : cela rend ses GMutex visibles
et évite les faux positifs constatés avec la GLib système. Aucune suppression
sanitizer n'est utilisée. PCRE2, TinyXML2 et libstdc++ ne sont pas instrumentées.

Les journaux sont conservés dans :
/home/alex/DEV/LIB/IVY/libivy-c/build/debian/3.18.2/validation/.

Répertoires de construction temporaires :
- /tmp/ivy-3.18.2-san-7btxq0mx
- /tmp/ivy-3.18.2-gcc-gx74taq6
- /tmp/pprzlink-ivy-3.18.2-package-vsrj34n_

OpenMP est testé avec GCC, pas avec les sanitizers Clang.
Aucun test Windows, macOS, Qt, matériel XBee ou GCC 13/Ubuntu 24.04 n'est inclus.

## Propriété et durée de vie des objets C++

Ivy C++ emploie déjà shared_ptr/weak_ptr pour les états et callbacks,
unique_ptr pour les enregistrements des timers, et des destructeurs RAII pour
les ressources C. LoopThread emprunte encore son Bus : celui-ci doit rester
vivant et immobile jusqu'à la fin du thread.

Depuis la publication de la 3.18.2, le contexte IvyContext est détenu localement
par un unique_ptr avec destructeur personnalisé dans le wrapper Ivy.
Cette modification a été validée séparément ; elle n'est pas dans les paquets
3.18.2 publiés.

pprzlink utilise unique_ptr pour les messages reçus, shared_ptr pour l'état
série retenu par les callbacks Asio, et des membres par valeur pour le bus et
les abonnements. Les dictionnaires sont empruntés par référence.

Les transports PprzTransport et XbeeTransport possèdent désormais leur Device
par unique_ptr. Le constructeur reçoit la propriété par std::move et refuse un
pointeur vide. getDevice() renvoie une référence empruntée, const sur un
transport const. setDevice() est supprimé : changer de périphérique demande
de reconstruire le transport, avec des tampons et un état d'initialisation
neufs. Les transports ne sont ni copiables ni déplaçables ; déplacer leur
unique_ptr reste possible. Le contexte Asio doit survivre au transport série.
Les appels externes doivent être terminés avant sa destruction.

Les exemples, tests et instructions de migration du README ont été adaptés.
Ce changement d'API nécessite d'adapter et de recompiler les applications
qui construisent des transports. Les modifications pprzlink restent locales.

### Validation de la propriété exclusive de Device

Trois nouvelles constructions complètes (bibliothèques statique et partagée,
exemples et tests) ont été effectuées :

| Configuration | Résultat |
| --- | --- |
| GCC 15 avec les headers et bibliothèques des paquets Ivy 3.18.2 extraits | 11/11 |
| Clang 22, ASan + UBSan + détection des fuites | 11/11, aucune alerte |
| Clang 22, ThreadSanitizer | 11/11, aucune alerte |

Les constructions instrumentées utilisent les bibliothèques Ivy privées déjà
instrumentées, incluant la modification locale IvyContext/unique_ptr.
Le chargement de ces bibliothèques est vérifié avec ldd.

Les régressions vérifient le transfert de propriété, l'accès par référence,
la destruction unique à travers Transport, la libération après une exception
du constructeur et le rejet des pointeurs vides. Les tests sur pseudo-terminaux
détruisent alternativement PprzTransport et XbeeTransport avec des lectures Asio
en attente, y compris une annulation traitée après destruction du transport.
La suite vérifie aussi les trames et l'initialisation XBee, ainsi que le
programme série complet.

Scripts, résultats et journaux :
/tmp/pprz-device-owner-5c7ao0j6/.

Aucun test matériel XBee ni GCC 13/Ubuntu 24.04 n'a été effectué.

## Autobaud XBee et sauvegarde de la vitesse — 27 septembre 2026

L'initialisation détecte désormais automatiquement le débit du modem. La cible
est 57600 bauds par défaut ; l'exemple accepte une autre cible avec -s.
Le débit cible est essayé d'abord, puis les autres vitesses standard parmi
1200, 2400, 4800, 9600, 19200, 38400, 57600 et 115200, sans doublon.

Après détection et lecture de BD, un changement passe par ATBDn, ATCN avec OK
à l'ancienne vitesse, changement de la vitesse hôte, nouvelle entrée AT et
vérification BD. ATWR n'est envoyé qu'après cette vérification et seulement
si le débit a changé. Il est acquitté avant la configuration MY/CH/AP et avant
tout envoi de trame. ATWR sauvegarde aussi les autres paramètres déjà présents
dans le modem. Les réglages MY/CH/AP effectués ensuite restent temporaires.

L'interface SerialDevice exprime la capacité de changer le débit hôte.
BoostSerialPortDevice l'implémente en annulant la lecture précédente et en
écartant les octets des anciennes complétions, sous son verrou existant.
Le temps de garde avant +++ permet de vider les entrées périmées restantes.
Aucun tcflush après les commandes ni changement aux écritures normales.

| Configuration | Résultat |
| --- | --- |
| GCC 15, headers/bibliothèques du paquet Ivy 3.18.2 extrait | 12/12 |
| Clang 22, ASan + UBSan + détection des fuites | 12/12, aucune alerte |
| Clang 22, ThreadSanitizer | 12/12, aucune alerte |

Les variantes instrumentées utilisent Ivy privé instrumenté, avec la conversion
locale IvyContext/unique_ptr, comme lors de la validation précédente.
Le chargement d'Ivy est vérifié par ldd pour les trois variantes.
Les bibliothèques statique/partagée, les exemples et les tests ont été reconstruits.

Couverture ajoutée :
- Détection depuis chacune des huit vitesses standard, défaut 57600 et cible 115200.
- Vérification du débit avant sauvegarde ; absence d'ATWR si le débit est déjà correct.
- Redémarrage simulé à la vitesse sauvegardée, sans balayage ni nouvelle écriture flash.
- ERROR/absence de réponse à chaque étape BD/CN/WR, BD incorrect et scan sans modem.
- Blocage des envois pendant la négociation et après un échec d'ATWR.
- Exécutable réel sur pseudo-terminal, sans option d'activation de l'autobaud :
  contrôle des vitesses termios, de la séquence AT et des diagnostics stderr.
- Changement de débit avec réception Asio concurrente et anciennes complétions en attente.

Script, environnements, résultats, journaux et empreintes des sources :
/tmp/pprz-xbee-autobaud-n1e2i57v/.
Constructions réutilisées : /tmp/pprz-device-owner-5c7ao0j6/{gcc,asan,tsan}.

Les pseudo-terminaux ne simulent pas les contraintes électriques du UART ni la
mémoire flash physique. Aucun test matériel XBee ni GCC 13/Ubuntu 24.04 n'a été
effectué. La persistance après une vraie coupure d'alimentation reste à vérifier.

## Publication avec Ivy 3.18.3

Ivy 3.18.3 est publié sur `alex31/libivy-c`, branche `FEATURE/cpp_wrapper`,
commit [b0bf831702c5bfd1313e83ded62eb14d17198534](https://github.com/alex31/libivy-c/commit/b0bf831702c5bfd1313e83ded62eb14d17198534).
La gestion d'IvyContext par unique_ptr fait maintenant partie de cette version.

Une nouvelle compilation GCC 15 utilise directement les headers et bibliothèques
des deux paquets 3.18.3 extraits : **12/12 tests pprzlink réussis**. Les commandes de
compilation et ldd confirment l'emploi de cette même installation extraite.
Les huit consommateurs C/C++ natif/GLib, statique/partagé des paquets passent aussi.

Les validations ASan/UBSan/fuites et TSan précédentes restent valables pour le
code testé : les changements Ivy C++ publiés correspondent exactement aux sources
des constructions privées instrumentées. Cette publication ne constitue pas une
nouvelle exécution des sanitizers pour un simple changement des métadonnées de version.

Rapport et reproduction côté Ivy :
[VALIDATION_3.18.3.md](https://github.com/alex31/libivy-c/blob/b0bf831702c5bfd1313e83ded62eb14d17198534/VALIDATION_3.18.3.md).
Les instructions de compilation portables de pprzlink sont dans [référence C++](../reference.md).
