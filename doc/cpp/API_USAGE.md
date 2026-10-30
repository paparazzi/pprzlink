# API pprzlink++ par les exemples

Les exemples comprennent un récepteur Ivy, un simulateur d'avion série, un
enregistreur UDP et un émetteur de commande UDP. Ils utilisent uniquement
les en-têtes publics de la bibliothèque. Leur répertoire peut être copié dans un autre projet et compilé avec le SDK installé.

| Exemple | Parcours et limite qu'il permet d'observer |
| --- | --- |
| [SIUnits.cpp](../../lib/v2.0/C++/examples/clients/SIUnits.cpp) | Consulter les unités XML/SI, arrondir une altitude vers l'entier XML, convertir les températures/tableaux et conserver l'encodage natif, sans I/O. |
| [IvyReceiver.cpp](../../lib/v2.0/C++/examples/clients/IvyReceiver.cpp) | S'abonner, lire une altitude en mètres par `getFieldSI()`, arrêter la boucle. L'abonnement est possédé par une variable locale ; le délai utilise encore l'API native Ivy. |
| [WorkshopIvy.cpp](../../lib/v2.0/C++/examples/clients/WorkshopIvy.cpp) | Choisir `e` ou `r` pour publier ou recevoir une altitude avec le même exécutable. Attendre l'annonce de l'abonnement avant de publier, avec le XML du guide. |
| [SerialAircraft.cpp](../../lib/v2.0/C++/examples/clients/SerialAircraft.cpp) | Choisir le client sol (`c`) ou le serveur avion (`s`) ; échanger PING, une altitude en mètres SI et PONG sur deux ports série reliés. L'application fournit et fait tourner son contexte Asio. |
| [UdpMessages.cpp](../../lib/v2.0/C++/examples/clients/UdpMessages.cpp) | Choisir l'émetteur (`e`) ou le récepteur (`r`) d'une commande UDP avec le même exécutable, puis afficher l'origine réseau et les champs reçus. |
| [UdpRecorder.cpp](../../lib/v2.0/C++/examples/clients/UdpRecorder.cpp) | Recevoir trois messages avec leur adresse source et leur taille. Aucun bus Ivy n'est démarré ni lié au programme. |
| [UdpSettingSender.cpp](../../lib/v2.0/C++/examples/clients/UdpSettingSender.cpp) | Remplir les deux champs d'une commande dans un seul `setField()`, puis envoyer son datagramme à un pair de test local. |
| [PtyPeer.cpp](../../lib/v2.0/C++/examples/clients/PtyPeer.cpp) | Choisir le client demandeur (`c`) ou le serveur répondeur (`s`) et échanger une altitude sur une liaison série, avec le XML du guide. |
| [PtyAgent.cpp](../../lib/v2.0/C++/examples/clients/PtyAgent.cpp) | Lancer le même agent aux deux extrémités : chaque processus publie et reçoit une altitude, sans rôle lié au port. |

Ces exemples sont de petits clients exécutables de référence, pas des services
complets de production. Les clients série/UDP/Ivy sont bornés à dix secondes.
Utiliser le XML fourni avec un pair de test qui charge les mêmes définitions.

