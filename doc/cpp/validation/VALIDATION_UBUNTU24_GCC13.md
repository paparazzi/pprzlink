# Validation Ubuntu 24.04 / GCC 13 — 29 septembre 2026

Ce rapport décrit la validation initiale, avant l'implémentation de `link++`.
Les résultats du nouvel agent et des extensions de bibliothèque sont consignés
dans [LINK_CPP_VALIDATION.md](LINK_CPP_VALIDATION.md).

**Résultat : le code C++ actuel compile avec le GCC 13 fourni par Ubuntu 24.04,
et les 12 tests passent. Aucune correction du source C++ n'a été nécessaire.**

Cette validation porte sur PPRZLINK
`756d00d1a88f8b7ad0a2c2a8ac6e98bf6fca34e6` et Ivy 3.18.3
`b0bf831702c5bfd1313e83ded62eb14d17198534`. Les modifications de cette session
concernent uniquement la documentation, dont le
[guide de remplacement du link OCaml](../historique/LINK_CPP_PLAN.md).

## Environnement réellement utilisé

| Élément | Version / provenance |
| --- | --- |
| Système | Ubuntu 24.04.5 LTS, Noble, x86_64 |
| Compilateurs | `/usr/bin/gcc-13` et `/usr/bin/g++-13`, 13.3.0 |
| Paquets compilateur et headers C++ | `13.3.0-6ubuntu2~24.04.1` |
| Mode C++ | C++23 ; `-std=c++23` pour les bibliothèques, `-std=gnu++23` pour les exécutables CMake |
| CMake | 3.28.3 |
| pkg-config | 1.8.1 |
| Python | 3.12.3 |
| Boost | `libboost1.83-dev`, `1.83.0-2.1ubuntu3.2`, dépôt Noble updates |
| TinyXML2 | `libtinyxml2-dev` et `libtinyxml2-10`, `10.0.0+dfsg-2`, dépôt Noble |
| libstdc++ et libgcc_s utilisées | `14.2.0-4ubuntu2~24.04.1`, dépôts officiels Noble updates/security |
| Ivy | 3.18.3, cœur C et wrapper C++ recompilés avec GCC 13 dans un préfixe privé |

Le numéro du paquet **runtime** libstdc++ de Noble est distinct de celui du
compilateur et de ses en-têtes : les en-têtes utilisés sont bien ceux de GCC 13
(`_GLIBCXX_RELEASE=13`). Le contrôle des macros confirme aussi la disponibilité
de `std::format`, `std::expected` et `std::bit_cast` utilisés par ce code.

## Isolation des dépendances

La machine a aussi un compilateur GCC 16 sélectionné par la commande `g++`,
ainsi qu'une libstdc++ 16 provenant d'un PPA. Une simple compilation avec le
compilateur par défaut n'aurait donc pas validé la cible demandée.

Pour éviter cette dépendance :

1. Le compilateur est imposé avec `CMAKE_CXX_COMPILER=/usr/bin/g++-13` et
   `CXX=/usr/bin/g++-13` pour Make. Les 40 commandes CMake ont été contrôlées.
2. Boost, TinyXML2, libstdc++6 et libgcc-s1 sont téléchargés aux versions
   officielles ci-dessus, puis extraits avec `dpkg-deb -x` dans `/tmp`.
   Aucun paquet système n'a été installé, remplacé ou rétrogradé.
3. Ivy est reconstruit depuis une archive Git de la révision indiquée,
   sans les modifications locales du dépôt Ivy et sans réutiliser ses objets.
4. `CMAKE_PREFIX_PATH` et `PKG_CONFIG_PATH` désignent ces dépendances privées.
5. `-L` désigne les bibliothèques Noble lors de l'édition de liens, avec des
   liens locaux `libstdc++.so` et `libgcc_s.so` vers les runtimes extraits.
6. `LD_LIBRARY_PATH` sélectionne les mêmes runtimes pour l'exécution.
   `ldd` confirme les chemins pour l'exemple Ivy, les consommateurs externes
   et la bibliothèque produite par Make.

Il s'agit d'une validation sur le système Ubuntu 24.04 existant avec les
dépendances ciblées isolées, pas d'une installation neuve dans une VM.

## Résultats

| Vérification | Résultat |
| --- | --- |
| Ivy C et C++ : compilation et installation privée avec GCC 13 | Réussies |
| CMake Debug : bibliothèque partagée et statique | Réussies, `-Wall -Wextra -Werror` |
| Deux exemples et dix exécutables de test C++ | Compilés |
| Suite CTest, incluant le test Python du diagnostic série | **12/12 réussis** |
| Installation CMake dans un préfixe privé | Réussie |
| Consommateur externe de `pprzlink++` | Compilé et échange Ivy réussi |
| Consommateur externe de `pprzlink++_static` | Compilé et échange Ivy réussi |
| Makefile historique : bibliothèques partagée et statique | Compilées avec `-O2 -Werror` |
| Sélection effective des bibliothèques Noble et Ivy privé | Vérifiée avec `ldd` |

Le consommateur externe utilise `find_package(pprzlink++ CONFIG REQUIRED)` et
les cibles installées, sans imposer lui-même le standard C++ ni les dépendances.
Il reprend l'exemple `IvyRoundTrip.cpp` et son XML dans un autre projet CMake.
La cible statique concerne pprzlink ; les dépendances Ivy restent partagées.

