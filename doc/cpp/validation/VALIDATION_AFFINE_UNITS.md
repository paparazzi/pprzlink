# Validation des conversions affines et du périphérique mémoire

Validation réalisée le 7 octobre 2026 sous Ubuntu 24.04, avec GCC 13.4.0 et Boost 1.83.

## Comportement et périmètre

Les valeurs SI sont calculées par `XML × multiplicateur + offset` ; l’inverse soustrait l’offset puis divise par le multiplicateur. Les coefficients XML explicites multiplient la pente de l’alternative une seule fois et préservent son offset. Les tableaux, arrondis entiers, contrôles de plage et octets du protocole gardent leur contrat.

Les préfixes usuels sont reconnus sur les bases autorisées dans la table, avec le facteur au carré/cube pour surfaces/volumes. Les expressions algébriques générales, préfixes empilés et nouveaux encodages à virgule fixe ne sont pas inférés. Toute déclaration inconnue dans `unit` ou `alt_unit`, même inutilisée ou sur un champ texte, arrête le chargement XML avec le contexte et la table à compléter. Les labels opaques explicitement enregistrés conservent seulement l’accès natif.

`MemoryDevice` est un flux mémoire événementiel : les écritures sont bouclées en entrée, `feed()` peut injecter des octets, et les notifications sont postées sur le contexte Asio fourni. Le guide et l’exemple de bouclage utilisent la classe de la bibliothèque.

## Vérifications

- Compilation CMake complète sans sous-module ni paquet de conversion supplémentaire.
- Suite complète avec Ivy et `link++` : 27/27 tests réussis.
- Configuration sans Ivy : 22/22 tests réussis.
- Doxygen : aucun avertissement.
- Construction et installation Make sans Ivy, puis client statique utilisant `MemoryDevice`.
- Comparaison de 2 216 champs du catalogue, dont 1 023 convertibles : aucune différence sur la disponibilité, l’unité SI et les valeurs vérifiées. La référence inclut les lectures natives 0, 1, 2, 100 et les écritures SI 0, 1, 100, avec les mêmes erreurs de plage. Tolérance numérique : 1e-12 absolue et relative.

## Taille des exécutables

Comparaison avec le commit de référence `9ccdad1`. Les sources des deux consommateurs ont été figées avant le changement et sont identiques pour les mesures avant/après. Les bibliothèques de composants sont liées statiquement ; TinyXML2 et les bibliothèques système restent dynamiques. Aucun Ivy n’est lié à ces deux programmes.

Les mesures Release utilisent `-O3 -DNDEBUG`, GCC 13.4.0 et `strip --strip-all`. La comparaison isole le remplacement du moteur de conversion ; la simplification ultérieure de l’exemple avec `MemoryDevice` n’est pas incluse dans ce calcul.

| Exécutable Release | Avant, octets | Après, octets | Gain, octets | Réduction |
| --- | ---: | ---: | ---: | ---: |
| `atelier` sans symboles | 1 491 408 | 699 872 | 791 536 | 53.07 % |
| `atelier_example_1` sans symboles | 1 671 696 | 871 968 | 799 728 | 47.84 % |
| `atelier` avec symboles | 1 831 224 | 989 360 | 841 864 | 45.97 % |
| `atelier_example_1` avec symboles | 2 082 696 | 1 232 840 | 849 856 | 40.81 % |

Les binaires Debug sont aussi recompilés avec les mêmes sources et `-g` des deux côtés :

| Exécutable Debug | Avant, octets | Après, octets | Réduction |
| --- | ---: | ---: | ---: |
| `atelier` | 20 923 800 | 17 451 456 | 16.60 % |
| `atelier_example_1` | 24 729 568 | 21 262 376 | 14.02 % |

Les suites n’exercent pas de radio XBee physique ; le framing et le dialogue AT sont couverts par les simulateurs et les tests de pseudo-terminaux.
