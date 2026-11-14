# OpenTTD Mobile (fork `Ankool29`)

Ce dossier contient l'empaquetage Android expérimental d'OpenTTD.

## Ce qui est ajouté par rapport à l'amont

- OpenTTD est compilé en bibliothèque partagée `libmain.so` chargée par
  `SDLActivity` (via `libSDL2.so`).
- L'entrée native se trouve dans `src/os/android/android_main.cpp` : elle
  définit `HOME`/`TMPDIR` sur le stockage privé de l'application avant
  l'initialisation d'OpenTTD.
- La couche tactile (`src/video/touch_gesture.*`) traduit les gestes en
  événements souris, sans code spécifique par fenêtre :
  - appui court → clic gauche,
  - appui long (~500 ms) → clic droit,
  - glissement à un doigt → glisser du bouton gauche (déplacement de la
    carte grâce au mode `MapLMB` par défaut sur Android, sliders et
    barres de défilement inchangés),
  - geste à deux doigts (pincement ou glissement) → molette (zoom de la
    carte, défilement des listes).
- `gui.touch_mode` agrandit les widgets interactifs et impose une taille
  minimale adaptée au doigt. Il est actif par défaut sur Android.
- En mode tactile, la fenêtre des ordres propose un bouton
  `Choose station` qui liste les stations compatibles triées par distance.
  La sélection sur la carte reste disponible via `Go To`. La liste réutilise
  la validation native des ordres, y compris la compatibilité de tous les
  véhicules partageant la liste d'ordres.
- En mode tactile, les fenêtres d'information non modales (véhicules, gares,
  villes, industries, listes, finances, etc.) peuvent être repliées en
  onglets latéraux via le bouton `Dock`. Les onglets conservent l'état de la
  fenêtre, restent accessibles au toucher et défilent lorsqu'ils sont trop
  nombreux pour l'écran.
- En mode tactile, la fenêtre des options limite la largeur des listes
  déroulantes à l'espace disponible et donne un défilement vertical aux
  onglets Général, Graphismes, Son et Social. L'onglet Avancé conserve son
  défilement existant.
- La reconnaissance tactile utilise une machine à états explicite (tap,
  glissement, appui long, deux doigts). Les coordonnées sont resynchronisées
  après rotation ou redimensionnement, les doigts supplémentaires sont
  ignorés proprement et les boutons souris sont relâchés en cas
  d'interruption.
- V2.5C annule un tracé en cours sans exécuter de commande lorsqu'un second
  doigt arrive, en conservant l'outil sélectionné. Deux doigts glissés
  ensemble déplacent la carte via le mécanisme natif du viewport, sans zoom
  lorsque la distance entre les doigts reste stable.
- Android utilise une échelle d'interface stable de 150 % par défaut, afin
  que la rotation portrait/paysage ne change pas la taille de l'interface.
- Le paysage et le portrait sont gérés par le redimensionnement SDL déjà
  présent dans OpenTTD (`GameSizeChanged` → `RelocateAllWindows`).

Les graphismes, les règles et le gameplay ne sont pas modifiés.

## Application et signature

- Identifiant d'application : `org.ankool29.openttdmobile`.
- Une clé expérimentale est fournie dans
  `android/keystore/openttd-mobile-experimental.p12` afin que les mises à
  jour successives de l'APK expérimental soient acceptées par Android.
  - type : PKCS12
  - alias : `openttdmobile`
  - mot de passe (stockage et clé) : `openttd-mobile-experimental`
- Cette clé est publique et ne doit servir qu'aux builds expérimentaux.
  Pour une distribution officielle, fournir une clé dédiée via les secrets
  GitHub :
  - `ANDROID_KEYSTORE_FILE` (chemin vers le fichier de clé)
  - `ANDROID_KEYSTORE_TYPE` (`PKCS12` ou `JKS`)
  - `ANDROID_KEYSTORE_PASSWORD`
  - `ANDROID_KEY_ALIAS`
  - `ANDROID_KEY_PASSWORD`

## Compilation

Le workflow `.github/workflows/android.yml` construit l'APK arm64 sur un
runner Linux (pas de compilation lourde sur le téléphone) :

1. Android SDK 34, build-tools 34.0.0, NDK r28c (`28.2.13676358`) ;
2. vcpkg au commit épinglé dans `vcpkg.json`, triplet
   `arm64-android-shared` (dépendances partagées pour que SDL2 soit
   chargeable par Java) ;
3. compilation des outils hôte (`strgen`) dans un build séparé, puis CMake avec le NDK (`ANDROID_ABI=arm64-v8a`,
   `ANDROID_PLATFORM=android-28`, `ANDROID_STL=c++_shared`) ;