Tests exécutés : `ivy_round_trip`, `serialization`, `field_value`, `ivy_link`,
`transport`, `xbee_transport`, `xbee_modem`, `xbee_baudrate`, `dictionary`,
`ivy_message_codec`, `serial_device`, `serial_messages`.

Lors du premier lancement dans le bac à sable, les dix tests sans échanges
Ivy réseau ont réussi. Les deux tests Ivy ont échoué avec
`Ivy: transport I/O failure`, puis **les deux ont réussi** après réexécution
hors du bac à sable, avec accès aux sockets locales. Les consommateurs externes
ont été lancés avec cet accès et ont tous deux réussi. Aucune modification
du code ni des tests n'a été faite pour contourner un échec.

## Reproduire sur Ubuntu 24.04 avec ses dépendances système

Installer les dépendances de compilation, si elles manquent :

```sh
sudo apt-get install gcc-13 g++-13 cmake pkg-config python3 \
  libboost-dev libtinyxml2-dev
```

Ivy 3.18.3 avec son wrapper C++ doit être construit sur la cible, avec
`CC=/usr/bin/gcc-13 CXX=/usr/bin/g++-13`. Ne pas réutiliser des binaires Ivy
construits sur Ubuntu 26. Suivre le README Ivy pour ses dépendances et son
installation, puis vérifier `pkg-config --modversion ivy-cpp ivy-c`.

Depuis `lib/v2.0/C++` :

```sh
/usr/bin/g++-13 --version
pkg-config --modversion ivy-cpp ivy-c tinyxml2

# Répertoire neuf : CMake conserve le compilateur dans son cache.
cmake -S . -B build-gcc13 \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-gcc13 -j4
ctest --test-dir build-gcc13 --output-on-failure

cmake --install build-gcc13 --prefix "$PWD/install-gcc13"

make -j4 libpprzlink++ CXX=/usr/bin/g++-13 \
  OBJ_DIR="$PWD/build-make-gcc13" CXXFLAGS='-O2 -Werror'
```

Les tests Ivy nécessitent des sockets locales UDP/TCP ; les tests série
nécessitent des pseudo-terminaux. Aucun modem physique n'est nécessaire.
Sur une machine comportant un PPA de compilateur récent, reproduire aussi
l'isolation des runtimes décrite plus haut : le seul choix de `g++-13` ne
garantit pas la provenance de `libstdc++.so.6` chargée.

## Commandes et traces de cette session

Répertoire temporaire : `/tmp/pprzlink-ubuntu24-gcc13-bJLxvG`.
Il contient `env.sh`, les paquets téléchargés, `deps`, `ivy-src`, `ivy`,
`build`, `install`, `consumer`, `make-build` et `logs`.
Ce chemin est une trace de validation, pas un prérequis de compilation.

L'environnement `env.sh` fixe les préfixes privés et les bibliothèques Noble.
Les commandes principales réellement utilisées sont :

```sh
cd /tmp/pprzlink-ubuntu24-gcc13-bJLxvG
. ./env.sh

make -C ivy-src/src -j6 cpp installliblinks includes installpkgconf install-cpp \
  CC=/usr/bin/gcc-13 CXX=/usr/bin/g++-13 CPP=/usr/bin/g++-13 \
  X86_64_CFLAGS= PREFIX="$PPRZ_IVY_PREFIX" LIB=/lib

cmake -S "$PPRZ_SOURCE/lib/v2.0/C++" -B build \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DCMAKE_PREFIX_PATH="$PPRZ_VALIDATION/deps/usr;$PPRZ_IVY_PREFIX" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j6
ctest --test-dir build --output-on-failure
# Les deux tests réseau sont relancés avec accès aux sockets :
ctest --test-dir build --rerun-failed --output-on-failure

cmake --install build --prefix "$PPRZ_VALIDATION/install"
cmake -S consumer -B consumer/build \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DCMAKE_PREFIX_PATH="$PPRZ_VALIDATION/install;$PPRZ_VALIDATION/deps/usr;$PPRZ_IVY_PREFIX"
cmake --build consumer/build -j2
ctest --test-dir consumer/build --output-on-failure

make -C "$PPRZ_SOURCE/lib/v2.0/C++" -j6 libpprzlink++ \
  CXX=/usr/bin/g++-13 OBJ_DIR="$PPRZ_VALIDATION/make-build" \
  CPPFLAGS="-I$PPRZ_VALIDATION/deps/usr/include" CXXFLAGS='-O2 -Werror'
```

Les journaux comprennent `ivy-build.log`, `configure.log`, `build.log`,
`ctest.log`, `ctest-ivy-unsandboxed.log`, `install.log`, les trois journaux
`consumer-*`, `make-build.log`, les sorties `ldd-*`, les versions ELF,
`compiler-search.txt`, `compiler-features.txt`, ainsi que les empreintes SHA-256
des sources compilées et des paquets (`source-sha256.txt`, `package-sha256.txt`).

## Portée et suite

La validation couvre le GCC 13.3 des mises à jour officielles Ubuntu 24.04
sur x86_64. Elle ne prétend pas avoir testé toutes les versions correctives
de GCC 13, ARM, du matériel XBee réel ni le futur agent `link++`.
Les sanitizers validés précédemment avec les autres compilateurs n'ont pas
été relancés ici.

Pour maintenir cette compatibilité, le guide prévoit une construction CI
Ubuntu 24.04 avec GCC 13 explicitement sélectionné, Ivy épinglé et CTest.
Cette CI reste à ajouter ; la présente vérification est une exécution locale.