Les deux rôles de `pty_peer` utilisent `guide_messages.xml` et attendent jusqu'à
60 secondes, pour permettre leur lancement manuel dans deux terminaux.
`pty_agent` utilise le même XML et continue jusqu'à Ctrl+C. Lancez deux
instances avec des identifiants différents, sur **Port A** et **Port B**
indifféremment ; les deux processus envoient et reçoivent.
`socat` prépare les deux ports virtuels reliés.
Le [guide d'utilisation](guide_d_utilisation.md#essayer-deux-processus-sur-des-pseudo-terminaux)
donne les commandes de compilation et de lancement. Ces clients se lient
uniquement à `pprzlink::io` et peuvent également utiliser le SDK sans Ivy.

## Principes de l’API

1. Les écritures numériques vérifient la plage et refusent une partie
   fractionnaire lors d'une conversion vers un entier. Une erreur ne remplace
   pas la valeur déjà présente. Les lectures `getField<T>()` restent strictes ;
   `getFieldAs<T>()` demande explicitement une conversion numérique scalaire contrôlée.
2. `bind()` conserve un abonnement qui reçoit un message et ses métadonnées
   facultatives. `start()` active la réception réactive et `stop()` la met en
   pause. Pour Ivy, `close()` termine la session et fait sortir de `run()`.
3. La réception ordinaire Ivy utilise aussi `bind()` et `unbind(id)`. Les callbacks
   empruntent le message ; le lien conserve les abonnements. Les requêtes utilisent
   la capacité commune `RequestReply`, actuellement implémentée uniquement par Ivy.
4. Les composants CMake `core`, `io` et `ivy` permettent de choisir les
   dépendances. Les cibles `pprzlink++` et `pprzlink++_static` les regroupent.

Les exemples et les tests de leurs échanges sont la référence pour faire
évoluer ces contrats. Les conventions de routage, PING périodiques et rapports
de station sol restent dans `link++`.

## Champs : unités SI explicites

Les méthodes existantes manipulent les unités XML. `getFieldAs<double>()`
convertit le type numérique, sans convertir l'unité. L'accès SI est un appel
explicite et utilise systématiquement des `double` :

```cpp
pprzlink::Message gps(dictionary.getDefinition("GPS"));
gps.setFieldSI("alt", 123.456);   // Mètres -> int32 en millimètres.
gps.setFieldSI("speed", 12.34);   // m/s -> uint16 en cm/s.
double altitude_m = gps.getFieldSI("alt");
int32_t altitude_mm = gps.getField<int32_t>("alt"); // 123456

const auto& field = gps.getFieldDefinition("alt");
std::cout << field.getUnit() << " -> " << field.getSIUnit(); // mm -> m
int32_t native = field.fromSI<int32_t>(1.23456); // 1235
double si = field.toSI(native);                 // 1.235 m
```

`getFieldDefinition()` est disponible avant même que la valeur soit définie,
par nom ou indice. `getUnit()` et `getAltUnit()` renvoient les libellés XML
originaux, ou une chaîne vide pour un attribut absent. `getAltUnitCoef()`
renvoie un `std::optional<double>` contenant uniquement le coefficient écrit
dans le XML. `getSIUnit()` indique l'unité effectivement utilisée par l'API SI ;
une chaîne vide et `canConvertSI() == false` signalent une conversion indisponible.

| Grandeur | Convention de l'API SI |
| --- | --- |
| Longueur, durée, vitesse | m, s, m/s |
| Angles, latitude et longitude incluses | rad |
| Température absolue | K ; 293.15 K devient 20 °C dans un champ Celsius |
| Pourcentage | Rapport sans unité ; 0.5 devient 50 % |

Un setter SI choisit le type du XML. Une destination entière est arrondie à
l'entier le plus proche, avec les demi-valeurs en s'éloignant de zéro, puis
contrôlée en plage. Il n'y a ni mode strict ni saturation. Le getter SI restitue
la valeur stockée après cet arrondi. `fromSI<T>()`/`toSI(T)` convertissent un
scalaire ou un élément de tableau sans modifier de message ; `T` doit être
exactement le type de base XML. Les flottants peuvent arrondir vers `float` ;
les grands entiers 64 bits peuvent perdre de la précision dans le `double` SI.

Pour un tableau homogène :

```cpp
std::array<double, 3> position_m{1.0, 2.0, 3.0};
message.setFieldSI("position", position_m); // Champ numérique XML de trois éléments.
const auto position_si = message.getFieldArraySI("position");
```

`getFieldSI()` renvoie un `double` pour un scalaire ; `getFieldArraySI()` renvoie
un `std::vector<double>` pour un tableau. Sa longueur correspond au tableau
stocké dans le message, fixe ou variable selon le XML. Les lectures acceptent
aussi un indice XML. Les paramètres de sortie `double&` et
`std::vector<double>&` de `getFieldSI()` restent disponibles.

Les surcharges scalaire et tableau conservent la forme et les contrôles de
taille XML. Une conversion qui échoue ne remplace ni le champ, ni la destination
d'une lecture. `setFieldSI()` renvoie `const Message&`, comme `setField()` ;
on peut écrire `transport.sendMessage(message.setFieldSI("altitude", 123.5))`
lorsque le message est complet.

Les règles éditables sont dans [UnitAliases.cpp](../../lib/v2.0/C++/pprzlink/UnitAliases.cpp) :

```cpp
{"1e7deg", "rad", std::numbers::pi / (180.0 * 1e7)},
{"deg_celsius", "K", 1.0, 273.15},
{"C", "K", 1.0, 273.15, "ESC", "temperature"},
{"adc", ""}, // Libellé connu, sans conversion SI faute de calibration.
```

`SI = XML * multiplicateur + offset` ; l'inverse soustrait l'offset avant de
 diviser par le multiplicateur. Les règles les plus spécifiques ont priorité,
puis la première en cas d'égalité. Un coefficient XML explicite multiplie
uniquement la pente de l'unité alternative, sans être appliqué deux fois.
Les contrôles de plage et arrondis du type XML restent inchangés.

Le résolveur accepte les préfixes usuels `da`, `G`, `M`, `k`, `h`, `d`, `c`, `m`,
`u`/`µ`/`μ`, `n`, `p` sur les bases marquées dans la table. Un seul préfixe est
appliqué ; les formes composées doivent être enregistrées. `cm²` et `mm3`
utilisent le facteur au carré et au cube. Les encodages comme `1e7deg` restent
des règles explicites, afin de ne pas deviner le sens d'un nouveau codage.

Toute déclaration inconnue dans `unit` ou `alt_unit` lève `bad_message_file`
dès le chargement XML, même si l'application ne lit pas ce champ en SI. Le
diagnostic indique le fichier, message, champ, libellé et `UnitAliases.cpp`.
Les incompatibilités de dimensions sont également refusées. Les libellés
opaques explicitement reconnus (`adc`, `pprz`, `dB`, données binaires, etc.)
peuvent être lus nativement mais une demande SI lève `field_unit_error`.
Les labels applicatifs `motor` et `foo` sont enregistrés comme opaques,
sans inventer de grandeur physique.

Les dépassements numériques lèvent `field_conversion_error`. Les NaN de mesures
flottantes se propagent ; ils ne sont pas acceptés vers les entiers. Les repères,
références d'altitude et epochs ne sont pas transformés. `GROUND_REF.pos`, dont
les unités varient selon `frame`, reste à traiter dans le code applicatif.
La conversion est intégrée directement dans la bibliothèque et dans son SDK.
Leurs appels existants gardent leurs unités XML ; les lectures/écritures SI sont
adoptables localement pour les champs dont `canConvertSI()` est vrai. Les
identifiants et commandes sans unité physique passent par l'API native. Le relais
de `link++` conserve les valeurs XML et les rapports de compatibilité OCaml.

Les exemples série, PTY et Ivy utilisent l'accès SI pour leurs altitudes.
Les tests lancent aussi les mêmes clients série/Ivy avec une altitude XML
`int32` en millimètres : l'entrée et l'affichage restent en mètres.
L'enregistreur UDP conserve les valeurs XML pour ses sorties de diagnostic ;
le réglage générique conserve son accès natif car son unité est indéterminée.
Dans `link++`, les accès aux champs sont utilisés pour le routage et le relais.
Les rapports `LINK_REPORT` gardent leur calcul/formatage compatible OCaml en
double, plutôt que d'introduire une conversion vers les floats de leur schéma.

## Champs : conversions et erreurs explicites

Avec les définitions de `client_messages.xml` :

```cpp
pprzlink::Message command(dictionary.getDefinition("CLIENT_SETTING"));
command.setSenderId(0);
command.setReceiverId(42); // Destinataire dans l'en-tête binaire.
command.setField("ac_id", 42);
command.setField("value", 123.5); // Double converti vers le float du XML.

auto exact = command.getField<float>("value");
auto converted = command.getFieldAs<double>("value");

try {
    command.setField("ac_id", 300);
} catch (const pprzlink::field_conversion_error &error) {
    // Le diagnostic indique le champ, la valeur et les types.
    // ac_id vaut toujours 42 : aucune troncature silencieuse vers 44.
}
```

`addField()` reste disponible et applique les mêmes contrôles que `setField()`.
`setField()` accepte également plusieurs couples nom/valeur dans un seul appel :

```cpp
command.setField("ac_id", 42, "value", 123.5);
```

Les deux surcharges renvoient `const Message&` : une référence au même message,
sans copie. Cela permet de renseigner les champs directement dans l'appel
d'envoi :

```cpp
link.sendMessage(command.setField("value", 12.5f));
link.sendMessage(command.setField("ac_id", 42, "value", 12.5f));
```

Le message doit avoir tous ses champs renseignés pour être envoyé. Si
`setField()` lève une exception, `sendMessage()` n'est pas appelé. La référence
retournée est empruntée au message et ne prolonge pas sa durée de vie.
Les appels qui ignorent le retour continuent de fonctionner ; `addField()`
conserve son retour `void`.

L'appel doit contenir un nombre pair d'arguments, avec au moins un couple.
Un nombre impair est refusé à la compilation. Chaque valeur peut avoir son
propre type, y compris une chaîne ou un tableau, avec les mêmes contrôles XML
que l'appel à deux arguments. Les programmes utilisant cet appel conservent
leur comportement.

Pour la partie variadique, les diagnostics de compilation citent les contraintes
nommées `EvenNumberOfFieldArguments` si une valeur manque et
`FieldNamesAreStrings` si une clé n'est pas convertible en `std::string`.
La première clé est directement un paramètre `const std::string&` : son mauvais
type provoque une erreur de conversion. Les erreurs renvoient à la ligne de
l'appel, y compris lorsque la mauvaise clé est en fin de liste.

| Appel incorrect | Diagnostic attendu |
| --- | --- |
| `message.setField("index", 7, "ac_id")` | `EvenNumberOfFieldArguments` non satisfaite : la valeur de `ac_id` manque. |
| `message.setField("index", 7, 42, 12.5)` | `FieldNamesAreStrings` non satisfaite : le troisième argument est une clé entière. |
| `message.setField("index", 7, "ac_id", 42, true, 12.5)` | Même contrainte : le cinquième argument est une clé booléenne. |

Les noms réels des champs et leurs types XML sont connus à l'exécution. Une
clé mal orthographiée, une chaîne fournie pour un champ numérique ou une valeur
hors plage restent donc des erreurs d'exécution, même si l'appel est correct
du point de vue des types C++.

Les couples sont appliqués de gauche à droite. Si une validation échoue, les
couples précédents restent appliqués ; le champ refusé et les suivants ne sont
pas modifiés. Un nom répété prend la dernière valeur. Ces appels préparent
seulement le message local : l'envoi reste un appel séparé à `sendMessage()`.

Les éléments de tableaux sont contrôlés eux aussi. Les écritures hors plage,
les flottants non finis vers un entier et les valeurs fractionnaires vers un
entier lèvent `field_conversion_error`, dérivée de `std::out_of_range`.
Les setters d'identifiants d'en-tête acceptent également des nombres contrôlés :
`setSenderId(42)` fonctionne directement ; `setSenderId(300)` et
`setReceiverId(-1)` échouent sans modifier l'identifiant précédent. La limite
protocolaire de quatre bits du composant reste vérifiée lors de l'encodage.
Le champ `ac_id` et le destinataire d'en-tête restent deux informations distinctes :
certains messages décrivent un autre avion. La bibliothèque ne déduit pas les
règles de routage à partir du nom d'un champ.

Une destination flottante peut arrondir, notamment lors d'une conversion
`double` → `float` ou d'un grand entier vers un flottant. La conversion contrôlée
ne promet donc pas une représentation sans perte. Les valeurs flottantes de
même type, y compris les NaN, restent prises en charge par le protocole binaire.

`getField<T>()` demande le type exact. Une incompatibilité lève
`field_type_mismatch`, dérivée de `std::bad_variant_access`, avec le nom du
champ, son type XML et le type demandé. `getFieldAs<T>()` convertit uniquement
les scalaires numériques ; les getters de tableaux utilisent les types
d'éléments du XML.

Pour lire du texte sans copier ses octets, utilisez une `std::string_view`
(en-tête `<string_view>`) sur un champ `string`, `char[]` ou `char[N]` :

```cpp
const auto label = samples.getField<std::string_view>("label");
```

La vue emprunte le stockage du message jusqu'au remplacement du champ, à la
réaffectation ou à la destruction du message ; n'utilisez pas un message
temporaire. Les lectures par index et avec paramètre de sortie acceptent
également ce type. La vue conserve la longueur complète et les octets nuls,
sans exiger de terminateur. Les autres types de champs lèvent
`field_type_mismatch`.

Pour les tableaux, `std::span<const T>` (en-tête `<span>`) emprunte les éléments
sans copie et prend la longueur du tableau stocké :

```cpp
const auto axes = samples.getField<std::span<const float>>("axes");
```

Le type `T` doit correspondre au type XML des éléments. Les getters acceptent
les spans à taille dynamique. Les mêmes règles de durée de vie que
pour `string_view` s'appliquent, ainsi que la lecture par index ou avec paramètre
de sortie. Utilisez des éléments `const` : `const auto` seul ne rend pas les
éléments d'un `std::span<T>` constants, et les spans modifiables ne sont pas
acceptés par ces getters.

## Bouclage en mémoire

`MemoryDevice` évite un périphérique personnalisé pour les exemples sans matériel.
Les écritures deviennent de l'entrée et les notifications sont postées sur Asio :

```cpp
#include <pprzlink/MemoryDevice.h>
#include <pprzlink/PprzTransport.h>

boost::asio::io_context context;
pprzlink::PprzTransport receiver(std::make_unique<pprzlink::MemoryDevice>(context), dictionary);
receiver.bind("GUIDE_ALTITUDE", [&](const pprzlink::Message &message) {
    std::cout << message.toString() << '\n';
    receiver.stop();
});
receiver.start();
receiver.sendMessage(altitude);
context.run();
```

`feed(span<const uint8_t>)` permet aussi d'injecter des octets ou fragments dans
un simulateur. Cette mécanique n'est pas nécessaire au code métier : les bindings
reçoivent des messages complets. Le contexte doit survivre au périphérique ; les
callbacks sont exécutés hors du verrou de son tampon, sans thread supplémentaire.

## Réception réactive : abonnements et filtres

`UdpTransport`, `PprzTransport`, `XbeeTransport` et `IvyLink` partagent `bind()`.
Les transports binaires utilisent le contexte Asio fourni par l'application ;
Ivy utilise sa boucle native. Aucun thread de polling n'est créé.

```cpp
receiver.bind("GUIDE_ALTITUDE", [](const pprzlink::Message &message) {
    std::cout << message.getFieldSI("altitude") << " m\n";
});
receiver.bind(pprzlink::ALL,
    [](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
        if (info.udpPeer) {
            std::cout << info.udpPeer->address << ':' << info.udpPeer->port << '\n';
        }
        std::cout << message.toString() << '\n';
    });
receiver.start();
context.run(); // Pour UDP et les Device Asio. Avec Ivy : link.run().
```

Le récepteur conserve les callbacks jusqu'à `unbind(id)` ou sa destruction.
L'identifiant retourné par `bind()` n'est pas un jeton RAII : un appel sans
variable de retour reste actif. Le nom du message est résolu au moment du bind.
`ALL` sélectionne tous les messages correctement décodés du dictionnaire.

```cpp
auto id = receiver.bind("GUIDE_ALTITUDE",
    {.senderId = 42, .udpAddress = "192.168.1.10"},
    [](const pprzlink::Message &message) { /* traiter l'altitude */ });
receiver.unbind(id);
```

| Critère de `ReceiveFilter` | Sélection |
| --- | --- |
| `senderId` | Identifiant PPRZLINK 0..255 ; les expéditeurs Ivy numériques sont aussi reconnus. |
| `sender` | Expéditeur Ivy textuel exact. |
| `receiverId` | Destination binaire 0..255 ; 255 signifie broadcast. |
| `className`, `classId` | Classe du dictionnaire, par nom ou identifiant 0..15. |
| `componentId` | Composant binaire 0..15. |
| `udpPeer` | Adresse IP et port source exacts du datagramme. |
| `udpAddress`, `udpPort` | Adresse IP seule ou port source seul. |
| `xbeeAddress` | Adresse radio source, indépendante de `senderId`. |
| `minimumRssi` | Seuil négatif en dBm ; les trames sans RSSI ne correspondent pas. |
| `where` | Prédicat booléen prenant le message, avec métadonnées en second argument facultatif. |

Les critères se combinent avec ET. Des conditions alternatives peuvent être
exprimées dans `where`, par exemple destination 42 ou broadcast. Les identifiants
hors plage, classes inconnues et filtres incompatibles avec le transport sont
refusés au bind. Ivy ne transmet pas les IDs binaires de destination/composant.
Les adresses UDP sont numériques et normalisées ; aucune résolution DNS n'est faite.

Chaque message est décodé une fois et présenté par référence constante aux
abonnements correspondants, dans l'ordre d'inscription. Un abonnement ajouté
pendant un callback commence au message suivant ; un abonnement retiré ne sera
plus invoqué. `stop()` désactive immédiatement la distribution et annule les
lectures de ce récepteur, sans arrêter le contexte Asio partagé. `start()` peut
le réactiver, en conservant les abonnements. Les callbacks d'un récepteur sont
sérialisés, y compris lorsque plusieurs threads exécutent le contexte.

Pour `IvyLink`, cette pause concerne les abonnements de `Receiver::bind()`.
Le bus, ses timers, les envois et les abonnements requête/réponse
continuent de fonctionner. `start()`
réactive les abonnements réactifs conservés. `close()` termine définitivement
la session Ivy et demande la sortie de `run()` ; une nouvelle session demande
un nouvel `IvyLink`. Ces opérations peuvent être appelées depuis un callback.

Les références de message/métadonnées et les `span`/`string_view` empruntés sont
valables pendant le callback. Copiez un `Message` et/ou `ReceiveInfo` pour les
conserver ; recréez les vues à partir de la copie. Une lambda peut envoyer une
réponse, appeler `stop()` ou `unbind()`. Détruisez le récepteur après la sortie
du callback. Les callbacks longs retardent les autres événements du contexte.

```cpp
receiver.onError([](const pprzlink::ReceiveError &error) {
    if (error.kind == pprzlink::ReceiveError::Kind::Decode)
        std::cerr << "Trame ignorée : " << error.message << '\n';
    else
        std::rethrow_exception(error.exception);
});
```

Une trame invalide est consommée, puis le décodage peut continuer. Par défaut,
les erreurs de décodage sont comptées et ignorées ; les erreurs terminales
arrêtent la réception et remontent dans la boucle. `onError()` peut les traiter.
Les exceptions des prédicats et callbacks applicatifs arrêtent la réception
et restent visibles ; elles ne sont pas converties en erreurs de trame.

Le décodeur sans I/O `PprzFrameDecoder` fournit `pushBytes()` et des lectures
incrémentales pour
les intégrations qui fournissent elles-mêmes les octets.

`UdpTransport::sendMessage(message, destination)` demande une destination réseau
explicite. Le pair UDP et l'identifiant PPRZLINK sont indépendants. Les messages
d'un même datagramme gardent sa source ; les trames incomplètes sont abandonnées
à sa frontière pour éviter de mélanger deux datagrammes ou deux émetteurs.

L'exemple [WorkshopUdp.cpp](../../lib/v2.0/C++/examples/clients/WorkshopUdp.cpp) propose
les modes `both`, `emitter` et `receiver` de l'atelier.

Avec XBee, l'adresse radio, l'identifiant PPRZLINK du destinataire, le frame ID
de suivi et le choix des ACK/retries radio sont indépendants. Pour imposer
ensemble l'adresse et le frame ID :

```cpp
radio.sendMessageToWithId(message, pprzlink::XbeeAddress::network(0x0100), 7,
                          pprzlink::RadioRetries::Enabled);
```

`sendMessageTo()` choisit lui-même le frame ID ; `sendMessageWithId()` conserve
le choix de destination fondé sur le message. Un frame ID explicite ne doit
pas être réutilisé pour un autre message tant que son statut est attendu ;
zéro supprime la réponse de statut. Un envoi écrit une trame complète ou lève
une exception. Le suivi des statuts, les délais et les éventuelles nouvelles
tentatives applicatives restent à la charge de l'appelant ; `RadioRetries`
sélectionne uniquement les ACK/retries effectués par le modem.

## Ivy : abonnements et durée de vie

`IvyLink::sendMessage()` transmet le texte produit par `serializeMessage()`.
Les champs numériques utilisent `std::to_chars`, indépendamment de la locale
et des réglages d'un flux. Les `float` et `double` finis sont transmis dans leur
représentation courte conservant exactement leur valeur lors du décodage dans
le même type : `123.6`, pas `123.600000`. La notation scientifique, les valeurs
minuscules et le zéro négatif sont pris en charge pour les scalaires et tableaux.
Le codec écrit directement depuis un tampon local sans flux temporaire par
champ ; le parseur évite les copies pour les nombres sans séparateurs `_`.
Le codec `serializeLegacyMessage()` garde son contrat distinct de formatage
OCaml/XML, notamment les précisions explicitement demandées dans le XML.

Les deux parseurs acceptent aussi `nan` et `inf`, avec signe optionnel et
leurs variantes majuscules `NAN`/`INF`, pour les scalaires et tableaux `float`
ou `double`. Les températures non mesurées de `CAMERA_SNAPSHOT_DL` peuvent
ainsi traverser le relais Ivy de `link++`. Le texte conserve le sens de NaN
et le signe des infinis ; les bits de charge utile d'un NaN ne sont pas
préservés. Les champs entiers refusent toujours les valeurs non finies, et
un nombre tel que `1e1000` reste un dépassement rejeté.

```cpp
auto binding = link.bind("CLIENT_ALTITUDE",
    [](const pprzlink::Message &message) {
        const double altitude = message.getFieldSI("altitude"); // Mètres SI.
        const auto &sender = std::get<std::string>(message.getSenderId());
        // Traiter altitude et sender pendant le callback.
    });
link.start(); // Avec un thread Ivy détenu par le lien ; run() appelle start() sinon.
// Plus tard : link.unbind(binding).
```

Le lien conserve le callback ; l'identifiant retourné sert uniquement à
`unbind()`. Sa destruction n'annule pas l'abonnement. Copiez le `Message` pour
le conserver après le callback. `bind(pprzlink::ALL, {.sender = "42"}, callback)`
sélectionne les messages d'un expéditeur textuel.

Ivy crée un abonnement natif par nom distinct. Un binding `ALL` les remplace
par un seul abonnement large ; son retrait rétablit les abonnements nommés.
Les filtres supplémentaires restent locaux. Les changements sont annoncés aux
pairs de façon asynchrone et peuvent entraîner une brève perte de messages.

Les callbacks s'exécutent sur le contexte Asio fourni, ou sur la boucle Ivy
sans contexte. La capacité `RequestReply` utilise `sendRequest()` et
`cancelRequest()` pour une réponse unique, `bindRequest()` et `unbindRequest()`
pour un répondeur persistant. Les IDs sont de simples valeurs ; leur destruction
ne désabonne pas. Les callbacks empruntent le message et, facultativement, ses
métadonnées. L'annulation supprime aussi les invocations en attente. Aucun délai
maximal n'est imposé ; les requêtes restent actives pendant `stop()`. Seul Ivy
implémente cette interface pour le moment.

Un récepteur qui doit quitter `link.run()` après son dernier message appelle
`link.close()` dans son callback. `link.stop()` suspend seulement les
abonnements réactifs `bind()` et laisse la boucle Ivy active.

## Dépendances choisies par le client

| Composant CMake | Contenu |
| --- | --- |
| `pprzlink::core` | XML, messages, conversions SI affines, codecs, framing PPRZ/XBee, réception réactive, `MemoryDevice` et interface abstraite `Device`. TinyXML2, en-têtes Boost et threads ; aucune dépendance Ivy. |
| `pprzlink::io` | Série Boost.Asio, fichiers POSIX et UDP ; dépend de `core` et des threads. |
| `pprzlink::ivy` | IvyLink ; dépend de `core`, Ivy C++ et des threads. |

Les composants sont des archives statiques construites avec PIC. Les cibles
`pprzlink++` et `pprzlink++_static` sont respectivement partagée
et statique. Ne pas mélanger les cibles agrégées et les composants dans un même
client ; choisir l'une des deux interfaces de construction.

Le type `BytesBuffer` est déclaré dans `<pprzlink/BytesBuffer.h>`, indépendamment
des périphériques et de leurs exécuteurs. Les en-têtes de valeurs et de codecs
n'incluent plus `Device.h` ni les API Asio par ce chemin. La cible CMake `core`
contient toujours les mécanismes de réception et dépend des en-têtes Boost.

```cmake
find_package(pprzlink++ CONFIG REQUIRED COMPONENTS io)
target_link_libraries(my_recorder PRIVATE pprzlink::io)
```

Cet appel ne cherche pas Ivy, même si le SDK a été construit avec son composant
Ivy. Sans `COMPONENTS`, `find_package`
charge les dépendances des bibliothèques agrégées. Demander le composant `ivy`
à un SDK construit sans Ivy provoque une erreur de configuration explicite.

## Construire et essayer les exemples

Depuis `lib/v2.0/C++`, après préparation des dépendances décrites dans
[la référence C++](reference.md#build) :

```sh
cmake -S . -B build-gcc13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DPPRZLINK_BUILD_EXAMPLES=ON
cmake --build build-gcc13
ctest --test-dir build-gcc13 --output-on-failure

./build-gcc13/ivy_receiver examples/clients/client_messages.xml 127.255.255.255:2010
```

Pour l'atelier Ivy du chapitre 8, le même exécutable utilise `guide_messages.xml`
et un rôle `r` ou `e`. Dans deux terminaux :

```sh
# Premier terminal : attend une altitude pendant dix secondes.
./build-gcc13/workshop_ivy examples/clients/guide_messages.xml r
```

```sh
# Second terminal : attend l'abonnement du récepteur, puis publie une altitude de 123,5 m.
./build-gcc13/workshop_ivy examples/clients/guide_messages.xml e
```

Le bus par défaut est `127.255.255.255:2010`. Un domaine facultatif peut suivre
le rôle ; utilisez la même valeur dans les deux instances. L'émetteur attend
le récepteur jusqu'à cinq secondes, ce qui permet aussi de le lancer en premier.
Le `messages.xml` créé dans le guide peut remplacer `guide_messages.xml`.

Pour l'exemple série, le troisième argument choisit le rôle. Dans un premier
terminal, lancez le serveur avion :

```sh
# Le serveur 42 attend un PING, puis répond avec CLIENT_ALTITUDE et PONG.
./build-gcc13/serial_aircraft examples/clients/client_messages.xml /dev/ttyUSB0 s
```

Dans un second terminal, lancez le client sol :

```sh
# Le client 0 envoie PING à l'avion 42 et affiche son altitude et son PONG.
./build-gcc13/serial_aircraft examples/clients/client_messages.xml /dev/ttyUSB1 c
```

Les deux ports doivent être **distincts et reliés**, physiquement ou par des
pseudo-terminaux. Le programme configure 57600 bauds, 8N1, sans contrôle de
flux. Le client répète le PING toutes les 500 ms jusqu'à recevoir les deux
réponses ; les deux rôles limitent leur attente à dix secondes et terminent
après un échange réussi. Le client peut aussi démarrer avant le serveur.
L'altitude est envoyée en réponse au PING, une fois le client prêt à la recevoir.

Pour demander et fournir une altitude, le même programme `pty_peer` utilise
`guide_messages.xml`. Dans deux terminaux, avec des ports distincts et reliés :

```sh
# Serveur avion 42 ; un port physique ou un pseudo-terminal peut être utilisé.
./build-gcc13/pty_peer examples/clients/guide_messages.xml /dev/ttyUSB0 s
```

```sh
# Client sol 0 ; répète la demande jusqu'à recevoir l'altitude, pendant au plus 60 secondes.
./build-gcc13/pty_peer examples/clients/guide_messages.xml /dev/ttyUSB1 c
```

Pour émettre et enregistrer une commande UDP, utilisez `udp_messages` avec un
rôle `r` ou `e` et le catalogue `client_messages.xml` :

```sh
# Premier terminal : reçoit un message et affiche sa source réseau et ses champs.
./build-gcc13/udp_messages examples/clients/client_messages.xml 4242 r
```

```sh
# Second terminal : envoie une commande CLIENT_SETTING à 127.0.0.1:4242.
./build-gcc13/udp_messages examples/clients/client_messages.xml 4242 e
```

Le rôle `e` prépare l'avion 42 et `value=12.5` ; le rôle `r` termine après le
premier message, ou après dix secondes sans réception. En réception, le port
0 laisse le système choisir un port libre, affiché par le programme ; en
émission, utilisez ce port effectivement attribué.

Construction sans Ivy, incluant les exemples série/UDP et leurs tests :

```sh
cmake -S . -B build-no-ivy -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DPPRZLINK_WITH_IVY=OFF -DBUILD_TESTING=ON -DPPRZLINK_BUILD_EXAMPLES=ON
cmake --build build-no-ivy
ctest --test-dir build-no-ivy --output-on-failure
```

Le Makefile accepte également `WITH_IVY=0`. Ses bibliothèques
agrégées excluent alors l'adaptateur Ivy.

Pour vérifier un usage extérieur au dépôt :

```sh
cmake --install build-gcc13 --prefix "$PWD/sdk"
cp -R examples/clients /tmp/pprzlink-client-examples
cmake -S /tmp/pprzlink-client-examples -B /tmp/pprzlink-client-build \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 -DCMAKE_PREFIX_PATH="$PWD/sdk"
cmake --build /tmp/pprzlink-client-build
```

`-DCLIENTS_WITH_IVY=OFF` construit les trois clients série/UDP sans rechercher
Ivy. Le standard C++23 et les dépendances transitives proviennent des cibles
installées ; le projet client ne les redéclare pas.