4. récupération des classes Java SDL et de l'icône depuis la source SDL
   `2.32.10` ;
5. copie des fichiers de baseset (polices, métadonnées) et des langues
   (`build-android/baseset`, `build-android/lang/*.lng`) dans les assets de
   l'APK ;
6. suppression des symboles des bibliothèques natives (`llvm-strip`) ;
7. assemblage Gradle et signature de l'APK.

L'APK est publié comme artefact `openttd-mobile-apk-<run>` avec son
empreinte SHA-256.

### Reproduire localement

Prérequis : JDK 17, SDK Android, NDK r28c, CMake ≥ 3.17, Ninja, vcpkg.

```sh
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/28.2.13676358"
./vcpkg/bootstrap-vcpkg.sh -disableMetrics

cmake -S . -B build-android -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-android-shared \
  -DVCPKG_OVERLAY_TRIPLETS="$PWD/android/triplets" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_STL=c++_shared \
  -DOPTION_USE_ASSERTS=OFF

cmake --build build-android --target openttd -j"$(nproc)"
```

Puis copier `libmain.so`, les `.so` de
`vcpkg/installed/arm64-android-shared/lib/` et `libc++_shared.so` dans
`android/app/src/main/jniLibs/arm64-v8a/`, récupérer les sources Java SDL,
et lancer :

```sh
gradle -p android assembleRelease
```

## État du build

Le workflow a produit un APK installable signé avec la clé expérimentale :

- exécution GitHub Actions : `43442750341` ;
- artefact : `openttd-mobile-apk-17` ;
- fichier : `openttd-mobile-arm64.apk`, 16 Mo ;
- SHA-256 : `3c264746bb2b0156ef089f5bc2a918a2e4627f9da9c418e78395d31b75af895f`.

Les bibliothèques natives sont vérifiées : `libmain.so` exporte `SDL_main`,
`AndroidStartupMarkComplete` et `AndroidLogError`, son nom de bibliothèque
est `libmain.so`, et toutes ses dépendances (`libSDL2.so`,
`libc++_shared.so`, `libfreetype.so`, `libcurl.so`, etc.) sont présentes
dans l'APK. Les segments `LOAD` sont alignés sur 16 Ko. L'APK contient les
66 fichiers de langue et les fichiers de baseset nécessaires au premier
démarrage. `minSdk` et l'API native sont alignés sur 28.

L'APK 17 reste la dernière version validée sur téléphone. En cas d'échec,
le dialogue de diagnostic ou `$HOME/.openttd/startup.log` doit être
transmis.

Le code des fenêtres rétractables (V2.3) est compilé et les 95 tests
unitaires locaux passent. Le workflow Android a produit l'APK 18 :

- exécution GitHub Actions : `43486729404` ;
- artefact : `openttd-mobile-apk-18` ;
- fichier : `openttd-mobile-arm64.apk`, 15,8 Mo ;
- SHA-256 : `f5a088ad598d14a654b2cfda54a209a728ae542efa94f8425ec5f23a7901cefd`.

Cet APK n'a pas encore été validé sur un véritable téléphone.

Le code V2.4 (fenêtre des options adaptative et défilante) est compilé et
les 95 tests unitaires locaux passent. Le workflow Android a produit
l'APK 19 :

- exécution GitHub Actions : `43507374981` ;
- artefact : `openttd-mobile-apk-19` ;
- fichier : `openttd-mobile-arm64.apk`, 15,8 Mo ;
- SHA-256 : `c84e0ff9b393ecb3f0c30b68eb251fcb7239abee25f2e79c949d414d1a0670ef`.

Cet APK n'a pas encore été validé sur un véritable téléphone en portrait
et en paysage.

Le code V2.5B (fiabilisation des gestes et des coordonnées) est compilé et
les 105 tests unitaires locaux passent. Le workflow Android a produit
l'APK 20 :

- exécution GitHub Actions : `43580154189` ;
- artefact : `openttd-mobile-apk-20` ;
- fichier : `openttd-mobile-arm64.apk`, 15,8 Mo ;
- SHA-256 : `aae37d60909bd5451bd8a457a8c93d89d81c5faad27fc98159de5419abb4e619`.

Cet APK doit encore être validé sur téléphone, en particulier les rotations
et les gestes à plusieurs doigts.

Le code V2.5C (annulation sécurisée et déplacement à deux doigts) est compilé
et les 107 tests unitaires locaux passent. Le workflow Android a produit
l'APK 21 :

- exécution GitHub Actions : `43583961082` ;
- artefact : `openttd-mobile-apk-21` ;
- fichier : `openttd-mobile-arm64.apk`, 15,8 Mo ;
- SHA-256 : `2547c950996f6e296c26e4f555ddbee3b97c4414bf91721328e6973dda941ed6`.

