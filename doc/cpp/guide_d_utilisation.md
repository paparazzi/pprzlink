# Apprendre à utiliser pprzlink++ pas à pas

Ce guide vous accompagne dans l'écriture d'une petite application C++ qui
manipule des messages PPRZLINK, puis les échange avec une autre application.
Vous commencerez sans réseau ni matériel. Vous ajouterez ensuite UDP, une
liaison série, le bus Ivy et, enfin, une radio XBee.

Il suppose que vous savez compiler un programme C++ et connaissez les
variables, les fonctions et les classes. Les notions particulières à la
bibliothèque sont introduites au moment où elles deviennent utiles.

Les exemples suivent **l'API C++ présente dans cette branche**, dans
`lib/v2.0/C++`, avec le protocole PPRZLINK v2. Ils utilisent un petit catalogue
XML propre au guide. Ce catalogue sert à expérimenter entre vos programmes ;
pour communiquer avec Paparazzi, vous utiliserez ensuite les définitions de
votre configuration réelle.

Le parcours est le suivant :

1. [Préparer un projet](#1-préparer-un-projet).
2. [Créer et lire un premier message](#2-créer-et-lire-un-premier-message).
3. [Remplir plusieurs champs et choisir les bons types](#3-remplir-plusieurs-champs-et-choisir-les-bons-types).
   Les [conversions SI explicites](#convertir-explicitement-les-unités) permettent aussi de travailler en mètres, radians et kelvins.
4. [Manipuler des tableaux et du texte](#4-manipuler-des-tableaux-et-du-texte).
5. [Encoder puis décoder sans matériel](#5-encoder-puis-décoder-sans-matériel).
6. [Échanger des messages en UDP](#6-échanger-des-messages-en-udp).
7. [Passer à une liaison série](#7-passer-à-une-liaison-série).
8. [Recevoir et publier sur Ivy](#8-recevoir-et-publier-sur-ivy).
9. [Faire une requête et attendre sa réponse](#9-faire-une-requête-et-attendre-sa-réponse).
10. [Utiliser une radio XBee](#10-utiliser-une-radio-xbee).
    L'exemple utilise un timer d'émission et présente [RTS/CTS](#activer-rtscts-sur-le-port-et-la-radio-ensemble),
    [la lecture de CTS](#lire-létat-cts-local), les [réessais par envoi](#choisir-les-réessais-radio-pour-chaque-envoi)
    et les [adresses radio typées](#choisir-la-destination-radio).
    Vous pourrez ensuite [configurer link++ avec le XML et RTS/CTS](#utiliser-link-avec-une-radio-xbee).
11. [Construire un outil générique ou un adaptateur](#11-construire-un-outil-générique-ou-un-adaptateur).
12. [Organiser une application durable et diagnostiquer les erreurs](#12-organiser-une-application-durable-et-diagnostiquer-les-erreurs).

## 1. Préparer un projet

### Construire la bibliothèque

La bibliothèque demande C++23, CMake 3.20 ou plus, TinyXML2 et Boost.
Sous Ubuntu 26.04, utilisez le compilateur natif GCC 15 avec Boost 1.90 et
TinyXML2 11. Les exigences minimales des sources restent GCC 13 et Boost 1.83 ;
les anciennes validations Ubuntu 24.04 sont conservées dans la référence.
Les conversions SI utilisent une table interne. Les dépendances du début de
ce guide s'installent avec :

```sh
# Installe le compilateur C++23, les outils de construction et les dépendances de base.
sudo apt install git g++ cmake pkg-config libtinyxml2-dev libboost-dev
```

Après une mise à niveau du système ou du compilateur, repartez d'un répertoire
de construction neuf et reconstruisez aussi vos applications. Les anciens caches
CMake peuvent conserver le compilateur et les versions de dépendances précédents.
Le répertoire d'atelier ci-dessous permet une construction distincte sous Ubuntu 26.04.

Ivy sera ajouté à l'étape 8. Commencez avec les commandes suivantes **à la
racine du dépôt pprzlink** :

```sh
# Repère les sources C++ et choisit le répertoire de travail de l'atelier.
export PPRZLINK_CPP="$PWD/lib/v2.0/C++"
export PPRZLINK_ATELIER=/tmp/pprzlink-guide-ubuntu26

# Configure une bibliothèque de débogage, sans Ivy, agent link++ ni tests.
cmake -S "$PPRZLINK_CPP" -B "$PPRZLINK_ATELIER/build-lib" \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPPRZLINK_WITH_IVY=OFF \
  -DPPRZLINK_BUILD_LINK=OFF \
  -DBUILD_TESTING=OFF
# Compile la bibliothèque et les exemples activés avec le parallélisme de l'environnement.
cmake --build "$PPRZLINK_ATELIER/build-lib"
# Installe les en-têtes, bibliothèques et cibles CMake dans le SDK de l'atelier.
cmake --install "$PPRZLINK_ATELIER/build-lib" \
  --prefix "$PPRZLINK_ATELIER/sdk"

# Crée le projet utilisateur, puis s'y place pour les étapes suivantes.
mkdir -p "$PPRZLINK_ATELIER/projet"
cd "$PPRZLINK_ATELIER/projet"
```

Le répertoire `sdk` contient les en-têtes, bibliothèques et fichiers CMake
installés. Les conversions d'unités sont compilées directement dans la
bibliothèque ; aucune installation supplémentaire n'est nécessaire.
Le répertoire `projet` accueillera votre programme. `/tmp` convient à cet
atelier ; choisissez un autre emplacement si vous souhaitez le conserver.

**À partir de maintenant, les commandes sont exécutées dans `projet`.**
Gardez ce terminal ouvert pour conserver les deux variables d'environnement.

### Décrire les messages de l'atelier

Créez `messages.xml` dans `projet` :

```xml
<?xml version="1.0"?>
<protocol>
  <!-- Les messages de l'atelier partagent la classe guide, d'identifiant binaire 1. -->
  <msg_class name="guide" id="1">
    <!-- Une altitude exprimée en mètres et stockée dans un float. -->
    <message name="GUIDE_ALTITUDE" id="1" link="broadcasted">
      <field name="altitude" type="float" unit="m"/>
    </message>
    <!-- Une commande : avion visé dans ac_id et valeur du réglage dans value. -->
    <message name="GUIDE_SETTING" id="2" link="forwarded">
      <field name="ac_id" type="uint8"/>
      <field name="value" type="float"/>
    </message>
    <!-- Tableaux fixe et variable, étiquette de quatre caractères et texte libre. -->
    <message name="GUIDE_SAMPLES" id="3">
      <field name="axes" type="float[3]"/>
      <field name="samples" type="uint16[]"/>
      <field name="label" type="char[4]"/>
      <field name="text" type="string"/>
    </message>
    <!-- Requête sans champs ; Ivy l'associera à la réponse GUIDE_ALTITUDE. -->
    <message name="GUIDE_ALTITUDE_REQ" id="4"/>
  </msg_class>
</protocol>
```

Pour le moment, regardez seulement `GUIDE_ALTITUDE` : il contient un champ
appelé `altitude`, de type `float`. Les autres messages serviront plus tard.
Les attributs `link` serviront au choix unicast/broadcast de l'atelier XBee.
Ces métadonnées ne sont pas ajoutées à la charge utile PPRZLINK.

Le XML est chargé **à l'exécution**. Il n'est pas nécessaire de générer une
classe C++ par message. Un objet `Message` pourra représenter n'importe
laquelle de ces définitions.

Les identifiants de classe et de message permettent au décodeur binaire de
retrouver la définition. Les deux correspondants doivent donc utiliser des
définitions compatibles, avec le même ordre et les mêmes types de champs.
La trame ne transporte pas le XML.

### Créer le projet CMake

Créez `CMakeLists.txt` à côté du XML :

```cmake
cmake_minimum_required(VERSION 3.20)
project(atelier_pprzlink LANGUAGES CXX)

# Cherche le SDK installé et ses fonctions de manipulation et d'encodage des messages.
find_package(pprzlink++ CONFIG REQUIRED COMPONENTS core)

# Crée le programme ; la cible core transmet ses dépendances et le standard C++23.
add_executable(atelier main.cpp)
target_link_libraries(atelier PRIVATE pprzlink::core)
```

Le composant `core` suffit pour manipuler des messages et leurs encodages.
La cible CMake transmet au programme les chemins d'en-têtes, les dépendances
et le standard C++23.

### Actualiser le SDK après une évolution de l'API

Le projet utilise les fichiers **installés dans le SDK**, qui peuvent être
plus anciens que les sources. Après une modification de la bibliothèque,
recompilez-la et réinstallez ensemble ses en-têtes et ses bibliothèques :

```sh
cmake --build "$PPRZLINK_ATELIER/build-lib"
cmake --install "$PPRZLINK_ATELIER/build-lib" \
  --prefix "$PPRZLINK_ATELIER/sdk"

# Indique explicitement le paquet si un ancien SDK est encore dans le cache CMake.
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="$PPRZLINK_ATELIER/sdk" \
  -Dpprzlink++_DIR="$PPRZLINK_ATELIER/sdk/share/pprzlink++/cmake"
cmake --build build
```

Une erreur telle que « `XbeeConfiguration` has no member named
`hardwareFlowControl` » signifie que le compilateur ne voit pas l'en-tête
attendu. Vérifiez d'abord que les sources contiennent cette API, puis le SDK
effectivement retenu :

```sh
cmake -LA -N build
```

La variable `pprzlink++_DIR` doit désigner le répertoire `share/pprzlink++/cmake`
du SDK voulu. Changer seulement `CMAKE_PREFIX_PATH` ne remplace pas toujours
un paquet déjà mémorisé. De même, des en-têtes récents liés à des archives
anciennes peuvent provoquer des références indéfinies : les deux doivent
provenir de la même construction de la bibliothèque.

## 2. Créer et lire un premier message

Créez maintenant `main.cpp` :

```cpp
#include <pprzlink/MessageDictionary.h>
#include <pprzlink/Message.h>
#include <exception>
#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Usage: atelier messages.xml\n";
        return 2;
    }

    try {
        // Charge les schémas XML ; les transports créés ensuite emprunteront ce dictionnaire.
        const pprzlink::MessageDictionary dictionary(argv[1]);
        // Une définition décrit les champs ; elle ne contient aucune valeur de mesure.
        const auto &definition = dictionary.getDefinition("GUIDE_ALTITUDE");
        // Crée un message dont les champs restent sans valeur jusqu'à leur écriture.
        pprzlink::Message altitude(definition);

        altitude.setField("altitude", 123.5f);
        // Demande le type exact du XML : ici float, sans conversion d'unité.
        const float metres = altitude.getField<float>("altitude");

        std::cout << "Altitude : " << metres << " m\n";
        std::cout << altitude.toString() << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Erreur : " << error.what() << '\n';
        return 1;
    }
}
```

Compilez et lancez :

```sh
# Configure le projet avec le compilateur système et indique où trouver le SDK installé.
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_PREFIX_PATH="$PPRZLINK_ATELIER/sdk"
# Compile le programme, puis lui fournit le catalogue XML chargé à l'exécution.
cmake --build build
./build/atelier messages.xml
```

La première ligne affiche :

```text
Altitude : 123.5 m
```

La seconde affiche le nom du message et la valeur de ses champs. C'est un
affichage de diagnostic, utile pendant le développement.

Quatre opérations viennent d'avoir lieu :

1. `MessageDictionary` a lu le fichier XML et possède les définitions.
2. `getDefinition()` a retrouvé la structure de `GUIDE_ALTITUDE`.
3. `Message` a créé un exemplaire de ce message, initialement sans valeur.
4. `setField()` a renseigné le champ, puis `getField<float>()` l'a relu.

La **définition** décrit ce qu'un message contient. Le **message** porte les
valeurs d'un exemplaire particulier. Vous pouvez créer dix messages à partir
de la même définition, chacun avec une altitude différente.

Un champ n'est pas automatiquement initialisé à zéro. Si vous lisez
`altitude` avant `setField()`, la bibliothèque lève `field_has_no_value`.
Si vous écrivez `"altitdue"`, elle lève `no_such_field` : les noms sont ceux
du XML, vérifiés à l'exécution.

**À essayer :** remplacez `123.5f` par `250.0f`, recompilez et relancez.
Ajoutez ensuite un second `setField("altitude", 300.0f)` avant la lecture :
il remplace la valeur précédente.

## 3. Remplir plusieurs champs et choisir les bons types

Les petits extraits des étapes 3 à 5 s'ajoutent dans le bloc `try` du programme
précédent, avant `return 0`. Ajoutez les en-têtes indiqués en tête de fichier.

### Préparer une commande complète

Nous voulons maintenant préparer un réglage pour l'avion 42. Ajoutez :

```cpp
pprzlink::Message command(dictionary.getDefinition("GUIDE_SETTING"));
// Ces trois identifiants appartiennent à l'en-tête, pas aux champs du XML.
command.setSenderId(0);
command.setReceiverId(42);
command.setComponentId(0);

command.setField("ac_id", 42);
command.setField("value", 12.5);

std::cout << command.toString() << '\n';
```

Les trois premiers appels renseignent **l'en-tête PPRZLINK**. Les deux suivants
renseignent **les champs du XML**. Dans cet atelier, nous choisissons 0 pour
l'émetteur sol et 42 pour l'avion. Ces appels n'établissent aucune connexion.

En particulier, `setReceiverId(42)` ne remplit pas `ac_id`.
Inversement, `setField("ac_id", 42)` ne modifie pas le destinataire de l'en-tête.
Certaines applications ont besoin de ces deux informations, éventuellement
avec des valeurs différentes.

Vous pouvez remplacer les deux écritures de champs par un seul appel :

```cpp
// Les arguments vont par couples nom/valeur, traités dans cet ordre.
command.setField("ac_id", 42, "value", 12.5);
```

Les arguments vont par couples **nom, valeur**. Cet appel remplit le message
entier, mais ne l'envoie pas encore. Plus tard, un seul `sendMessage(command)`
transmettra l'ensemble de ses champs.

`setField()` renvoie une référence constante au message, ce qui autorise
aussi `transport.sendMessage(command.setField(...))`. Conservez pour commencer
deux instructions distinctes : le remplissage, puis l'envoi. Le chaînage
`command.setField(...).setField(...)` ne convient pas, car le premier résultat
est constant. L'ancienne méthode `addField()` remplit également un champ ;
les exemples de ce guide utilisent `setField()`.

### Comprendre les conversions

Le XML impose le type stocké. Dans `GUIDE_SETTING`, `ac_id` est un `uint8`,
même si le littéral C++ `42` est un `int`. De même, `12.5` est converti en
`float` à l'écriture de `value`.

Ajoutez l'en-tête `<cstdint>`, puis essayez :

```cpp
// Lecture native : le type demandé doit correspondre exactement au XML.
const auto aircraft = command.getField<std::uint8_t>("ac_id");
const float exactValue = command.getField<float>("value");
// Conversion numérique contrôlée de float vers double, sans changer l'unité.
const double convertedValue = command.getFieldAs<double>("value");

// uint8_t peut être un caractère pour cout ; le cast impose un affichage numérique.
std::cout << "Avion " << static_cast<unsigned>(aircraft)
          << " : " << exactValue << " / " << convertedValue << '\n';
```

`getField<T>()` demande le type exact. `getFieldAs<T>()` demande explicitement
une conversion numérique contrôlée. Ainsi, `getField<double>("value")` lèverait
`field_type_mismatch`, puisque ce champ est un `float`.
Le `static_cast<unsigned>` sert seulement à afficher `uint8_t` comme un nombre
avec `std::cout`, au lieu de l'interpréter comme un caractère.

Voici les correspondances utiles au début :

| Type du XML | Lecture C++ exacte |
| --- | --- |
| `uint8`, `uint16`, `uint32`, `uint64` | `std::uint8_t`, `std::uint16_t`, `std::uint32_t`, `std::uint64_t` |
| `int8`, `int16`, `int32`, `int64` | `std::int8_t`, `std::int16_t`, `std::int32_t`, `std::int64_t` |
| `float`, `double` | `float`, `double` |
| `char` | `char` |
| `string` | `std::string` |

Les écritures `setField("ac_id", 300)` et `setField("ac_id", 42.5)` sont
refusées : la première déborde d'un octet, la seconde perdrait une partie
fractionnaire. Les conversions vers un flottant peuvent en revanche arrondir.
`getFieldAs()` ne convertit pas les tableaux et ne transforme pas un texte
comme `"12.5"` en nombre.

### Convertir explicitement les unités

`getField()` et `setField()` manipulent les valeurs dans les unités du XML.
`getFieldAs<double>()` change leur type numérique, pas leur unité. Pour fournir
ou récupérer une grandeur SI, appelez les nouvelles méthodes :

```cpp
// Fournit des mètres ; le setter convertit vers l'unité et le type du champ XML.
altitude.setFieldSI("altitude", 123.5);
// Restitue en double la valeur SI correspondant à la valeur réellement stockée.
double altitude_si_m = altitude.getFieldSI("altitude");
std::cout << altitude_si_m << " m\n";
```

Ces appels utilisent un `double` pour la valeur SI. L'unité SI est toujours
cohérente : mètres, secondes, mètres/seconde, radians pour les angles (latitude
et longitude incluses), kelvins pour les températures absolues. Les pourcentages
sont des rapports sans unité : une entrée SI de `0.5` représente `50 %`.
Le setter choisit le type de stockage déclaré par le champ XML.

Les métadonnées sont consultables avant même de remplir un message :

```cpp
// Emprunte les métadonnées du champ ; cette référence dépend de la durée de vie du message.
const auto& info = altitude.getFieldDefinition("altitude");
std::cout << "XML : " << info.getUnit() << "; alternative : " << info.getAltUnit() << '\n';
// Le coefficient est facultatif : l'optional n'est lu que s'il contient une valeur.
if (auto coefficient = info.getAltUnitCoef()) {
    std::cout << "Coefficient XML explicite : " << *coefficient << '\n';
}
std::cout << "SI : " << info.getSIUnit()
          << "; conversion disponible : " << std::boolalpha << info.canConvertSI() << '\n';
```

La définition peut aussi être récupérée par indice. Les métadonnées restent
empruntées au message ; sa destruction ou son remplacement invalide leurs références.

Pour essayer une conversion sans modifier de message, ajoutez `<cstdint>` et :

```cpp
const pprzlink::MessageField distance("distance", "int32", {}, "mm");
// Convertit les mètres en int32 millimètres, avec arrondi au plus proche : 1235 mm.
int32_t millimetres = distance.fromSI<int32_t>(1.23456);
// Reconversion de l'entier stocké : l'arrondi précédent donne maintenant 1.235 m.
double distance_si_m = distance.toSI(millimetres);
std::cout << millimetres << " mm = " << distance_si_m << " m\n";
```

`fromSI<T>()` exige le type numérique exact du XML, comme `getField<T>()`.
Les destinations entières sont toujours arrondies à l'entier le plus proche,
avec les demi-valeurs en s'éloignant de zéro. Le getter SI restitue la valeur
réellement stockée après cet arrondi. Une valeur hors plage lève
`field_conversion_error`, sans saturation. Une écriture qui échoue conserve
la valeur précédente. Il n'y a pas de mode strict à sélectionner.

Les flottants peuvent arrondir vers `float` et conserver des NaN de mesure.
Les écritures vers des entiers refusent les valeurs non finies. Le `double` SI
ne permet pas une lecture exacte de tous les grands entiers 64 bits ; utilisez
le getter natif lorsque l'exactitude de ces entiers est nécessaire.

Pour un tableau dont tous les éléments portent la même unité, l'entrée est un
`std::span<const double>` (également construit depuis `std::array` ou `std::vector`)
et `getFieldArraySI()` renvoie directement un `std::vector<double>` :

```cpp
// Avec un champ XML : <field name="position" type="int16[3]" unit="cm"/>
std::array<double, 3> position_m{1.0, -2.0, 3.0};
// L'array fournit un span pour l'appel ; le message possède les int16 convertis en cm.
// Les valeurs stockées sont 100, -200, 300, sans conserver de vue sur position_m.
message.setFieldSI("position", position_m);
// Crée un vector<double> indépendant contenant 1.0, -2.0, 3.0 m.
const auto position_si = message.getFieldArraySI("position");
```

Ajoutez `<array>` pour cet extrait. Le vecteur retourné prend la taille du
tableau stocké dans le message : vous n'avez pas à répéter le type ni la
longueur du XML. Les lectures acceptent aussi un indice XML. Pour un scalaire,
`getFieldSI("altitude")` renvoie un `double`.

Les surcharges avec paramètre de sortie restent disponibles :

```cpp
std::vector<double> output;
// Remplace le vecteur de sortie par les valeurs SI ; sa taille suit celle du champ.
message.getFieldSI("position", output);
```

Ajoutez `<vector>` pour cet extrait. Si un élément échoue, ni le champ entier
ni le tableau de sortie d'une lecture ne sont remplacés. Le champ
`axes` du catalogue initial n'a pas d'unité ; ajoutez une unité correspondant
à vos mesures dans le XML avant d'utiliser une lecture SI.

L'exemple autonome [SIUnits.cpp](../../lib/v2.0/C++/pprzlink/examples/clients/SIUnits.cpp) fournit
ce XML et vérifie aussi l'encodage puis le décodage natif. Il se lance sans
matériel ni réseau :

```sh
# Lance l'exemple autonome de conversions SI, sans réseau ni matériel.
"$PPRZLINK_ATELIER/build-lib/si_units"
```

Vous devez notamment voir `1235 mm = 1.235 m` et `20 Celsius = 293.15 K`.

### Modifier les conventions Paparazzi

La table de conversion est dans [UnitAliases.cpp](../../lib/v2.0/C++/pprzlink/UnitAliases.cpp).
Une règle décrit directement la pente et le décalage vers l'unité SI :

```cpp
// Chaque règle donne le nom XML, le symbole SI, la pente et éventuellement l'offset.
{"1e7deg", "rad", std::numbers::pi / (180.0 * 1e7)},
{"deg_celsius", "K", 1.0, 273.15},
// Les deux derniers paramètres limitent cette interprétation de C au champ ESC.temperature.
{"C", "K", 1.0, 273.15, "ESC", "temperature"},
{"adc", ""}, // Libellé reconnu ; conversion SI interdite sans calibration.
```

La conversion est `SI = nombre_XML * multiplicateur + offset`. Ajoutez une
règle, puis recompilez pprzlink. Les sélecteurs message/champ sont facultatifs ;
la règle la plus spécifique gagne, puis la première en cas d'égalité.
Les conversions préparées sont immuables et partagées entre copies de définitions.

Les préfixes SI usuels permettent d'anticiper des unités comme `km`, `µA`, `MHz`
ou `mm/s^2` à partir des bases enregistrées. Les surfaces et volumes utilisent
le facteur au carré/cube (`cm²`, `mm3`). Le résolveur accepte un seul préfixe et
les formes composées connues ; il ne devine pas une expression algébrique ni
un nouvel encodage à virgule fixe.

Si le XML fournit `alt_unit` et `alt_unit_coef`, ce coefficient explicite
multiplie la pente de la règle alternative une seule fois ; l'offset n'est
pas multiplié. Cette description suffit pour certains champs à virgule fixe
sans attribut `unit`. Un coefficient sans unité ne suffit pas.

Toute unité inconnue dans `unit` ou `alt_unit` provoque `bad_message_file`
dès le chargement du catalogue, avant le démarrage des communications. Le
message d'erreur indique le fichier, message, champ, libellé et la table à
compléter. Il faut ajouter la règle ou corriger le XML avant de relancer.
Les dimensions incohérentes sont aussi refusées au chargement.

Un libellé connu mais sans conversion SI reste utilisable par les getters
natifs ; une demande SI lève `field_unit_error`. Cela concerne notamment les
commandes, capteurs sans calibration et `GROUND_REF.pos`, qui mélange degrés
et mètres selon le repère. Les transformations de repères, références
 d'altitude et epochs restent à la charge de l'application.

### Adopter l'API SI dans un programme existant

Vous pouvez remplacer localement une lecture par `getFieldSI()` ou une
écriture par `setFieldSI()` lorsque la grandeur est fournie en SI et que ses
métadonnées permettent la conversion. Les transports série, UDP et Ivy
continuent à transmettre les mêmes types et valeurs XML. Recompilez la
bibliothèque et les programmes clients : la disposition des classes C++ a
changé, ce qui interdit de réutiliser des binaires construits avec les anciens en-têtes.

Les clients série, PTY et Ivy fournis utilisent maintenant `getFieldSI()` et
`setFieldSI()` pour leurs altitudes. Le XML fourni possède `unit="m"`, mais
le même code fonctionne avec une altitude XML entière en millimètres : les
programmes continuent à fournir et afficher des mètres. Les réglages génériques `GUIDE_SETTING.value` et
`CLIENT_SETTING.value` n'ont pas d'unité physique connue : gardez leur accès
natif. Le routage de `link++` relaie lui aussi les valeurs XML ; les identifiants
`ac_id`, masques, commandes et compteurs n'exigent aucune conversion SI.
Les traitements applicatifs de mesures peuvent appeler les méthodes SI sur
les messages reçus, sans modifier les codecs ni le relais.

### Observer une erreur sans arrêter le programme

Ajoutez l'en-tête `<pprzlink/exceptions/pprzlink_exception.h>`, puis :

```cpp
try {
    // 300 déborde du uint8 XML : la validation échoue avant de remplacer le champ.
    command.setField("ac_id", 300);
} catch (const pprzlink::field_conversion_error &error) {
    std::cout << "Valeur refusée : " << error.what() << '\n';
}
std::cout << "ac_id est toujours " << command.getFieldAs<int>("ac_id") << '\n';
```

Une écriture invalide laisse la valeur précédente de **ce champ** intacte.
L'appel à plusieurs couples procède de gauche à droite : si le deuxième couple
échoue, le premier reste appliqué. Ce n'est pas une transaction.

Pour préparer une modification qui doit réussir entièrement avant de remplacer
le message initial, travaillez sur une copie :

```cpp
// La copie possède ses valeurs : une erreur ne modifiera que cette candidate.
auto candidate = command;
candidate.setField("ac_id", 43, "value", 15.0);
// Cette affectation n'est atteinte que si toutes les écritures ont réussi.
command = candidate;
```

**À essayer :** demandez `getFieldAs<int>("value")` lorsque `value` vaut `12.5`.
L'erreur confirme que la bibliothèque ne tronque pas silencieusement ce nombre.

## 4. Manipuler des tableaux et du texte

Ajoutez les en-têtes `<array>`, `<span>`, `<vector>`, `<string>` et `<string_view>`. Le message
`GUIDE_SAMPLES` permet d'apprendre quatre formes de champs :

```cpp
pprzlink::Message samples(dictionary.getDefinition("GUIDE_SAMPLES"));
// Les conteneurs sont copiés dans le message et les longueurs fixes sont vérifiées.
samples.setField(
    "axes", std::array<float, 3>{1.0f, 2.0f, 3.0f},
    "samples", std::vector<std::uint16_t>{100, 200, 300},
    "label", std::string("TEST"),
    "text", std::string("Mesure de départ"));

// Ces spans empruntent les tableaux natifs : aucun élément n'est copié ni converti.
// Le remplacement du champ ou la destruction du message invalide la vue correspondante.
const auto axes = samples.getField<std::span<const float>>("axes");
const auto values = samples.getField<std::span<const std::uint16_t>>("samples");
// Les string_view empruntent les caractères, en conservant leur longueur exacte.
const auto label = samples.getField<std::string_view>("label");
const auto description = samples.getField<std::string_view>("text");

std::cout << "Axe X : " << axes[0] << ", " << values.size() << " mesures\n";
std::cout << label << " : " << description << '\n';
```

`float[3]` exige exactement trois éléments. `uint16[]` autorise une taille
variable ; un `std::vector<std::uint16_t>{}` représente un tableau vide.
Les types des éléments doivent correspondre au XML lors de la lecture.

`getField<std::span<const T>>()` emprunte sans copie le tableau stocké dans le
message et renvoie sa longueur. Les getters acceptent les spans à taille
dynamique, même pour un champ XML de taille fixe comme `float[3]`. Une copie
reste disponible avec `getField<std::array<T, N>>()` ou
`getField<std::vector<T>>()`. Les vues restent valides jusqu'au remplacement du
champ, à la réaffectation ou à la destruction du message. Les lectures par index
et avec paramètre de sortie acceptent également les spans. Les éléments sont
constants : `std::span<T>` permettrait de les modifier, même si la vue est
déclarée `const auto`, et n'est donc pas accepté par ces getters.

Pour lire un tableau en unités SI, ses éléments doivent avoir une unité connue.
Dans `messages.xml`, remplacez la définition du champ `axes` par :

```xml
<!-- Les trois valeurs natives sont en centimètres ; la lecture SI fournira des mètres. -->
<field name="axes" type="float[3]" unit="cm"/>
```

Ajoutez ensuite cet extrait après la lecture des axes :

```cpp
// Convertit chaque float en cm vers un double en mètres, dans un vecteur possédé.
// Ce résultat reste valide indépendamment des vues natives et de la durée de vie du message.
const auto axes_si = samples.getFieldArraySI("axes");
std::cout << "Axes en mètres :";
for (const double metres : axes_si) {
    std::cout << ' ' << metres;
}
std::cout << '\n';
```

Recompilez et relancez le programme pour recharger ce XML. Avec les valeurs
natives `1.0f`, `2.0f`, `3.0f` écrites plus haut, vous obtenez :

```text
Axes en mètres : 0.01 0.02 0.03
```

`getFieldArraySI()` renvoie un `std::vector<double>` contenant les valeurs
converties en SI. Vous n'avez à préciser ni le type XML ni la longueur : ce
getter fonctionne pour les tableaux fixes et variables. Le vecteur possède
ses éléments et reste valide si le champ est remplacé ou le message détruit.
La lecture SI conserve les valeurs natives du message ; la vue `axes` contient
toujours `1.0f`, `2.0f`, `3.0f`, exprimées ici en centimètres.

`char[4]` représente quatre caractères, stockés comme un tableau. L'écriture
depuis `std::string` est pratique. Pour obtenir une copie du tableau, utilisez
`getField<std::vector<char>>()`. `string` est un champ texte distinct, dont
`getField<std::string>()` fournit une copie.

`getField<std::string_view>()` permet de lire sans copier les caractères des
champs `string`, `char[]` et `char[N]`. La vue emprunte le stockage du message :
elle reste valide jusqu'au remplacement de ce champ, à la réaffectation ou à
la destruction du message. Elle ne doit pas être obtenue depuis un message
temporaire. La lecture par index et les surcharges avec paramètre de sortie
acceptent aussi `std::string_view`. La taille est conservée intégralement,
y compris les éventuels octets nuls ; aucun terminateur n'est nécessaire.

Pour `char[4]`, la chaîne `"TEST"` fournit exactement quatre caractères : aucun
terminateur nul n'est ajouté. `"OK"` est trop court et `"HELLO"` trop long.
Les longueurs sont des nombres d'octets ; un caractère accentué en UTF-8 peut
occuper plusieurs octets.

Le format binaire encode la longueur des tableaux variables et des chaînes
sur un octet, soit au plus 255 éléments ou octets. La taille totale du message
doit aussi tenir dans le transport choisi : une trame PPRZ complète est limitée
à 255 octets. Un tableau autorisé isolément peut donc être trop grand pour
le message complet. Les tableaux de `string` ne sont pas pris en charge par
le codec binaire.

**À essayer :** remplacez les trois axes par deux éléments. Vous obtenez une
erreur de longueur à l'écriture, avant toute communication.

## 5. Encoder puis décoder sans matériel

Nous avons jusqu'ici manipulé des objets C++. Pour les transporter, il faut
en faire des octets. Un **codec** réalise cette conversion ; un **transport**
l'associe à un canal d'entrée/sortie. Pour essayer sans réseau ni matériel,
`MemoryDevice` reboucle les octets envoyés vers la réception.

Ajoutez `<pprzlink/MemoryDevice.h>`, `<pprzlink/PprzTransport.h>`,
`<boost/asio/io_context.hpp>` et `<memory>`, puis cet extrait au programme
de l'étape 2, avant `return 0` :

```cpp
altitude.setSenderId(42);
altitude.setReceiverId(0);

// Le contexte exécutera les callbacks ; il doit survivre au périphérique et au transport.
boost::asio::io_context context;
// make_unique crée le périphérique, puis le transport en prend la propriété exclusive.
pprzlink::PprzTransport loopback(
    std::make_unique<pprzlink::MemoryDevice>(context), dictionary);
// Enregistre le callback ; [&] lui permet d'appeler loopback.stop() par référence.
// Le Message reçu est emprunté seulement pendant cet appel.
loopback.bind("GUIDE_ALTITUDE", [&](const pprzlink::Message &message) {
    std::cout << "Altitude décodée : "
              << message.getFieldSI("altitude") << " m\n";
    // Désactive la réception ; les notifications déjà postées seront traitées sans nouveau message.
    loopback.stop();
});

// Active la réception avant l'envoi ; bind() seul ne commence pas à lire.
loopback.start();
// L'envoi écrit dans MemoryDevice ; le callback attend le traitement des événements.
const auto frameSize = loopback.sendMessage(altitude);
std::cout << "Trame complète : " << frameSize << " octets\n";
// Exécute les callbacks sur ce thread. Après stop() et le traitement des notifications
// restantes, aucune opération ne maintient la boucle active : run() rend la main.
context.run();
```

Vous devez retrouver `123.5` et une taille de **12 octets** : quatre octets
de champ `float`, quatre d'en-tête PPRZLINK v2 et quatre d'enveloppe PPRZ.
`altitude.getByteSize()` ne compte que les champs ; `frameSize` compte
la trame complète.

`bind()` associe le nom du message à une lambda. `start()` active la réception,
puis `sendMessage()` encode le message et renvoie le nombre d'octets écrits.
`context.run()` traite les événements Asio sur le thread appelant : c'est
pendant cet appel que la lambda reçoit le message décodé. Elle arrête le
transport avec `stop()`, puis `run()` rend la main.

Le transport conserve les octets incomplets et appelle la lambda uniquement
pour un message complet. L'application n'a pas à vérifier les fragments ni à
interroger la réception. Le composant `core` suffit pour cet exemple.

### Essayer aussi le format texte

Pour préparer l'étape Ivy, ajoutez `<pprzlink/IvyMessageCodec.h>` :

```cpp
// Produit le texte du protocole, avec les valeurs natives et les guillemets requis par Ivy.
const auto line = pprzlink::ivy_codec::serializeMessage(altitude);
std::cout << line << '\n';

// Le parseur reçoit séparément la définition, l'expéditeur et le corps du message.
auto fromText = pprzlink::ivy_codec::parseMessageBody(
    dictionary.getDefinition("GUIDE_ALTITUDE"),
    "42", "GUIDE_ALTITUDE 123.5");
std::cout << fromText.getFieldSI("altitude") << '\n';
```

La ligne sérialisée commence par `42 GUIDE_ALTITUDE`, suivie de l'altitude.
Pour une altitude de `123.6`, elle vaut `42 GUIDE_ALTITUDE 123.6` : les nombres
transmis par Ivy ne sont pas complétés à six décimales. Le codec utilise
`std::to_chars` sur le type stocké pour produire la représentation courte qui
préserve exactement la valeur d'un `float` ou `double` fini à l'aller-retour.
La notation scientifique peut apparaître pour des valeurs très petites ou
très grandes ; les valeurs minuscules et le signe du zéro sont conservés.
Ce format ne dépend ni de la locale ni de la précision de `std::cout`.
Les champs `float` et `double`, scalaires ou tableaux, acceptent aussi `nan`,
`inf`, leurs formes majuscules `NAN`, `INF`, avec un signe `+` ou `-` facultatif.
Les codecs Ivy standard et Legacy conservent le sens de NaN et le signe de
l'infini ; le texte ne conserve pas les bits de charge utile d'un NaN.
Un dépassement numérique ou un jeton mal formé reste rejeté avec
`wrong_message_format`. Ces formes non finies ne sont pas acceptées pour un entier.

`parseMessageBody()` reçoit séparément l'expéditeur et le corps du message,
qui commence par le nom. Ces fonctions ne nécessitent pas Ivy installé : elles
appartiennent à `core` et ne font aucune communication.

Pour manipuler uniquement les octets, `<pprzlink/BytesBuffer.h>` fournit
`BytesBuffer`, alias de `std::vector<uint8_t>`, sans inclure Asio ni `Device.h`.
Les en-têtes des valeurs et codecs utilisent ce type indépendant. La cible
CMake `core` conserve toutefois Boost pour ses interfaces de réception et de transport.

`toString()` est destiné au diagnostic ; `serializeMessage()` produit le
format texte d'échange. Utilisez le codec, plutôt qu'une concaténation manuelle,
pour gérer les champs texte et les tableaux.

## 6. Échanger des messages en UDP

Nous allons maintenant envoyer l'altitude à un second socket du même programme.
Cette expérience utilise le réseau local de la machine et ne demande aucun
matériel. Le système choisira deux ports libres.

Dans `CMakeLists.txt`, remplacez les lignes qui sélectionnent et lient `core`
par :

```cmake
# Le composant io ajoute UDP et la liaison série aux fonctions de core.
find_package(pprzlink++ CONFIG REQUIRED COMPONENTS io)
# Transmet au programme les bibliothèques et dépendances nécessaires aux entrées/sorties.
target_link_libraries(atelier PRIVATE pprzlink::io)
```

Conservez `add_executable(atelier main.cpp)` avant `target_link_libraries()`.
Le composant `io` inclut les fonctions de `core` et ajoute notamment UDP et
la série.

Remplacez maintenant **tout `main.cpp`** par :

```cpp
#include <pprzlink/UdpTransport.h>
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <boost/asio/steady_timer.hpp>

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Usage: atelier messages.xml\n";
        return 2;
    }
    try {
        using namespace std::chrono_literals;
        const pprzlink::MessageDictionary dictionary(argv[1]);
        // Déclaré avant les transports, le contexte leur survivra à la destruction.
        boost::asio::io_context context;

        // Le port 0 demande un port libre au système, distinct pour chaque socket.
        pprzlink::UdpTransport receiver(context, dictionary,
            {.local = {"127.0.0.1", 0}});
        pprzlink::UdpTransport sender(context, dictionary,
            {.local = {"127.0.0.1", 0}});

        pprzlink::Message altitude(dictionary.getDefinition("GUIDE_ALTITUDE"));
        altitude.setSenderId(42);
        altitude.setReceiverId(0);
        altitude.setFieldSI("altitude", 123.5);

        bool received = false;
        // Fixe l'échéance à deux secondes ; la construction ne bloque pas le programme.
        boost::asio::steady_timer timeout(context, 2s);
        // Programme un callback exécuté par run(), à l'échéance ou lors d'une annulation.
        // [&] capture les variables locales par référence ; elles vivent jusqu'au retour de run().
        timeout.async_wait([&](const boost::system::error_code &error) {
            // À l'échéance, error est vide : stop() annule la lecture UDP et ne la relance pas.
            // Si le timer a été annulé après une réception, error vaut operation_aborted : on l'ignore.
            if (!error) receiver.stop();
        });
        // Enregistre l'abonnement sans lire immédiatement ; les arguments seront empruntés au callback.
        receiver.bind("GUIDE_ALTITUDE",
            [&](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
                // Une réception UDP fournit l'IP et le port source réels du datagramme.
                const auto &peer = info.udpPeer.value();
                std::cout << "Reçu de " << peer.address << ':' << peer.port
                          << " : " << message.toString() << '\n';
                received = true;
                // Désactive la réception pour empêcher la lecture suivante, sans arrêter le contexte partagé.
                receiver.stop();
                // Annule l'attente des deux secondes, sinon elle maintiendrait run() actif.
                // Le callback d'une attente annulée reçoit operation_aborted.
                timeout.cancel();
            });
        // Arme la lecture avant l'envoi ; l'arrivée du message sera traitée dans run().
        receiver.start();
        // Récupère le port choisi par le système, au lieu du 0 demandé au constructeur.
        const auto destination = receiver.localEndpoint();
        // Écrit le datagramme ; cela ne déclenche pas le callback de réception sur-le-champ.
        const auto sent = sender.sendMessage(altitude, destination);
        std::cout << "Envoyé : " << sent << " octets vers le port "
                  << destination.port << '\n';
        // Attend et traite les événements sur ce thread. Message reçu : lecture non relancée, timer annulé.
        // Délai expiré : timer terminé et lecture annulée. Dans les deux cas, run() traite
        // les dernières annulations puis rend la main, car il ne reste plus d'opération en attente.
        context.run();
        if (!received) throw std::runtime_error("Aucun message UDP reçu dans le délai prévu");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

Reconfigurez, compilez et lancez :

```sh
# Actualise la configuration après le passage de core à io.
cmake -S . -B build
# Compile, puis lance l'échange entre les deux sockets UDP du programme.
cmake --build build
./build/atelier messages.xml
```

Vous voyez l'envoi de 12 octets, puis la réception de `GUIDE_ALTITUDE` avec
`altitude=123.5` selon le format d'affichage du codec. Les ports changent
d'une exécution à l'autre.

Trois détails expliquent ce programme :

1. Le port local `0` demande au système un port disponible. `localEndpoint()`
   permet de connaître le port réellement attribué.
2. `sendMessage()` demande une destination IP/port explicite. Le destinataire
   PPRZLINK, ici 0, est une information distincte de cette destination réseau.
3. `bind()` conserve la lambda à appeler lorsque le message arrive. `start()`
   active la lecture et `context.run()` exécute les callbacks Asio sur le thread
   appelant. Aucun thread supplémentaire n'interroge les canaux.

Le timer empêche d'attendre indéfiniment. Asio attend les données ou l'échéance ;
aucune boucle avec `sleep_for()` n'est nécessaire. `stop()` annule les lectures
de ce récepteur ; l'annulation du timer permet à `context.run()` de terminer.

### Passer à deux processus

Utilisez deux instances du même exécutable, avec les modes `receiver` et
`emitter` de l'exemple présenté ci-dessous. Le récepteur écoute sur
`{"127.0.0.1", 4242}` et l'émetteur utilise cette destination. Pour écouter sur
les interfaces réseau de la machine,
utilisez l'adresse locale `"0.0.0.0"` ; l'émetteur devra viser l'adresse IP
réelle du récepteur.

Si votre protocole répond au port d'origine, `info.udpPeer` donne la
destination de la réponse. D'autres applications, dont certaines configurations
Paparazzi, utilisent un port montant distinct : c'est une décision de votre
application, pas une déduction faite par la bibliothèque.

Le callback emprunte le `Message` et le `ReceiveInfo` pendant son invocation.
Copiez ces objets pour les conserver. Les vues `span`/`string_view` empruntées
ne doivent pas survivre au message qui les contient.
Le décodeur UDP abandonne les trames incomplètes à la fin d'un datagramme ;
il ne mélange pas les morceaux de deux datagrammes.

**À essayer :** envoyez trois altitudes et comptez les invocations de la lambda
avant d'arrêter le récepteur au troisième message. Vous pouvez aussi envoyer `GUIDE_SETTING` après avoir rempli
ses deux champs : le mécanisme de transport reste identique.

### Les trois modes de l'atelier

L'exemple [WorkshopUdp.cpp](../../lib/v2.0/C++/pprzlink/examples/clients/WorkshopUdp.cpp) propose
`both`, `emitter` et `receiver`. Pour l'utiliser comme `main.cpp` :

```sh
# Utilise l'exemple complet comme programme de l'atelier, puis le recompile.
cp "$PPRZLINK_CPP/pprzlink/examples/clients/WorkshopUdp.cpp" main.cpp
cmake --build build
# Le mode both réunit l'émetteur et le récepteur dans un même processus.
./build/atelier messages.xml both
```

Dans un premier terminal, lancez le mode `receiver` :

```sh
# Le même exécutable écoute sur 127.0.0.1:4242 pendant 30 secondes, ou jusqu'à Ctrl+C.
./build/atelier messages.xml receiver
```

Dans le second terminal, lancez le mode `emitter` :

```sh
# Le rôle choisi par l'argument envoie les trois altitudes au récepteur.
./build/atelier messages.xml emitter
```

### Choisir les messages et leurs émetteurs

Le callback simple reçoit seulement le message. Pour écouter tous les types,
utilisez `receiver.bind(pprzlink::ALL, lambda)`. Le récepteur conserve le binding ;
il n'est pas nécessaire de garder l'identifiant retourné.

```cpp
// Combine le nom du message et le senderId de l'en-tête PPRZLINK, distinct de l'adresse UDP.
receiver.bind("GUIDE_ALTITUDE", {.senderId = 42},
    [](const pprzlink::Message &message) {
        std::cout << message.getFieldSI("altitude") << " m\n";
    });
```

`senderId` sélectionne l'émetteur PPRZLINK. Pour sélectionner l'origine réseau,
utilisez `udpPeer` (IP et port), `udpAddress` (IP seule) ou `udpPort` (port seul).
Ces critères sont distincts et peuvent être combinés. `receiverId`, `className`,
`classId`, `componentId` et un prédicat `where` complètent la sélection.
Voir le [contrat des filtres](API_USAGE.md#réception-réactive--abonnements-et-filtres).

## 7. Passer à une liaison série

Sur une liaison série, deux objets coopèrent : le **Device** lit et écrit
des octets ; le **Transport** transforme ces octets en messages PPRZLINK.
Ici, nous utiliserons `BoostSerialPortDevice` et `PprzTransport`.

Le projet reste lié à `pprzlink::io`. Cet exemple utilise deux adaptateurs
USB–série accessibles, le même fichier `messages.xml` et le protocole PPRZ v2.
Les deux ports sont configurés par le programme en **57600 bauds, 8N1,
sans contrôle de flux**.

Reliez les deux adaptateurs avec des niveaux électriques compatibles :

```text
Adaptateur A       Adaptateur B
TX ────────────── RX
RX ────────────── TX
GND ───────────── GND
```

Le même programme tourne dans deux terminaux. Son dernier argument choisit
le rôle : `e` pour **émettre**, `r` pour **recevoir**.
Remplacez tout `main.cpp` par :

```cpp
// SPDX-License-Identifier: LGPL-3.0-or-later
// Atelier série : émettre (e) ou recevoir (r) avec deux adaptateurs reliés.
#include <pprzlink/BoostSerialPortDevice.h>
#include <pprzlink/PprzTransport.h>
#include <boost/asio/signal_set.hpp>
#include <csignal>
#include <exception>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

pprzlink::Message altitude(const pprzlink::MessageDictionary &dictionary, double metres)
{
  // Crée un message selon le schéma XML, avec un champ altitude encore sans valeur.
  pprzlink::Message message(dictionary.getDefinition("GUIDE_ALTITUDE"));
  // Ces identifiants appartiennent à l'en-tête PPRZLINK : avion 42 vers station sol 0.
  message.setSenderId(42);
  message.setReceiverId(0);
  // Fournit des mètres ; la bibliothèque convertit vers l'unité et le type du XML.
  message.setFieldSI("altitude", metres);
  return message;
}


int receive(boost::asio::io_context &context, pprzlink::PprzTransport &transport,
            const char *port)
{
  // Observe Ctrl+C (SIGINT) et SIGTERM pendant toute l'exécution de context.run().
  boost::asio::signal_set signals(context, SIGINT, SIGTERM);
  // Compte les messages décodés, pas les octets ni les fragments de trame.
  unsigned int count = 0;
  // [&] permet d'accéder au transport ; run() exécutera le callback sur ce thread.
  signals.async_wait([&](const boost::system::error_code &error, int) {
    // Un signal reçu sans erreur arrête la réception et annule la lecture série en cours.
    // stop() n'arrête pas le contexte partagé avec d'autres canaux.
    if (!error) transport.stop();
  });
  // ALL sélectionne tous les types du catalogue ; bind() enregistre le callback sans lire.
  transport.bind(pprzlink::ALL, [&](const pprzlink::Message &message) {
    // Le message est emprunté pendant cet appel ; copiez-le si vous devez le conserver.
    std::cout << "Reçu : " << message.toString() << std::endl;
    ++count;
  });
  // Commence les lectures asynchrones après avoir installé les callbacks.
  transport.start();
  std::cout << "Récepteur prêt sur " << port
            << " (57600 bauds, 8N1 ; Ctrl+C pour arrêter)" << std::endl;
  // Attend et traite les lectures et les signaux sur ce thread, sans interrogation périodique.
  // Après un signal, son attente est terminée et la lecture est annulée ;
  // run() traite les dernières notifications puis rend la main.
  context.run();
  std::cout << "Reçu " << count << " message(s)\n";
  return 0;
}


int emit(pprzlink::PprzTransport &transport, const pprzlink::MessageDictionary &dictionary)
{
  // Chaque altitude produit un message distinct, entièrement rempli avant son envoi.
  for (const double metres : {100.0, 110.0, 123.5}) {
    const auto message = altitude(dictionary, metres);
    // Encode une trame PPRZ v2 et l'écrit de façon synchrone sur le port série.
    // Le résultat compte les octets écrits, sans confirmer la réception distante.
    const auto sent = transport.sendMessage(message);
    std::cout << "Envoyé : " << message.toString()
              << " (" << sent << " octets)" << std::endl;
  }
  // Les écritures sont synchrones : ce mode termine sans start() ni context.run().
  return 0;
}


int main(int argc, char **argv)
{
  // Attend le catalogue XML, le chemin du port et un rôle valide ; argc protège l'accès à argv[3].
  if (argc != 4 || (std::string_view(argv[3]) != "e" && std::string_view(argv[3]) != "r")) {
    std::cerr << "Usage: " << argv[0] << " messages.xml port-serie e|r\n"
              << "  e : émettre trois altitudes\n"
              << "  r : recevoir jusqu'à Ctrl+C\n";
    return 2;
  }
  try {
    // Charge les définitions à l'exécution ; le dictionnaire doit survivre au transport.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    // Le contexte doit aussi survivre au port et au transport qui l'utilisent.
    boost::asio::io_context context;

    using Serial = pprzlink::BoostSerialPortDevice;
    // Ouvre le port indiqué ; make_unique prépare sa propriété exclusive.
    auto device = std::make_unique<Serial>(context, argv[2]);
    // Configure le port avant de transférer sa propriété au transport ; les deux côtés doivent correspondre.
    device->setBaudrate(Serial::Baudrate(57600)); // Débit de la liaison : 57600 bauds.
    device->setDataBits(Serial::DataBits(8)); // 8 bits de données.
    device->setParity(Serial::Parity(Serial::Parity::none)); // N : aucune parité.
    device->setStopBits(Serial::StopBits(Serial::StopBits::one)); // 1 : un bit d'arrêt.
    device->setFlowcontrol(Serial::Flowcontrol(Serial::Flowcontrol::none)); // Aucun contrôle de flux.

    // Le transport encadre/décode les messages et devient propriétaire du port.
    // std::move vide device ; le transport fermera le port à sa destruction.
    pprzlink::PprzTransport transport(std::move(device), dictionary);
    // L'émetteur écrit puis termine ; le récepteur attend jusqu'à Ctrl+C.
    if (std::string_view(argv[3]) == "e") return emit(transport, dictionary);
    return receive(context, transport, argv[2]);
  } catch (const std::exception &error) {
    // Affiche les erreurs de chargement du XML, de configuration du port ou d'I/O.
    // À la sortie du bloc try, le transport est détruit avant le contexte et le dictionnaire.
    std::cerr << "Erreur : " << error.what() << '\n';
    return 1;
  }
}
```

Compilez depuis le répertoire `projet` :

```sh
# Recompile l'atelier après avoir remplacé le programme UDP par le programme série.
cmake --build build
```

Dans un premier terminal, lancez le **récepteur** sur l'adaptateur A :

```sh
# Le rôle r écoute sur le premier adaptateur jusqu'à Ctrl+C.
./build/atelier messages.xml /dev/ttyUSB0 r
```

Attendez l'affichage de « Récepteur prêt ». Dans un second terminal, depuis
le même répertoire `projet`, lancez l'**émetteur** sur l'adaptateur B :

```sh
# Le rôle e envoie les trois altitudes sur le second adaptateur, puis termine.
./build/atelier messages.xml /dev/ttyUSB1 e
```

Adaptez `/dev/ttyUSB0` et `/dev/ttyUSB1` aux noms réels de vos périphériques.
Chaque processus ouvre son propre adaptateur ; les deux utilisent le même XML.

Le mode `e` envoie trois messages `GUIDE_ALTITUDE` : **100, 110 et 123,5 m**,
puis termine. `sendMessage()` écrit chaque trame sur le port ; son retour
ne confirme pas sa réception par le correspondant.

Le mode `r` affiche les messages reçus et continue à écouter. Arrêtez-le
avec **Ctrl+C** ; il affiche alors le nombre de messages reçus. Il n'émet
aucun message et n'a pas de délai d'expiration.

Avec cette liaison, le récepteur doit afficher :

```text
Reçu : GUIDE_ALTITUDE [altitude=100.000000]
Reçu : GUIDE_ALTITUDE [altitude=110.000000]
Reçu : GUIDE_ALTITUDE [altitude=123.500000]
```

`std::move(device)` transfère au transport la propriété du périphérique :
le transport le détruira à sa propre destruction. La variable `device` est
alors vide ; configurez donc le port avant ce transfert.

Comme UDP, la réception série utilise Boost.Asio. `transport.start()` commence
les lectures asynchrones, puis `context.run()` traite les lectures et les
abonnements. Le `signal_set` reçoit Ctrl+C et appelle `transport.stop()` pour
annuler la lecture en cours ; le contexte rend alors la main.

Les callbacks applicatifs sont appelés après la libération du mutex d'I/O du
périphérique. Une lambda peut envoyer une réponse, se désabonner ou arrêter
son récepteur. `stop()` conserve les bindings pour un éventuel `start()`
ultérieur, sans arrêter un contexte partagé avec d'autres canaux.

L'ordre des déclarations est intentionnel : le transport est détruit avant
le contexte et le dictionnaire qu'il utilise. Nous reviendrons à ces durées
de vie à l'étape 12.

### Réagir au bon message

Un transport peut recevoir plusieurs types de messages. Dans une application
qui poursuit sa réception, remplacez l'abonnement `ALL` de l'exemple par :

```cpp
// Le filtre vérifie le destinataire de l'en-tête, pas le champ applicatif ac_id.
transport.bind("GUIDE_SETTING", {.receiverId = 42},
    [](const pprzlink::Message &message) {
        // uint8 -> int est une conversion numérique contrôlée ; value reste lu comme float XML.
        const auto aircraft = message.getFieldAs<int>("ac_id");
        const auto value = message.getField<float>("value");
        std::cout << "Réglage pour " << aircraft << " : " << value << '\n';
    });
```

La bibliothèque ne décide pas à votre place si un message reçu doit être
appliqué à votre avion. Vérifiez le type de message et les informations
d'adressage attendues par votre application.

Pour tester les deux côtés avec le même programme, utilisez
[SerialAircraft.cpp](../../lib/v2.0/C++/pprzlink/examples/clients/SerialAircraft.cpp) et son
catalogue `client_messages.xml`. Son troisième argument choisit `s` pour le
**serveur avion 42**, ou `c` pour le **client sol 0**. Dans un premier terminal :

```sh
# Le serveur répond au premier PING par une altitude de 123,5 m, puis par PONG.
"$PPRZLINK_ATELIER/build-lib/serial_aircraft" \
  "$PPRZLINK_CPP/pprzlink/examples/clients/client_messages.xml" /dev/ttyUSB0 s
```

Dans le second terminal :

```sh
# Le client envoie le PING et attend l'altitude et le PONG sur l'autre port relié.
"$PPRZLINK_ATELIER/build-lib/serial_aircraft" \
  "$PPRZLINK_CPP/pprzlink/examples/clients/client_messages.xml" /dev/ttyUSB1 c
```

Les ports doivent être distincts et reliés, comme dans l'exemple précédent.
Les deux rôles attendent au plus dix secondes. Le client réessaie toutes les
500 ms, ce qui permet aussi de le lancer avant le serveur. L'altitude part
après le PING, lorsque le client est prêt à la recevoir ; les deux processus
terminent une fois l'échange achevé.

### Essayer deux processus sur des pseudo-terminaux

Sur Linux, `link++ -socat start` crée deux ports virtuels reliés et rend la main.
Il sert ici à gérer la liaison : les deux instances de l'atelier utilisent
directement la bibliothèque, sans lancer l'agent de télémétrie ni le bus Ivy.
Le programme `socat` doit être installé et accessible sur PATH.

L'exemple [PtyAgent.cpp](../../lib/v2.0/C++/pprzlink/examples/clients/PtyAgent.cpp) est symétrique :
le **même programme** tourne dans les deux processus. Chaque instance publie
`GUIDE_ALTITUDE` une fois par seconde et affiche les altitudes qu'elle reçoit.
Elle fait tourner son contexte Asio et utilise uniquement les en-têtes publics
de `pprzlink::io`. La copie du catalogue de l'atelier s'appelle
[guide_messages.xml](../../lib/v2.0/C++/pprzlink/examples/clients/guide_messages.xml).

Les noms **Port A** et **Port B** sont des étiquettes. Les deux extrémités
lisent et écrivent ; aucune n'est réservée au sol, à l'avion, au demandeur ou
au répondeur. Vous pouvez permuter les ports sans changer les programmes.

Depuis la **racine du dépôt Paparazzi**, compilez les exemples une fois dans
un répertoire distinct de celui du `make` habituel :

```sh
# Configure une construction distincte avec les exemples et l'outil link++.
cmake -S sw/ext/pprzlink/lib/v2.0/C++ -B var/build/pprzlink-pty-ubuntu26 \
  -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Debug \
  -DPPRZLINK_BUILD_EXAMPLES=ON -DPPRZLINK_BUILD_LINK=ON -DBUILD_TESTING=OFF
# Compile uniquement l'outil de gestion de la liaison et l'agent de démonstration.
cmake --build var/build/pprzlink-pty-ubuntu26 \
  --target link++ pty_agent
# Crée les deux pseudo-terminaux reliés ; relève les noms Port A et Port B affichés.
./var/build/pprzlink-pty-ubuntu26/apps/link/link++ -socat start
```

Le démarrage affiche **Port A** et **Port B**, par exemple `/dev/pts/12` et
`/dev/pts/13`. Dans un premier terminal, ouvrez l'un des ports avec
l'identifiant 1 :

```sh
# Lance l'agent 1 sur Port A ; remplace /dev/pts/12 par le chemin affiché par start.
./var/build/pprzlink-pty-ubuntu26/pty_agent \
  sw/ext/pprzlink/lib/v2.0/C++/pprzlink/examples/clients/guide_messages.xml \
  /dev/pts/12 1
```

Dans un second terminal, ouvrez l'autre port avec l'identifiant 2 :

```sh
# Lance l'agent 2 sur Port B ; remplace /dev/pts/13 par l'autre chemin affiché.
./var/build/pprzlink-pty-ubuntu26/pty_agent \
  sw/ext/pprzlink/lib/v2.0/C++/pprzlink/examples/clients/guide_messages.xml \
  /dev/pts/13 2
```

Les identifiants doivent être différents ; ils appartiennent aux messages,
pas aux ports. L'agent 1 publie `124.5 m` et reçoit `125.5 m` de l'agent 2.
L'agent 2 publie `125.5 m` et reçoit `124.5 m` de l'agent 1. Les messages sont
envoyés avec le destinataire de diffusion 255 ; chaque instance ignore ses
propres messages et les messages adressés à un autre identifiant.

```text
Agent 1 TX GUIDE_ALTITUDE 124.5 m
Agent 1 RX from 2: GUIDE_ALTITUDE 125.5 m
```

Les processus continuent jusqu'à **Ctrl+C**. Les adresses `/dev/pts/N` sont
temporaires : utilisez les valeurs réellement affichées par `start`.

À la fin, arrêtez les deux agents avec Ctrl+C, puis la liaison :

```sh
# Arrête socat et supprime la liaison virtuelle après l'arrêt des deux agents.
./var/build/pprzlink-pty-ubuntu26/apps/link/link++ -socat stop
```

Vous pouvez remplacer `guide_messages.xml` par le `messages.xml` créé à
l'étape 1 : les définitions sont identiques. Cet exemple se compile
également avec le SDK installé, en copiant le répertoire `clients` comme
décrit dans [API_USAGE.md](API_USAGE.md). Il se lie uniquement à
`pprzlink::io`, sans Ivy.

Pour travailler spécifiquement la requête et la réponse, utilisez le même
programme [PtyPeer.cpp](../../lib/v2.0/C++/pprzlink/examples/clients/PtyPeer.cpp) aux deux
extrémités. Son troisième argument choisit `c` pour le client demandeur ou
`s` pour le serveur répondeur. Depuis la racine du dépôt Paparazzi :

```sh
# Compile l'unique exécutable qui contient les deux rôles série.
cmake --build var/build/pprzlink-pty-ubuntu26 --target pty_peer
```

Dans le premier terminal :

```sh
# Le serveur avion 42 répond sur Port A ; utilise le chemin réellement affiché par socat.
./var/build/pprzlink-pty-ubuntu26/pty_peer \
  sw/ext/pprzlink/lib/v2.0/C++/pprzlink/examples/clients/guide_messages.xml /dev/pts/12 s
```

Dans le second terminal :

```sh
# Le client sol 0 demande une altitude sur Port B et affiche la réponse reçue.
./var/build/pprzlink-pty-ubuntu26/pty_peer \
  sw/ext/pprzlink/lib/v2.0/C++/pprzlink/examples/clients/guide_messages.xml /dev/pts/13 c
```

Les deux rôles échangent `GUIDE_ALTITUDE_REQ` et `GUIDE_ALTITUDE`, puis
terminent après la réponse. Ils attendent au plus 60 secondes ; le client
réessaie toutes les 500 ms. Vous pouvez permuter les ports et l'ordre de
lancement. Le `messages.xml` du guide peut remplacer `guide_messages.xml`.

## 8. Recevoir et publier sur Ivy

Ivy est un bus de messages texte. Avec `IvyLink`, vous vous abonnez aux
messages qui vous intéressent : une fonction de rappel, ou *callback*, est
appelée lorsqu'un message correspondant arrive.

### Ajouter le composant Ivy

Cette branche nécessite Ivy 3.18 ou plus et son interface native C++
`ivy-cpp`. Le [README](reference.md#build) précise l'installation Ivy utilisée
pour les validations et la configuration d'un préfixe privé.
Conservez la `libstdc++` fournie par Ubuntu 26.04 à la compilation et à
l'exécution : l'interface Ivy installée peut demander des symboles absents d'un
ancien environnement. GCC 13 reste le minimum pour les sources ; cela ne rend
pas une interface Ivy précompilée compatible avec toutes les anciennes versions de `libstdc++`.

Une fois ces dépendances installées, vérifiez leur découverte et reconstruisez
le SDK :

```sh
# Vérifie que pkg-config trouve les interfaces C++ et C d'Ivy dans leur version installée.
pkg-config --modversion ivy-cpp ivy-c

# Réactive Ivy dans la construction existante, puis reconstruit la bibliothèque.
cmake -S "$PPRZLINK_CPP" -B "$PPRZLINK_ATELIER/build-lib" \
  -DPPRZLINK_WITH_IVY=ON
cmake --build "$PPRZLINK_ATELIER/build-lib"
# Met à jour le SDK pour rendre le composant Ivy disponible au projet utilisateur.
cmake --install "$PPRZLINK_ATELIER/build-lib" \
  --prefix "$PPRZLINK_ATELIER/sdk"
```

Dans le `CMakeLists.txt` du projet, remplacez le composant `io` par `ivy` et
la cible `pprzlink::io` par `pprzlink::ivy`, puis reconfigurez avec
`cmake -S . -B build`.

### Un programme, deux rôles

Le même `main.cpp` reçoit ou publie selon son deuxième argument : `r` pour
**recevoir**, `e` pour **émettre**. Le premier argument reste `messages.xml`.
Le domaine Ivy est facultatif ; sa valeur par défaut est
`127.255.255.255:2010`.

Remplacez tout `main.cpp` par le programme suivant. Il est également disponible
dans [WorkshopIvy.cpp](../../lib/v2.0/C++/pprzlink/examples/clients/WorkshopIvy.cpp) :

```cpp
// SPDX-License-Identifier: LGPL-3.0-or-later
// Atelier Ivy : le même programme publie (e) ou reçoit (r) une altitude.
#include <pprzlink/IvyLink.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace {
    void waitForReceiver(pprzlink::IvyLink &link)
    {
        // Ivy découvre les applications et leurs abonnements de façon asynchrone.
        // L'horloge monotone borne cette découverte à cinq secondes.
        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            // Un résultat valide peut encore contenir un optional vide : aucun pair trouvé.
            const auto peer = link.getBus().find_application("guide-recepteur");
            if (!peer) throw std::system_error(peer.error(), "Recherche du récepteur Ivy");
            if (*peer) {
                // **peer fournit l'application ; ses regex décrivent les abonnements annoncés.
                const auto bindings = link.getBus().application_regexps(**peer);
                if (!bindings) throw std::system_error(bindings.error(), "Abonnements Ivy");
                if (!bindings->empty()) return;
            }
            // Le thread principal attend ; le thread Ivy continue à découvrir les pairs.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("Abonnement du récepteur introuvable après 5 secondes");
    }

    int receive(const pprzlink::MessageDictionary &dictionary, const std::string &domain)
    {
        bool received = false;
        // Sans argument true, la boucle Ivy sera exécutée par link.run() sur ce thread.
        pprzlink::IvyLink link(dictionary, "guide-recepteur", domain);
        // Conserve le jeton : sa destruction désabonne le callback.
        // [&] permet de modifier received et d'arrêter link pendant toute l'attente.
        auto subscription = link.subscribeMessage("GUIDE_ALTITUDE",
            [&](std::string sender, pprzlink::Message message) {
                // Le callback reçoit un message complet ; son expéditeur Ivy est textuel.
                std::cout << "Altitude de " << sender << " : "
                          << message.getFieldSI("altitude") << " m" << std::endl;
                received = true;
                link.close();
            });
        // Le résultat possède le jeton du timer ; sa durée de vie conserve l'attente active.
        auto timeout = link.getBus().bind_event(
            [&](std::chrono::milliseconds) { link.close(); },
            ivy::after(std::chrono::seconds(10)));
        if (!timeout) throw std::system_error(timeout.error(), "Temporisation Ivy");
        std::cout << "Récepteur prêt sur Ivy (" << domain
                  << ", attente de 10 secondes)" << std::endl;
        // Traite messages et timer sur ce thread, jusqu'à link.close().
        // subscription et timeout restent vivants pendant tout run().
        link.run();
        if (!received) std::cerr << "Aucune altitude reçue\n";
        return received ? 0 : 1;
    }

    int emit(const pprzlink::MessageDictionary &dictionary, const std::string &domain)
    {
        // Remplit une altitude en mètres SI ; le XML choisit l'unité et le type transmis.
        pprzlink::Message altitude(dictionary.getDefinition("GUIDE_ALTITUDE"));
        altitude.setSenderId(42);
        altitude.setFieldSI("altitude", 123.5);
        // true démarre un thread Ivy pendant que le thread principal attend la découverte.
        // Le destructeur du lien arrêtera et rejoindra ce thread.
        pprzlink::IvyLink link(dictionary, "guide-emetteur", domain, true);
        std::cout << "Émetteur prêt sur Ivy (" << domain << ')' << std::endl;
        waitForReceiver(link);
        // Publie après l'annonce de l'abonnement : Ivy ne conserve pas les messages pour plus tard.
        link.sendMessage(altitude);
        std::cout << "Altitude publiée sur Ivy." << std::endl;
        // Remonte sur ce thread les erreurs éventuelles des callbacks du thread Ivy.
        const auto status = link.getBus().take_callback_error();
        if (!status) throw std::system_error(status.error(), "Callback Ivy");
        return 0;
    }
}

int main(int argc, char **argv)
{
    // Le rôle suit le XML ; un domaine facultatif permet de choisir un autre bus Ivy.
    if (argc < 3 || argc > 4 ||
        (std::string_view(argv[2]) != "e" && std::string_view(argv[2]) != "r")) {
        std::cerr << "Usage: " << argv[0] << " messages.xml e|r [domaine-Ivy]\n"
                  << "  e : publier une altitude\n"
                  << "  r : recevoir une altitude\n";
        return 2;
    }
    try {
        // Le catalogue reste vivant jusqu'au retour du rôle choisi et à la destruction de son lien.
        const pprzlink::MessageDictionary dictionary(argv[1]);
        const std::string domain = argc == 4 ? argv[3] : "127.255.255.255:2010";
        if (std::string_view(argv[2]) == "e") return emit(dictionary, domain);
        return receive(dictionary, domain);
    } catch (const std::exception &error) {
        // Les erreurs XML, de découverte ou de callbacks donnent un code de sortie non nul.
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

La fonction `receive()` utilise `subscribeMessage()`. Son callback reçoit
**un message complet**, avec les champs déjà décodés, et le nom textuel de
l'expéditeur dans `sender`. Conservez `subscription` pendant toute la durée
souhaitée de l'abonnement : sa destruction désabonne le callback.

`link.run()` traite les événements sur le thread appelant. Le callback appelle
`close()` après la première altitude ; le timer fait de même si aucun message
n'arrive pendant dix secondes. Cet appel termine la session et demande le retour
de `run()`. Le jeton du timer reste vivant dans `timeout` pendant toute l'attente.

Pour une pause temporaire des abonnements enregistrés avec `bind()`, utilisez
`stop()`, puis `start()` pour reprendre avec les mêmes bindings. Pendant cette
pause, les abonnements natifs `subscribeMessage()` et les requêtes restent
actifs, ainsi que les envois et les timers : la boucle Ivy continue de tourner.
`close()` termine la session définitivement ; pour en ouvrir une autre, créez
un nouvel `IvyLink`. Ces deux méthodes peuvent être appelées depuis un callback.

La fonction `emit()` construit le message et fournit l'altitude en mètres SI.
Le paramètre `true` du constructeur démarre une boucle Ivy sur un thread
appartenant au lien. Ce thread poursuit la découverte pendant que le thread
principal attend dans `waitForReceiver()`. Dans ce rôle, la boucle est déjà
active ; le destructeur du lien l'arrête et rejoint son thread.

Ivy publie auprès des abonnés présents au moment de l'envoi. La découverte
vérifie donc l'application `guide-recepteur` **et ses abonnements annoncés**
avant de publier. Elle attend au plus cinq secondes ; aucune saisie au clavier
n'est nécessaire.

### Lancer les deux instances

Reconfigurez et compilez l'unique exécutable `atelier` :

```sh
# Actualise le lien vers le composant Ivy, puis compile le programme avec ses deux rôles.
cmake -S . -B build
cmake --build build
```

Dans un premier terminal, depuis `projet`, lancez le récepteur :

```sh
# Le rôle r attend une altitude sur le bus Ivy pendant dix secondes au maximum.
./build/atelier messages.xml r
```

Dans un second terminal, depuis le même répertoire, lancez l'émetteur :

```sh
# Le rôle e attend l'abonnement du récepteur, publie 123,5 m et termine.
./build/atelier messages.xml e
```

Le récepteur doit afficher `Altitude de 42 : 123.5 m`, puis terminer.
L'émetteur peut aussi démarrer en premier si le récepteur le rejoint avant
l'expiration des cinq secondes de découverte. Pour choisir un autre bus,
ajoutez son domaine après le rôle, identique pour les deux instances, par
exemple `./build/atelier messages.xml r 127.255.255.255:2020`.

Le nom d'application `guide-emetteur` identifie le participant Ivy.
`setSenderId(42)` identifie l'expéditeur dans le message PPRZLINK : ce sont
deux noms différents. Sur Ivy, vous pouvez aussi écrire
`setSenderId(std::string("station-sol"))`. Un envoi binaire exigera un
identifiant numérique entre 0 et 255, éventuellement sous forme de texte
numérique.

Enfin, le format texte Ivy n'encode pas les destinataire et composant de
l'en-tête binaire. `setReceiverId()` ne sélectionne donc pas un destinataire
sur le bus Ivy. Le contenu XML et les règles de l'application déterminent
comment traiter une commande. L'exemple
[IvyRoundTrip.cpp](../../lib/v2.0/C++/pprzlink/examples/IvyRoundTrip.cpp) réunit aussi deux liens
Ivy dans un même processus pour un échange autonome.

## 9. Faire une requête et attendre sa réponse

Une publication annonce une information. Une requête demande une réponse
associée à une demande précise. `IvyLink` fournit cette association pour les
messages dont le nom se termine par `_REQ`.

Notre XML contient déjà `GUIDE_ALTITUDE_REQ`. La réponse attendue est
`GUIDE_ALTITUDE`, c'est-à-dire le même nom sans le suffixe `_REQ`. Ce
mécanisme est propre à l'adaptateur Ivy présenté ici ; un couple PING/PONG
sur série demande sa propre logique applicative.

### Transformer le récepteur en répondeur

Reprenez le programme unique de l'étape 8. Dans sa fonction `receive()`,
remplacez uniquement l'abonnement à `GUIDE_ALTITUDE` par :

```cpp
// Conserve l'abonnement jusqu'à sa destruction ; le lien associe chaque demande à sa réponse.
auto subscription = link.subscribeRequestAnswerer(
    dictionary.getDefinition("GUIDE_ALTITUDE_REQ"),
    [&](std::string sender, pprzlink::Message request) {
        (void)request; // Cette requête n'a aucun champ.
        std::cout << "Demande reçue de " << sender << std::endl;
        pprzlink::Message answer(dictionary.getDefinition("GUIDE_ALTITUDE"));
        answer.setSenderId(42);
        answer.setFieldSI("altitude", 123.5);
        received = true;
        // Retourne la réponse au lien, qui l'enverra avec la corrélation de cette requête.
        return answer;
    });
```

Le callback **retourne** le message de réponse. `IvyLink` l'envoie avec
l'identifiant de corrélation approprié ; vous n'appelez pas `sendMessage()`
pour cette réponse. Ce répondeur reste actif jusqu'à la temporisation du
programme, et peut traiter plusieurs demandes pendant cet intervalle.

### Transformer l'émetteur en demandeur

Dans le même `main.cpp`, ajoutez `<future>` et `<memory>` aux en-têtes.
Dans la fonction `emit()`, remplacez la construction et le remplissage du
message `altitude`, avant la construction du lien, par :

```cpp
// La promesse transmet le résultat du thread Ivy au thread principal.
// shared_ptr la maintient vivante si un callback est encore en cours après désabonnement.
auto result = std::make_shared<std::promise<double>>();
// Le future permet d'attendre ce résultat, puis de récupérer sa valeur ou son exception.
auto future = result->get_future();
pprzlink::Message request(dictionary.getDefinition("GUIDE_ALTITUDE_REQ"));
request.setSenderId(std::string("station-sol"));
```

Conservez la construction du lien et l'appel à `waitForReceiver(link)`.
Remplacez les deux lignes d'envoi `link.sendMessage(altitude)` et d'affichage
`Altitude publiée sur Ivy.` par :

```cpp
// Installe l'abonnement à la réponse avant l'envoi et renvoie son identifiant d'annulation.
const long pending = link.sendRequest(request,
    // Capture le shared_ptr par valeur : le callback possède une référence à la promesse.
    [result](std::string, pprzlink::Message answer) {
        try {
            // Rend le future prêt en lui fournissant l'altitude SI depuis le thread Ivy.
            result->set_value(answer.getFieldSI("altitude"));
        } catch (...) {
            // Rend aussi le future prêt en cas d'erreur ; future.get() relancera cette exception.
            result->set_exception(std::current_exception());
        }
    });

// Attend au plus trois secondes sur le thread principal ; la boucle Ivy tourne sur son propre thread.
if (future.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
    // Supprime l'attente de réponse, sans interrompre un callback qui aurait déjà commencé.
    link.UnbindMessage(pending);
    throw std::runtime_error("La requête est restée sans réponse");
}
// Consomme le résultat une seule fois ; une exception transmise par la promesse est relancée ici.
std::cout << "Altitude demandée : " << future.get() << " m\n";
```

Recompilez l'unique programme. Dans un terminal, `./build/atelier messages.xml r`
joue maintenant le répondeur ; dans l'autre, `./build/atelier messages.xml e`
joue le demandeur et doit afficher `Altitude demandée : 123.5 m`.

`sendRequest()` installe l'attente puis envoie la demande. À la première
réponse valide, cette attente est automatiquement supprimée. La bibliothèque
ne fixe pas de délai maximal : ici, le programme attend trois secondes puis
annule l'abonnement avec `UnbindMessage()` si nécessaire.

Le callback s'exécute sur le thread Ivy de l'émetteur. La paire
`std::promise` / `std::future` transmet le résultat au thread principal.
La capture par `shared_ptr` maintient la promesse en vie même si un callback
avait déjà commencé au moment de l'annulation. Se désabonner n'interrompt
pas une fonction de rappel déjà en cours.

**À essayer :** ne lancez pas le répondeur. Vous verrez d'abord échouer
l'attente de son abonnement. Pour observer le délai de réponse lui-même,
faites démarrer un participant `guide-recepteur` abonné à `GUIDE_ALTITUDE`
comme à l'étape 8 : il est découvert, mais ne répond pas aux requêtes.

## 10. Utiliser une radio XBee

Revenez au programme série de l'étape 7. Dans `CMakeLists.txt`, ajoutez `io`
aux composants demandés par `find_package()` et liez la cible **`atelier`**
à `pprzlink::io`. Pour un projet limité à cet exemple, le fichier complet est :

```cmake
cmake_minimum_required(VERSION 3.20)
project(atelier_pprzlink LANGUAGES CXX)

# io fournit BoostSerialPortDevice et inclut les fonctions de core, dont XbeeTransport.
find_package(pprzlink++ CONFIG REQUIRED COMPONENTS io)

add_executable(atelier main.cpp)
target_link_libraries(atelier PRIVATE pprzlink::io)
```

Si vous conservez d'autres exécutables utilisant Ivy, demandez
`COMPONENTS io ivy` et gardez leurs liens à `pprzlink::ivy`.
Lier une autre cible, par exemple `atelier_udp`, à `pprzlink::io` ne suffit
pas : chaque exécutable doit déclarer ses propres dépendances.

`BoostSerialPortDevice.h` déclare le port série ; son implémentation est
compilée dans `io`. Une cible liée uniquement à `core` ou `ivy` peut donc
compiler les appels, puis échouer à l'édition des liens avec des
« références indéfinies » vers le constructeur et les méthodes `setBaudrate()`,
`setDataBits()`, `setParity()`, `setStopBits()` et `setFlowcontrol()`.

Le `Device` reste un port série ; c'est le transport qui change pour produire
les trames de l'API XBee. L'API prise en charge utilise **AP=1**, sans
échappement.

Le même exécutable conserve les rôles `e` et `r` du chapitre 7, dans deux
processus reliés chacun à sa radio. L'exemple suivant concerne des radios
802.15.4. Ajoutez `<pprzlink/XbeeTransport.h>`, `<boost/asio/steady_timer.hpp>`,
`<chrono>`, `<functional>`, `<optional>` et `<variant>` aux en-têtes, puis remplacez la construction du
`PprzTransport` dans `main()` par :

```cpp
// Confie le port série au transport radio ; device devient vide après ce transfert de propriété.
pprzlink::XbeeTransport transport(std::move(device), dictionary);

pprzlink::XbeeConfiguration configuration;
// Les deux radios ont des adresses distinctes, choisies selon le rôle du processus.
configuration.localAddress = std::string_view(argv[3]) == "e" ? 0x0200 : 0x0100;
// Désactive la détection/changement de débit : le port doit déjà parler à la vitesse du modem.
configuration.targetBaudrate = std::nullopt;
// Suppose RTS et CTS câblés entre le convertisseur et le XBee ; false impose les deux côtés sans contrôle de flux.
configuration.hardwareFlowControl = true;
// Prépare le dialogue AT ; start() et les événements de context.run() le feront progresser.
transport.startInitialization(configuration);
```

Dans la signature de la fonction `receive()`, remplacez uniquement le type
`pprzlink::PprzTransport &` par `pprzlink::Transport &`. Cette interface de base
fournit les mêmes `bind()`, `start()` et `stop()` ; le corps de la fonction
reste utilisable avec la radio.

Remplacez la fonction `emit()` par cette émission périodique. L'exemple
suppose que RTS et CTS sont câblés ; sinon, choisissez
`configuration.hardwareFlowControl = false`. Le réglage série `none` hérité
de l'étape 7 sert seulement de préparation : l'initialisation XBee applique
ensuite le choix commun à la radio et au port.

### Envoyer trois messages dans la callback d'un timer

Le catalogue donne `link="broadcasted"` à `GUIDE_ALTITUDE`. Cet exemple lit
la définition pour choisir l'adresse radio `0xffff` et le destinataire
PPRZLINK 255. Dans cet atelier, les autres cas utilisent la radio `0x0100`.
Le broadcast ne demande aucun ACK MAC, avec les options normales du modem.

```cpp
// Un seul contexte traite le timer, les messages reçus et les statuts radio.
int emit(boost::asio::io_context &context, pprzlink::XbeeTransport &transport,
         const pprzlink::MessageDictionary &dictionary)
{
  const auto unicastReceiver = pprzlink::XbeeAddress::network(0x0100);
  boost::asio::steady_timer timer(context);
  boost::asio::signal_set signals(context, SIGINT, SIGTERM);
  double offset = 0;

  std::function<void(const boost::system::error_code &)> sendMessages;
  sendMessages = [&](const boost::system::error_code &error) {
    if (error || !transport.isRunning()) return;
    for (const double metres : {100.0, 110.0, 123.5}) {
      auto message = altitude(dictionary, metres + offset);
      const bool broadcast = message.getDefinition().getLinkMode() ==
                             pprzlink::MessageDefinition::LinkMode::Broadcasted;
      if (broadcast) message.setReceiverId(255);
      const auto receiver = broadcast ? pprzlink::XbeeAddress::network(0xffff)
                                      : unicastReceiver;
      const auto sent = transport.sendMessageTo(message, receiver);
      const auto frameId = transport.getLastFrameId();
      std::cout << "Envoyé : " << message.toString()
                << " (" << sent << " octets série, trame "
                << static_cast<unsigned>(frameId) << ')' << std::endl;
    }
    ++offset;
    // Rend la main à Asio entre les lots pour traiter les retours.
    timer.expires_after(std::chrono::milliseconds(1));
    timer.async_wait(sendMessages);
  };

  signals.async_wait([&](const boost::system::error_code &error, int) {
    if (!error) {
      timer.cancel();
      transport.stop();
    }
  });
  transport.bind(pprzlink::ALL, [](const pprzlink::Message &message) {
    std::cout << "Reçu : " << message.toString() << std::endl;
  });
  transport.setStatusCallback([](const pprzlink::XbeeTransport::RadioStatus &event) {
    if (const auto *tx = std::get_if<pprzlink::XbeeTransport::TransmitStatus>(&event)) {
      std::cout << "Statut TX : trame " << static_cast<unsigned>(tx->frameId)
                << ", code " << static_cast<unsigned>(tx->status)
                << " (0 = succès du modem)" << std::endl;
    }
  });
  // Attend la fin du dialogue AT avant d'armer le premier envoi.
  transport.onReady([&] {
    timer.expires_after(std::chrono::milliseconds(1));
    timer.async_wait(sendMessages);
    std::cout << "Émetteur prêt ; Ctrl+C pour arrêter" << std::endl;
  });
  transport.start();
  context.run();
  return 0;
}
```

Dans `main()`, l'appel du rôle émetteur devient :

```cpp
// Le rôle r conserve l'appel à receive(context, transport, argv[2]).
if (std::string_view(argv[3]) == "e") return emit(context, transport, dictionary);
```

Actualisez aussi l'aide du rôle `e` : il émet désormais des lots jusqu'à
Ctrl+C, avec un délai demandé de 1 ms entre deux lots.

Depuis le répertoire `projet`, reconfigurez et compilez après les modifications
du CMake et du programme :

```sh
# Recharge les composants du SDK et la liaison de la cible atelier.
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PPRZLINK_ATELIER/sdk"
cmake --build build
```

Lancez `./build/atelier messages.xml /dev/ttyUSB0 r`
et `./build/atelier messages.xml /dev/ttyUSB1 e` dans deux terminaux. Le rôle
`r` configure la radio à l'adresse `0x0100` et reçoit jusqu'à Ctrl+C ; le rôle
`e` configure sa radio à `0x0200`, puis répète les lots de trois altitudes
vers `0x0100` jusqu'à Ctrl+C. L'altitude de chaque lot est décalée d'un mètre.
Les deux radios doivent utiliser le même canal.

Il faut que la callback rende la main : un `while (true)` dans `onReady()`
empêcherait Asio de traiter les lectures série, les statuts TX et les timers.
Les variables utilisées par les callbacks restent ici vivantes pendant tout
`context.run()`. À l'arrêt, annulez **le timer et la réception** ; arrêter
seulement le transport laisserait le timer actif.

Le délai de **1 ms commence après chaque lot** : il ne garantit pas une
période exacte de 1 ms. Les écritures série restent synchrones et peuvent
attendre si les tampons sont pleins ou si CTS suspend l'émission. À 57600
bauds en 8N1, la liaison transporte au maximum 5760 octets par seconde ;
les trois trames TX16 de cet exemple représentent 51 octets, soit environ
8,9 ms sur le fil. Les tampons permettent seulement d'absorber une rafale.

Dans cette configuration, le modem doit déjà communiquer à la vitesse
configurée sur le port série, 57600 bauds dans notre programme.
`targetBaudrate = std::nullopt` conserve cette vitesse. L'initialisation
configure l'adresse locale et le mode API ; le canal reste inchangé lorsque
`configuration.channel` n'est pas renseigné. Le choix commun RTS/CTS est
appliqué même lorsque l'autobaud est désactivé.

`start()` active le dialogue AT ; l'arrivée des octets et les échéances Asio
vérifient les réponses et font avancer l'initialisation. Le rôle `e` effectue
ses envois dans le timer armé par `onReady()`, après cette préparation. Le
dialogue AT avance sur les données et ses échéances, sans boucle de polling.
Une erreur est signalée par `onError()` ou remontée dans la boucle.
Attendez la fin de cette phase avant d'envoyer
des messages, et laissez le transport seul utiliser le périphérique pendant
l'initialisation.

Si vous utilisez simplement `startInitialization()` sans configuration, le
comportement est différent : l'initialiseur détecte la vitesse, vise 57600
bauds et peut enregistrer les réglages courants avec `ATWR` lors d'un
changement de vitesse. Choisissez ce comportement intentionnellement.
Si le modem est déjà configuré, vous pouvez aussi omettre l'initialisation ;
dans ce cas `isReady()` suppose cette préparation, sans interroger le modem.

### Activer RTS/CTS sur le port et la radio ensemble

Si RTS et CTS sont câblés entre le convertisseur USB-série et le XBee, ajoutez
ce réglage avant `transport.startInitialization(configuration)` :

```cpp
configuration.hardwareFlowControl = true;
```

Cette option unique configure le contrôle de flux matériel des deux côtés :
le premier chiffre désigne le TTY hôte, le second la radio.

| `hardwareFlowControl` | TTY Linux | XBee | Paire finale |
| --- | --- | --- | --- |
| `false`, par défaut | Sans contrôle de flux | `D6=0`, `D7=0` | **00** |
| `true` | RTS/CTS matériel (`CRTSCTS`) | `D6=1`, `D7=1` | **11** |

L'API de configuration XBee ne propose pas deux réglages indépendants qui
permettraient de demander **10** ou **01**.
Les commandes `ATD6` et `ATD7` relisent les réglages de la radio ; après le
dernier `OK` de `ATCN`, la bibliothèque applique et vérifie le réglage du TTY
(`CRTSCTS` sous Linux). `onReady()` n'est appelé qu'après ces vérifications.

Ne réglez pas séparément `device->setFlowcontrol(...)` pour activer le XBee.
Dès que l'initialisation prend en charge le port, les modifications isolées
du contrôle de flux hôte sont refusées avec `std::logic_error`. Pour modifier
le choix, relancez `startInitialization()` avec la nouvelle valeur commune.
L'exemple `serial_messages -transport xbee -hfc` utilise cette même option ;
`-hfc` et `-xbee-no-init` sont incompatibles.

La phase AT désactive temporairement le contrôle de flux hôte pour éviter
qu'un CTS encore mal configuré bloque les commandes. Les deux périphériques
physiques ne changent pas simultanément : une erreur peut laisser des réglages
partiellement appliqués, mais bloque l'état prêt et tous les envois de messages.
Corrigez la cause, puis relancez l'initialisation. Aucun `ATWR` supplémentaire
n'est émis pour sauvegarder ces réglages `D6`/`D7`.
Une initialisation ultérieure qui change le débit peut toutefois enregistrer
les réglages laissés par la session précédente : `ATWR` sauvegarde tous les
paramètres courants, avant d'appliquer la nouvelle configuration de session.
Si vous omettez complètement l'initialisation avec un modem déjà configuré,
la bibliothèque ne peut pas vérifier cette cohérence.

Avec RTS/CTS activé, le pilote du convertisseur respecte CTS pour suspendre
les octets envoyés au XBee local ; RTS permet de réguler le sens inverse.
Ce mécanisme protège la liaison UART locale. Il ne constitue pas un
acquittement du correspondant radio distant.

### Lire l'état CTS local

Après le transfert du port au transport, vous pouvez toujours lire son état
via l'interface série empruntée au transport :

```cpp
const auto &serial = dynamic_cast<const pprzlink::SerialDevice &>(transport.getDevice());
const bool clearToSend = serial.getCTS();
```

`true` signifie que CTS est affirmé, `false` qu'il est désaffirmé. Le nom
`getCTS()` est aussi utilisé dans la bibliothèque C++
[`wjwwood/serial`](https://github.com/wjwwood/serial/blob/main/include/serial/serial.h).
Cette lecture interroge le pilote avec `TIOCMGET` sous Linux ; elle ne modifie
ni les commandes AT ni le réglage RTS/CTS. Elle reste disponible lorsque
l'initialisation XBee a réservé les modifications de contrôle de flux.

Un pilote qui ne fournit pas cet état provoque une exception
`boost::system::system_error`, avec le code natif (`ENOTTY` sur les
pseudo-terminaux Linux). Une fonctionnalité absente ne renvoie pas un faux
état « occupé ». `getHardwareFlowControl()` indique séparément si le contrôle
de flux est activé, et `transport.isReady()` si l'initialisation a abouti.
`getCTS()` n'attend pas que CTS change, mais partage le verrou des I/O série.
Son résultat est un instantané : il ne réserve pas de place pour l'écriture
suivante et ne garantit pas que celle-ci sera non bloquante.

Ces informations correspondent à des questions différentes :

| Appel ou événement | Information fournie |
| --- | --- |
| `transport.isReady()` / `onReady()` | Initialisation locale terminée ; le mode sans initialisation suppose une configuration préalable. |
| `serial.getHardwareFlowControl()` | Contrôle RTS/CTS activé dans la configuration hôte. |
| `serial.getCTS()` | CTS local affirmé ou désaffirmé, selon le pilote. |
| `TransmitStatus` | Résultat d'une tentative de transmission radio identifiée par `frameId`. |

`getCTS()` appartient à `SerialDevice`, implémenté ici par
`BoostSerialPortDevice`, et non à `XbeeTransport`. La référence obtenue par
`transport.getDevice()` reste valide pendant la vie du transport. Le
`unique_ptr device` d'origine est vide après `std::move(device)` ; utilisez
donc cette référence empruntée pour les lectures ultérieures.

Pour afficher le premier état puis ses changements dans le timer, déclarez
avant la lambda `sendMessages`, dans `emit()` :

```cpp
const auto &serial = dynamic_cast<const pprzlink::SerialDevice &>(transport.getDevice());
std::optional<bool> previousCts;
```

Puis, après le contrôle d'erreur au début de `sendMessages` :

```cpp
const bool cts = serial.getCTS();
if (!previousCts || *previousCts != cts) {
  previousCts = cts;
  std::cout << (cts ? "CTS" : "BUSY") << std::endl;
}
```

Les parenthèses font porter l'affichage sur le résultat du ternaire. Le
dernier état reste hors de la callback et survit à ses invocations. Une
exception de lecture remonte ici vers le `catch` de `main()` ; elle ne doit
pas être convertie en `false`. Pour un essai sur pseudo-terminal, gardez le
timer sans ce bloc de surveillance : les drapeaux de configuration du TTY
peuvent être testés même si le terminal ne fournit aucun signal CTS physique.

### Choisir les réessais radio pour chaque envoi

Le dernier argument des méthodes XBee peut recevoir l'enum fortement typé
`XbeeTransport::RadioRetries` :

```cpp
using RadioRetries = pprzlink::XbeeTransport::RadioRetries;
const auto receiver = pprzlink::XbeeAddress::network(0x0100);

pprzlink::Message setting(dictionary.getDefinition("GUIDE_SETTING"));
setting.setSenderId(0);
setting.setReceiverId(42);
setting.setField("ac_id", 42, "value", 12.5);
// Demande les mécanismes normaux du modem pour une commande importante.
transport.sendMessageTo(setting, receiver, RadioRetries::Enabled);

const auto telemetry = altitude(dictionary, 123.5);
// Une altitude ou position périssable peut être envoyée sans ACK/réessais MAC.
transport.sendMessageTo(telemetry, receiver, RadioRetries::Disabled);
```

Ces appels ont lieu après l'initialisation, par exemple dans une callback.
Omettre le dernier argument équivaut à `RadioRetries::Enabled` :
`transport.sendMessageTo(setting, receiver)` conserve les options normales du modem.

| Valeur de l'enum | Options de la trame | Effet en 802.15.4 legacy |
| --- | --- | --- |
| `RadioRetries::Enabled`, par défaut | `0x00` | Mode normal du modem ; ACK et réessais MAC en unicast selon sa configuration. |
| `RadioRetries::Disabled` | `0x01`, bit « Disable MAC ACK » | Désactive l'ACK MAC pour cet envoi et les réessais associés. |

Il s'agit d'un `enum class`, pas d'un booléen. Ce choix est propre à la trame,
sans modifier la configuration globale du modem. En 802.15.4, il agit sur
les ACK et réessais MAC ; il ne reconfigure pas les autres couches de réessais
(`MM`/`RR`). Les trames 868 `0x10` utilisent également le bit « Disable ACK ».
En particulier, `Enabled` conserve le comportement antérieur, sans forcer
des mécanismes que la configuration globale du modem aurait désactivés.
Voir le [format TX16 de Digi](https://docs.digi.com/resources/documentation/digidocs/90001500/reference/r_frame_0x01.htm).

Une destination radio broadcast n'exige aucun ACK MAC, même avec les options
normales `0x00`. Pour les commandes Paparazzi, l'attribut XML existant
`link="broadcasted"` permet à `link++` de choisir cette diffusion ; les
commandes ciblées conservent leur champ `ac_id` pour le filtrage dans l'autopilote.
La [section link++](#utiliser-link-avec-une-radio-xbee) détaille cette convention.
Le choix explicite `Disabled` reste utile pour un envoi unicast sans ACK.

Le même argument final est accepté par `sendMessageWithId()` et les anciennes
méthodes d'adressage conservées pour compatibilité. Pour l'adressage déduit
du destinataire PPRZLINK, utilisez
`sendMessage(message, RadioRetries::Disabled)` ; l'appel historique
`sendMessage(message)` utilise `Enabled`, y compris via `Transport&`.
Le type est déclaré dans `<pprzlink/RadioRetries.h>`, avec l'alias
`XbeeTransport::RadioRetries`.

La bibliothèque n'ajoute aucune réémission côté PC. Le retour de statut radio
reste disponible tant que le `frameId` est non nul, mais un statut « OK » sans
ACK ne prouve pas que le destinataire a reçu le message. Le choix `Disabled`
sur un envoi ne change pas le comportement par défaut des envois suivants.

Le paramètre ne crée ni file de réémission côté PC, ni durée de validité des
données, ni remplacement automatique par une mesure plus récente. Si une
position vieillit avant son envoi, c'est à l'application de la remplacer ou
de l'abandonner. Après épuisement des essais internes du modem, une commande
importante peut elle aussi échouer : le statut final doit être traité.
Mettre `frameId` à zéro supprime seulement le retour de statut, indépendamment
du choix `RadioRetries`.

### Choisir la destination radio

L'envoi habituel utilise le destinataire PPRZLINK comme adresse radio.
Pour essayer une destination unicast explicite dans la boucle de `emit()`,
remplacez le bloc de construction, de sélection XML et d'envoi par :

```cpp
// Destinataire logique de l'en-tête, indépendant de l'adresse radio passée à l'envoi.
auto message = altitude(dictionary, metres);
message.setReceiverId(0);
// À appeler lorsque le modem est prêt ; 0x0100 identifie la radio distante.
const auto receiver = pprzlink::XbeeAddress::network(0x0100);
const auto sent = transport.sendMessageTo(message, receiver);
```

Ici, 0 est le destinataire logique dans l'en-tête du message et `0x0100`
l'adresse de la radio distante. Adaptez celle-ci à votre installation.
`XbeeAddress::network()` désigne une adresse courte `MY`, et
`XbeeAddress::ieee()` une adresse matérielle `SH/SL` :

```cpp
const auto receiver = pprzlink::XbeeAddress::ieee(0x0013a200405291ab);
transport.sendMessageTo(message, receiver);
```

Le type d'adresse dépend de la fabrique choisie : même `ieee(0x0100)` utilise
le format IEEE. Les littéraux n'exigent ni cast ni suffixe particulier.
Les valeurs négatives et les adresses `network()` dépassant `0xffff` sont
refusées à la construction avec `std::out_of_range`, avant toute troncature.
Les valeurs flottantes et booléennes ne sont pas acceptées. Les anciennes
méthodes `sendMessageTo16()` et `sendMessageTo64()` restent disponibles.
Le type est inclus par `XbeeTransport.h` et peut aussi être utilisé seul avec
`<pprzlink/XbeeAddress.h>`. `kind()` et `value()` permettent d'inspecter son
format et sa valeur. Une valeur IEEE doit tenir sur 64 bits.

La variante `XbeeTransport::Api::Series868`, passée en troisième argument au
constructeur, sélectionne les trames étendues et requiert une adresse `ieee()`.
Elle ne configure pas à elle seule un modem 868 MHz. Les essais logiciels
de cette branche couvrent ces
trames ; la validation sur matériel 868 reste à effectuer, comme indiqué
dans le [rapport de validation](validation/LINK_CPP_VALIDATION.md).

### Distinguer écriture série et livraison radio

La valeur renvoyée par `sendMessage()` est un nombre d'octets écrits vers
le modem. `emit()` installe déjà un callback avec `setStatusCallback()` pour
recevoir le résultat radio. Pour le personnaliser, remplacez son callback
par celui-ci ; dans un autre programme, installez-le avant les envois :

```cpp
// Installe l'observateur avant l'envoi ; le traitement de la réception lui remettra les statuts radio.
transport.setStatusCallback([](const pprzlink::XbeeTransport::RadioStatus &event) {
    // Le variant peut contenir plusieurs types de statut ; get_if renvoie nullptr pour les autres.
    if (const auto *status =
            std::get_if<pprzlink::XbeeTransport::TransmitStatus>(&event)) {
        // Les casts affichent les octets comme des nombres, plutôt que comme des caractères.
        std::cout << "Trame radio " << static_cast<unsigned>(status->frameId)
                  << ", statut " << static_cast<unsigned>(status->status) << '\n';
    }
});
```

Dans la boucle de `emit()`, l'identifiant est relevé immédiatement après
l'envoi, avant de présenter une autre trame au transport :

```cpp
// Associe cet envoi au futur TransmitStatus portant le même frameId.
const auto frameId = transport.getLastFrameId();
std::cout << "Identifiant radio envoyé : " << static_cast<unsigned>(frameId) << '\n';
```

Activez la réception avec `start()` et faites tourner Asio avec `context.run()`.
Le traitement de la réception déclenche aussi les callbacks de statut, même
si aucun message applicatif n'est rendu. Pour le format 802.15.4 legacy,
les principaux codes de `TransmitStatus::status` sont :

| Code | Sens |
| --- | --- |
| `0x00` | Succès rapporté par le modem. |
| `0x01` | Aucun ACK reçu. |
| `0x02` | Échec de l'accès au canal radio (CCA). |

En unicast avec les ACK effectivement activés, un succès confirme la réception
d'un ACK du XBee destinataire. Avec `RadioRetries::Disabled`, ou en broadcast,
un statut « OK » ne prouve pas la réception distante. Dans tous les cas,
il ne confirme pas que le programme derrière cette radio a lu le message
PPRZLINK ou exécuté une commande : cela demande une réponse applicative.
Voir les [statuts TX documentés par Digi](https://docs.digi.com/resources/documentation/digidocs/90001500/reference/r_frame_0x89.htm).

Pour suivre plusieurs envois, associez chaque statut à son `frameId`.
Les identifiants automatiques parcourent 1 à 255. Une application avancée
peut les gérer elle-même avec `sendMessageWithId()` : elle doit conserver
les identifiants en attente et éviter de les réutiliser prématurément. La
valeur 0 désactive la réponse de statut. Le transport ne gère pas les
réessais applicatifs ; définir un délai et décider d'une retransmission
relèvent du programme utilisateur.

`sendMessageWithId()` déduit la destination radio du destinataire PPRZLINK.
Pour choisir aussi une adresse radio indépendante, utilisez `sendMessageToWithId()` :

```cpp
const auto radio = pprzlink::XbeeAddress::ieee(0x0013a200405291ab);
// L'application réserve l'identifiant 42 jusqu'au statut ou à son propre délai.
transport.sendMessageToWithId(message, radio, 42);
// ID nul et désactivation des ACK sont deux choix indépendants.
transport.sendMessageToWithId(message, radio, 0, pprzlink::RadioRetries::Disabled);
```

Le destinataire dans l'en-tête PPRZLINK reste inchangé. Le choix de réessais
par défaut est `RadioRetries::Enabled`. Les adresses `network()` conviennent
au mode legacy ; le mode `Series868` requiert `ieee()`.
`sendMessageTo()` reste disponible lorsque l'ID doit être attribué automatiquement.

L'identifiant radio n'est ni l'identifiant XML du message ni un compteur
illimité. Le compteur automatique peut boucler ; il ne réserve pas les
identifiants pour votre table d'envois en attente. Pour une corrélation durable,
gérez leur allocation sans réutiliser un identifiant encore attendu. Une
absence d'ACK ne prouve pas à elle seule que la trame n'a pas été reçue :
l'ACK peut avoir été perdu. Enfin, `TransmitStatus::retries` est renseigné
pour les statuts étendus `0x8b` ; le format legacy `0x89` ne fournit pas ce
compteur et la valeur zéro du champ C++ ne compte donc pas ses réessais réels.

Les messages radio reçus portent aussi des métadonnées. Pour les afficher,
remplacez le binding `ALL` de `emit()` par :

```cpp
// La seconde référence apporte les métadonnées de cette réception ; elle est empruntée pour l'appel.
transport.bind(pprzlink::ALL,
    [](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
        std::cout << message.toString() << '\n';
        // L'optional doit contenir des informations radio avant d'être déréférencé.
        if (info.xbee) {
            const auto &radio = *info.xbee;
            std::cout << "Adresse radio source : " << radio.sourceAddress << '\n';
            // Certaines variantes n'ont pas de RSSI ; le cast signé précède le changement de signe.
            if (radio.hasRssi) std::cout << "RSSI : " << -static_cast<int>(radio.rssi) << " dBm\n";
        }
    });
```

L'adresse radio source et l'identifiant d'expéditeur PPRZLINK décrivent deux
niveaux d'adressage. Le RSSI n'est pas présent dans toutes les variantes,
d'où le contrôle de `hasRssi`. La charge utile RF acceptée par ce transport
est limitée à 100 octets, en-tête PPRZLINK compris.

### Utiliser link++ avec une radio XBee

L'agent [link++](https://github.com/alex31/pprzlink/tree/FEATURE/reactive_reception/lib/v2.0/C%2B%2B/apps/link) relie le bus Ivy aux avions, applique les règles
`link="forwarded"` et `link="broadcasted"`, et suit leur présence. Il utilise
les mêmes `MessageDefinition` et `XbeeTransport` que votre application.
Il attend le catalogue Paparazzi complet, avec ses classes `telemetry`,
`datalink` et `ground`, et les définitions utilisées par le routage et les
rapports. Utilisez le catalogue de votre configuration ; le petit XML
`GUIDE_*` de cet atelier ne remplit pas ce rôle.

Après l'installation d'Ivy décrite à l'étape 8, construisez l'agent depuis
les sources actuelles de pprzlink :

```sh
cmake -S "$PPRZLINK_CPP" -B "$PPRZLINK_ATELIER/build-link" \
  -DCMAKE_CXX_COMPILER=g++ \
  -DPPRZLINK_WITH_IVY=ON \
  -DPPRZLINK_BUILD_LINK=ON \
  -DPPRZLINK_BUILD_EXAMPLES=OFF \
  -DBUILD_TESTING=OFF
cmake --build "$PPRZLINK_ATELIER/build-link" --target link++ -j4
"$PPRZLINK_ATELIER/build-link/apps/link/link++" --help
```

Dans une arborescence Paparazzi, vous pouvez aussi reconstruire le binaire
utilisé par les outils sol, depuis la racine de Paparazzi :

```sh
make -C sw/ground_segment/tmtc -f link_cpp.mk PAPARAZZI_SRC="$PWD" \
  PPRZLINK_CPP_BUILD="$PWD/var/build/pprzlink-cpp-ubuntu26"
```

Cette commande reconstruit la bibliothèque et l'agent ensemble à partir du
sous-module présent, puis copie l'exécutable dans
`sw/ground_segment/tmtc/link++`.
Elle utilise le compilateur système par défaut ; `PPRZLINK_CXX` permet de le
choisir explicitement. Les composants `pprzlink::io` et `pprzlink::ivy` sont liés
statiquement à l'agent : l'exécutable copié est indépendant du répertoire de
construction pprzlink. Il utilise encore les bibliothèques partagées système
Ivy et TinyXML2, ainsi que la bibliothèque d'exécution C++.

Dans `LINK_REPORT`, le temps aller-retour est mesuré avec une horloge monotone.
Il vaut `9999 ms` avant le premier PONG associé à un PING, puis conserve la
dernière mesure terminée pendant l'attente des réponses suivantes. Un PONG sans
PING en attente ne modifie pas cette mesure. L'agent ne crée une attente que
pour un aéronef encore actif auquel il a envoyé le PING. Le protocole ne contient
pas de numéro de séquence : un PONG très retardé peut être associé à un PING
plus récent.

Pour lancer ce binaire depuis la racine de Paparazzi, avec RTS/CTS activé :

```sh
export PAPARAZZI_HOME="$PWD"
./sw/ground_segment/tmtc/link++ \
  -transport xbee -d /dev/ttyUSB0 -s 57600 -hfc
```

`-hfc` est un commutateur sans argument. En mode XBee, il active ensemble le
RTS/CTS du TTY Linux et les fonctions `D6=1`, `D7=1` du modem. Sans `-hfc`,
les deux côtés sont désactivés (`D6=0`, `D7=0`, sans `CRTSCTS`). Les deux fils
RTS/CTS doivent être câblés pour les activer. En mode `-transport pprz`,
`-hfc` configure seulement le port série.

La préparation AT vérifie le modem et le TTY avant d'annoncer
`XBee ready: RTS/CTS enabled on host and modem`. L'agent conserve le débit
fixe demandé par `-s` : il ne lance ni changement de débit ni `ATWR`.
Dans Paparazzi Center, ajoutez `-hfc` aux arguments de lancement de `link++`
pour obtenir le même réglage.

Les messages sont chargés depuis `$PPRZLINK_DIR/messages.xml` si cette variable
est définie, sinon depuis `$PAPARAZZI_HOME/var/messages.xml`, sinon depuis
`/usr/share/pprzlink/messages.xml`. Après modification du catalogue, relancez
l'agent. Un `PPRZLINK_DIR` conservé d'un autre essai prend priorité sur
`PAPARAZZI_HOME`.

Le choix par type de message utilise l'attribut XML `link` déjà présent dans
Paparazzi. `MessageDefinition::getLinkMode()` l'expose ; `link++` choisit la
destination puis `XbeeTransport` construit la trame radio correspondante.

| Attribut sur `<message>` | Comportement radio | Réémission par link++ |
| --- | --- | --- |
| `link="forwarded"` | Unicast vers l'avion désigné par `ac_id` ; ACK MAC selon la configuration du modem. | Autorisée sur erreur d'ACK, selon `-xbee_retries`. |
| `link="broadcasted"` | Diffusion radio à `0xffff`, sans ACK MAC ; les champs du message restent intacts. | Aucun nouvel envoi après un échec radio. |
| Absent | Pas de routage automatique d'une commande Ivy ordinaire. | Les envois internes, tels que PING, gardent leur routage propre. |

Une diffusion radio peut viser logiquement un seul avion. Par exemple, en
conservant les champs habituels de `SETTING` :

```xml
<message name="SETTING" id="4" link="broadcasted">
  <field name="index" type="uint8"/>
  <field name="ac_id" type="uint8"/>
  <field name="value" type="float"/>
</message>
```

La commande Ivy `ground SETTING 3 42 12.5` est alors diffusée avec le
`receiverId` PPRZLINK à 255 et l'adresse radio `0xffff`, tandis que le champ
`ac_id` vaut toujours 42. Le gestionnaire `SETTING` de l'autopilote vérifie
`ac_id == AC_ID` avant d'appliquer le réglage. Les autres avions rejettent
cette commande. Le routage radio et le destinataire applicatif restent ainsi
distincts, en suivant la convention Paparazzi existante.

`-xbee_retries 3` autorise au maximum trois tentatives côté PC, premier envoi
compris, pour les envois unicast. La valeur par défaut est 10 ;
`-xbee_retries 1` supprime les réémissions côté PC. Cette option ne réactive
jamais les réémissions de broadcasts. La bibliothèque conserve les options
radio normales `0x00` dans l'agent : l'adresse broadcast suffit à supprimer
les ACK MAC.

L'absence d'ACK ne garantit pas une émission radio unique : certains firmwares
802.15.4 répètent les broadcasts selon le paramètre global `RR`. `link++`
ne modifie pas ce paramètre. Voir [RR dans la documentation Digi](https://docs.digi.com/resources/documentation/digidocs/90001500/reference/r_cmd_rr.htm).

Les statuts TX restent demandés et leurs identifiants sont suivis aussi pour
les broadcasts. Un statut absent expire après cinq secondes sans réémission
automatique ; un statut « OK » sans ACK ne prouve pas la réception distante.
Lorsque les 255 identifiants sont occupés, un nouvel envoi est refusé avec un
diagnostic sur stderr. L'agent poursuit son fonctionnement et garde les
échéances des transmissions en cours, même si de nouvelles commandes arrivent.
L'envoi refusé n'est pas mis en file ; les envois suivants peuvent reprendre
dès qu'un statut ou une expiration libère un identifiant. Un PING refusé ne
démarre aucune nouvelle mesure RTT ; le timer des PING périodiques reste actif.
La bibliothèque seule ne crée aucune réémission côté PC : cette politique
supplémentaire appartient à l'agent `link++`.

En mode UDP, une commande diffusée est normalement envoyée à chaque avion
vivant. Avec `-udp_broadcast`, `link++` émet un seul datagramme vers
`-udp_broadcast_addr` et `-udp_uplink_port`, dès lors qu'au moins un avion est
vivant. Découvrir plusieurs avions ne multiplie pas ce broadcast réseau ;
sans avion vivant, aucun datagramme n'est émis.

## 11. Construire un outil générique ou un adaptateur

### Afficher un message dont on ne connaît pas les champs à l'avance

Un enregistreur ou une interface de diagnostic peut parcourir la définition
au lieu de coder le nom de chaque champ. Revenez au programme sans I/O de
l'étape 2, ajoutez `<pprzlink/TextCodec.h>`, puis :

```cpp
// Emprunte le schéma du message ; il fournit les types et l'ordre XML, pas les valeurs.
const auto &schema = altitude.getDefinition();
for (std::size_t i = 0; i < schema.getNbFields(); ++i) {
    const auto &field = schema.getField(i);
    std::cout << field.getName() << " (" << field.getType().toString() << ") = ";
    // Le formateur traite le variant natif sans imposer de type C++ ni de conversion SI.
    pprzlink::writeDebugField(std::cout, altitude.getRawValue(i));
    std::cout << '\n';
}
```

Les indices suivent l'ordre du XML. Ce traitement fonctionne aussi avec un
message reçu d'un transport, quel que soit son nom, dès lors que tous ses
champs ont une valeur.

Pour découvrir les messages d'une classe, vous pouvez aussi écrire :

```cpp
// getMsgsForClass crée un vecteur de définitions ; la boucle emprunte ses éléments.
// Ce vecteur temporaire reste vivant jusqu'à la fin de la boucle.
for (const auto &schema : dictionary.getMsgsForClass("guide")) {
    std::cout << schema.getName() << " : " << schema.getNbFields() << " champs\n";
}
```

Si votre traitement doit examiner le type C++ de chaque valeur,
`message.getField(nom)` sans paramètre de template fournit une référence
constante au `std::variant` de stockage, utilisable avec `std::visit`.
Cette référence appartient au message : ne la conservez pas après sa
destruction ou le remplacement du champ.

Les métadonnées chargées comprennent les noms, types, identifiants, le
format d'affichage, le mode de routage `link`, les unités et leurs coefficients
explicites. `field.getUnit()` fournit l'unité XML et `field.getSIUnit()` celle
de l'accès SI. Les descriptions textuelles ne sont pas exposées. Les accès
`getFieldSI()`, `getFieldArraySI()` et `setFieldSI()` demandent une conversion d'unité ; les appels
natifs et les codecs continuent à utiliser les valeurs XML.

### Brancher un autre flux d'octets

Supposons que votre programme possède déjà un flux TCP ou un canal de
simulation. Écrivez une classe dérivée de `Device`, puis confiez-la à
`PprzTransport`. Le transport prend en charge l'encodage et le décodage ;
votre application reçoit toujours les messages avec `bind()`.

Pour une simulation en mémoire, utilisez `MemoryDevice`, comme à
[l'étape 5](#5-encoder-puis-décoder-sans-matériel). Ses écritures sont bouclées
en entrée. `MemoryDevice::feed()` permet aussi d'injecter des octets dans le
flux, sans avoir à écrire un adaptateur.

Un adaptateur réel implémente `getExecutor()`, `startReception()`,
`stopReception()` et `setReceiveCallback()`. Ses notifications sont appelées
hors des verrous d'I/O et les buffers survivent aux lectures annulées.
`readAll()` consomme les octets disponibles ; `writeBuffer()` écrit le tampon
entier ou lève une exception. `MemoryDevice` permet d'apprendre la réception
sans avoir à écrire cette mécanique.

Pour les fichiers ou FIFO sous Unix, la bibliothèque fournit déjà
`PosixFileDevice`, dans le composant `io`. Pour changer la vitesse UART pendant
une initialisation XBee, un adaptateur doit implémenter l'interface plus
spécifique `SerialDevice` et sa méthode `resetBaudrate()`. L'initialisation XBee
utilise aussi `applyHardwareFlowControl()` et `readHardwareFlowControl()` pour
configurer et vérifier le contrôle RTS/CTS du port hôte.
La lecture `getCTS()` est une capacité optionnelle : son implémentation par
défaut signale `operation_not_supported`. Un adaptateur qui la propose doit
renvoyer l'état logique du signal et propager ses erreurs de lecture.

Pour `PosixFileDevice`, un échec d'ouverture POSIX lève `std::system_error` ;
un échec d'affectation du descripteur à Asio lève
`boost::system::system_error`. Les erreurs de lecture, y compris la fin de flux
(EOF), sont signalées après consommation des octets déjà tamponnés par
`boost::system::system_error`, depuis `availableBytes()`, `readAll()` ou
`startReception()`. Les erreurs d'écriture ou de `poll()` utilisent
`std::system_error` ; un délai dépassé ou une indisponibilité terminale
signalée par `poll()` utilise `std::runtime_error`.

`PprzTransport` et `XbeeTransport` dérivent tous deux de `Transport` : un
programme peut choisir l'un ou l'autre avec un `std::unique_ptr<Transport>`.
`UdpTransport` et `IvyLink` ont leurs propres opérations d’envoi, mais les
quatre modes partagent `Receiver`, `bind()`, `start()` et `stop()`.

## 12. Organiser une application durable et diagnostiquer les erreurs

### Respecter la durée de vie des objets

Le modèle utilisé dans les exemples est simple : créez le dictionnaire,
puis le contexte d'I/O si nécessaire, puis les objets de communication.
La destruction se fera dans l'ordre inverse.

| Objet | Ce qu'il conserve | Conséquence pratique |
| --- | --- | --- |
| `Message` | Sa propre définition et ses valeurs | Il peut être copié et conservé après la réception. |
| Transports et `IvyLink` | Une référence au dictionnaire | Le dictionnaire doit rester vivant pendant leur utilisation. |
| `PprzTransport`, `XbeeTransport` | La propriété exclusive du `Device` | Configurez le périphérique avant `std::move`. |
| Périphérique série, `MemoryDevice` et transport UDP | Le contexte Asio fourni | Détruisez-les avant le contexte. |
| Abonnement Ivy | Le callback et ses captures | Gardez l'abonnement actif, et ses données capturées valides. |

Pour les transports binaires, effectuez les appels à un même transport
depuis un seul thread, ou sérialisez-les explicitement. Le fait que le
périphérique série protège ses propres opérations ne rend pas le transport
entier utilisable simultanément sans coordination.

Avec Ivy en mode threadé, les callbacks exécutent sur le thread de la boucle
Ivy. Utilisez une file, un mutex ou une promesse pour communiquer avec le
reste de votre programme. Déclarez l'état capturé avant le lien pour qu'il
lui survive. Arrêtez les autres appelants avant de détruire le lien ; ne
détruisez jamais le lien depuis l'un de ses callbacks. `stop()` peut en
revanche y être appelé.

En mode `run()`, une erreur de callback est contrôlée lorsque la boucle rend
la main. En mode threadé, inspectez aussi `getBus().take_callback_error()`
pour détecter ces erreurs ; ce résultat suit le contrat d'erreur natif Ivy.

La réception réactive d'`IvyLink`, activée par `start()`, ignore le trafic dont
le nom de message apparent est absent du dictionnaire. Un message connu dont
les champs sont malformés reste signalé à `onError()` comme erreur `Decode`.
Les requêtes et réponses conservent leurs abonnements propres.

### Séparer le traitement métier de la réception

Après les premiers essais, extrayez le traitement dans une fonction. Celle-ci
reçoit déjà un message complet et peut être essayée avec un message construit
en mémoire :

```cpp
void traiterAltitude(const pprzlink::Message &message)
{
    // Vérifie le schéma avant de demander un champ qui pourrait être absent d'un autre message.
    if (message.getDefinition().getName() != "GUIDE_ALTITUDE") return;
    const double metres = message.getFieldSI("altitude");
    std::cout << "Altitude traitée : " << metres << " m\n";
}
```

Appelez-la avec `message` dans un callback UDP, série ou Ivy. Le calcul métier
n'a ainsi pas besoin de connaître le transport. Placez cette fonction avant
`main()` si vous l'ajoutez à l'un des programmes du guide.

### Comprendre ce que signale une erreur

Les exemples terminent le programme lorsqu'une exception remonte jusqu'à
`main()`. Dans un service continu, vous choisirez quelles erreurs de contenu
peuvent être journalisées avant de poursuivre et quelles erreurs d'I/O
nécessitent une reconnexion ou un arrêt.

| Symptôme | Première vérification |
| --- | --- |
| Fichier XML introuvable | Le chemin est interprété depuis le répertoire de lancement du programme. |
| `no_such_message` ou `no_such_class` | Le XML chargé contient-il les noms et identifiants attendus ? |
| `no_such_field` | Le nom du champ correspond-il exactement à celui du XML ? |
| `field_has_no_value` | Tous les champs ont-ils été remplis avant lecture ou sérialisation ? |
| `field_type_mismatch` | Le type de `getField<T>()` correspond-il au XML ? Faut-il `getFieldAs<T>()` ? |
| `field_conversion_error` | La valeur tient-elle dans le type XML après conversion et, pour un setter SI entier, arrondi ? Le setter natif refuse les fractions. |
| `field_unit_error` | `canConvertSI()` est-il vrai ? Vérifier les unités/coefficient XML et la correspondance dans `UnitAliases.cpp`. |
| `std::length_error` | Taille de tableau fixe incorrecte, tableau trop long ou message trop grand ? |
| Aucun callback de réception série | Le contexte Asio tourne-t-il ? Le correspondant émet-il, avec le bon débit et le bon protocole ? |
| Rien ne déclenche le callback Ivy | Même domaine Ivy, abonnement encore vivant, boucle active et publication après découverte ? |
| Requête Ivy sans réponse | Le répondeur est-il abonné à la définition `_REQ` et retourne-t-il le bon type de réponse ? |
| XBee écrit des octets mais rien n'arrive | Destination radio correcte, modem en AP=1, statuts TX traités ? |
| `hardwareFlowControl`, `RadioRetries` ou `XbeeAddress` inconnus à la compilation | Le SDK actif contient-il ces API ? Reconstruire et réinstaller la bibliothèque, puis vérifier `pprzlink++_DIR` dans le cache CMake. |
| Référence indéfinie vers une nouvelle méthode XBee | Les en-têtes et archives viennent-ils de la même version du SDK ? |
| `getCTS()` provoque `ENOTTY` | Un pseudo-terminal ou ce pilote ne fournit pas les signaux modem ; ce n'est pas un état CTS désaffirmé. |
| Un setter de contrôle de flux lève `std::logic_error` après initialisation XBee | Reconfigurer les deux côtés avec `XbeeConfiguration::hardwareFlowControl`, puis relancer `startInitialization()`. |
| Les retours cessent pendant l'émission | La callback rend-elle la main ? Une écriture synchrone attend-elle les tampons ou CTS ? |
| « Trame OK » sans preuve de réception | L'envoi est-il unicast avec ACK activés, ou sans ACK/en broadcast ? |

Toutes les erreurs ne passent pas par `pprzlink_exception` : les erreurs de
conversion, de type et de longueur ont d'autres bases standard. Le
`catch (const std::exception &)` des exemples couvre ces familles ; dans
votre application, utilisez des captures plus précises lorsque le traitement
diffère.

Le décodeur PPRZ élimine les octets parasites et les trames dont la longueur
ou le checksum est invalide. Un contenu invalide dans une trame correctement
délimitée peut lever une exception après consommation de cette trame. Un
appel ultérieur peut alors poursuivre la lecture.

Pour observer ce qui se passe, utilisez les statistiques du décodeur ou du
transport. Cet extrait suppose un objet `transport` binaire existant :

```cpp
// Emprunte les compteurs cumulés : leur valeur évolue avec les prochaines réceptions.
const auto &stats = transport.getStatistics();
std::cout << "Messages reçus : " << stats.receivedMessages
          << ", erreurs de checksum : " << stats.checksumErrors
          << ", erreurs de décodage : " << stats.decodingErrors << '\n';
```

Des checksums corrects ne garantissent pas que deux XML donnent le même sens
aux octets : un ordre de champs différent peut produire des valeurs erronées
sans modifier la longueur de trame. Vérifiez toujours la compatibilité des
définitions aux deux extrémités.

### Relier l'atelier à une application Paparazzi

Pour utiliser ces acquis dans votre projet, remplacez `messages.xml` par le
catalogue de votre configuration et choisissez les noms de messages qui y
existent. Le catalogue source du dépôt se trouve dans
[message_definitions/v1.0/messages.xml](../../message_definitions/v1.0/messages.xml) ;
le `v1.0` de ce chemin désigne le catalogue et ne sélectionne pas le protocole
de transport utilisé par la bibliothèque C++.

Les règles de routage vers les avions, le suivi de présence, les délais de
réponse et les réessais appartiennent à l'application. L'agent
[link++](https://github.com/alex31/pprzlink/tree/FEATURE/reactive_reception/lib/v2.0/C%2B%2B/apps/link) montre une application complète qui relie les transports
au bus Ivy et applique ces règles.

Pour poursuivre avec des exemples déjà intégrés au dépôt :

- [IvyRoundTrip.cpp](../../lib/v2.0/C++/pprzlink/examples/IvyRoundTrip.cpp) échange une altitude
  entre deux liens Ivy et attend la découverte de l'abonnement.
- [SerialAircraft.cpp](../../lib/v2.0/C++/pprzlink/examples/clients/SerialAircraft.cpp) choisit le
  rôle client ou serveur pour échanger PING, une altitude et PONG sur série.
- [PtyPeer.cpp](../../lib/v2.0/C++/pprzlink/examples/clients/PtyPeer.cpp) choisit le client demandeur
  ou le serveur répondeur sur une liaison série physique ou virtuelle.
- [UdpMessages.cpp](../../lib/v2.0/C++/pprzlink/examples/clients/UdpMessages.cpp) choisit l'émission
  d'une commande UDP ou la réception avec affichage de l'origine réseau.

Pour ce dernier exemple, utilisez le catalogue `client_messages.xml` et le
même exécutable dans les deux terminaux :

```sh
# Depuis la racine du dépôt Paparazzi, compile le programme UDP avec ses rôles e/r.
cmake --build var/build/pprzlink-pty-ubuntu26 --target udp_messages
```

Dans le premier terminal :

```sh
# Le rôle r attend un message sur le port UDP local 4242 pendant dix secondes au maximum.
./var/build/pprzlink-pty-ubuntu26/udp_messages \
  sw/ext/pprzlink/lib/v2.0/C++/pprzlink/examples/clients/client_messages.xml 4242 r
```

Dans le second terminal :

```sh
# Le rôle e remplit tous les champs de CLIENT_SETTING, puis envoie une commande au port 4242.
./var/build/pprzlink-pty-ubuntu26/udp_messages \
  sw/ext/pprzlink/lib/v2.0/C++/pprzlink/examples/clients/client_messages.xml 4242 e
```

Le récepteur affiche l'adresse et le port source, la taille de la trame et
les champs du message, puis termine. L'émetteur envoie une commande pour
l'avion 42 avec `value=12.5` et termine après l'écriture du datagramme.
Le `messages.xml` de l'atelier utilise des noms `GUIDE_*` ; ce programme UDP
attend les définitions `CLIENT_*` de son catalogue d'exemple.

Ces exemples utilisent leurs propres XML. Leurs commandes de compilation et
d'exécution sont détaillées dans [API_USAGE.md](API_USAGE.md).
[architecture.md](architecture.md) permet ensuite d'approfondir les relations
entre les classes et les choix de conception.