Cet APK doit être validé sur téléphone : tracé rail interrompu par un second
doigt, déplacement à deux doigts avec un outil actif, relâchement partiel,
rotation, et constructions immédiates.

Le correctif V2.5C-FIX réinitialise complètement l'état de sélection de tuiles
lors d'une annulation, afin d'éviter les coordonnées de tuile invalides lors
du prochain rafraîchissement. Le code est compilé et les 108 tests unitaires
locaux passent. Le workflow Android a produit l'APK 22 :

- exécution GitHub Actions : `43587650648` ;
- artefact : `openttd-mobile-apk-22` ;
- fichier : `openttd-mobile-arm64.apk`, 15,8 Mo ;
- SHA-256 : `8064cb5b97d5bf0ab2018fd694221c0653fa06ddbe86482fca37afb1b4648223`.

Cet APK doit être validé sur téléphone : annulation d'un tracé rail par un
second doigt, absence de crash, puis nouveau tracé et déplacement à deux
doigts.

## Données de jeu

OpenTTD a besoin d'un baseset graphique (OpenGFX). Au premier lancement,
la fenêtre de démarrage d'OpenTTD propose de télécharger les données
officielles (connexion Internet requise). Les fichiers sont stockés dans
le dossier privé de l'application (`$HOME/.openttd`).

Les fichiers indispensables au démarrage (polices, métadonnées de baseset,
`openttd.grf`, `opntitle.dat`, sons factices, traductions) sont désormais
empaquetés dans l'APK et extraits une fois par version vers
`$HOME/.openttd/{baseset,lang}`. Sans eux, OpenTTD quittait silencieusement
avant d'afficher la moindre fenêtre.

## Diagnostic d'un démarrage raté

Le lancement natif écrit `startup.log` dans le stockage privé de
l'application. Un `STARTUP_COMPLETE` y est ajouté dès que la boucle
principale de la vidéo est atteinte. En cas de plantage natif, un
`crash.log` contient le signal et une pile d'appels brute ; logcat reçoit
en plus le tombstone Android habituel.

Au lancement suivant, si le démarrage précédent ne contient pas
`STARTUP_COMPLETE` ou si un crash a été enregistré, l'application affiche
un dialogue avec le rapport et les boutons `Copy` / `Share` / `Continue`.

Sans PC, le rapport peut être copié depuis ce dialogue. Avec le débogage
sans fil et Termux :

```sh
pkg install android-tools
adb pair <ip>:<port> <code>      # une seule fois
adb connect <ip>:<port>
adb logcat -s OpenTTD SDL AndroidRuntime DEBUG libc
```

## Limites connues de cette première version

- Seule l'architecture `arm64-v8a` est produite.
- HarfBuzz et ICU ne sont pas compilés pour Android : le rendu des
  écritures complexes (arabe, etc.) utilise le chemin de repli d'OpenTTD.
- Le sélecteur de stations ne liste que les stations ; les dépôts,
  waypoints, bouées, le dépôt le plus proche et les ordres conditionnels
  restent accessibles via `Go To` ou la carte.
- La liste applique la compatibilité de tous les véhicules partageant les
  ordres : une station utilisable par le véhicule courant mais pas par un
  véhicule partagé est masquée.
- Les fenêtres modales, les dialogues critiques, les fenêtres ayant un
  enfant ouvert et les fenêtres pendant un placement actif ne peuvent pas
  être rétractées. Le défilement des onglets se fait à la molette ou au
  geste à deux doigts, sans barre de défilement dédiée.
- La fenêtre des options n'a pas encore été validée sur un véritable
  téléphone en portrait et en paysage. Les listes déroulantes très longues
  sont plafonnées à la largeur de l'écran ; leur contenu reste accessible
  dans la liste déroulante.
- L'annulation V2.5C couvre les outils à tracé. Les outils à exécution
  immédiate (dépôt, tunnel, industrie, etc.) ne sont pas encore soumis à une
  confirmation et peuvent agir dès le premier clic/seuil. Les mutations
  directes de l'éditeur (forêt, terrain) ne sont pas annulables.
- L'APK expérimental est signé avec la clé publique fournie ; ne pas
  l'utiliser pour une distribution de production.
- Pas encore d'intégration du cycle de vie Android (pause/reprise
  explicite) au-delà de ce que SDL gère.
- Les gestes sont globaux ; les cas particuliers (glisser-déposer de
  véhicules, sélection de zone) reposent sur la traduction en souris et
  devront être validés fenêtre par fenêtre sur appareil.
