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

- exécution GitHub Actions : `43381536634` ;
- artefact : `openttd-mobile-apk-14` ;
- fichier : `openttd-mobile-arm64.apk`, 16 Mo ;
- SHA-256 : `9a630f6136f08c0bbf95eb02517177bd6c2aa135fec9ec556b9f4a45ee6ab2a9`.

Les bibliothèques natives sont vérifiées : `libmain.so` exporte `SDL_main`,
`AndroidStartupMarkComplete` et `AndroidLogError`, son nom de bibliothèque
est `libmain.so`, et toutes ses dépendances (`libSDL2.so`,
`libc++_shared.so`, `libfreetype.so`, `libcurl.so`, etc.) sont présentes
dans l'APK. Les segments `LOAD` sont alignés sur 16 Ko. L'APK contient les
66 fichiers de langue et les fichiers de baseset nécessaires au premier
démarrage. `minSdk` et l'API native sont alignés sur 28.

Cette version n'a pas encore été exécutée sur un appareil dans
l'environnement de compilation ; le test de lancement doit être fait sur
téléphone. En cas d'échec, le dialogue de diagnostic ou
`$HOME/.openttd/startup.log` doit être transmis.

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
- Le premier lancement doit encore être validé sur appareil après le
  correctif de démarrage ; l'environnement de compilation ne permet pas
  d'exécuter Android.
- L'APK expérimental est signé avec la clé publique fournie ; ne pas
  l'utiliser pour une distribution de production.
- Pas encore d'intégration du cycle de vie Android (pause/reprise
  explicite) au-delà de ce que SDL gère.
- Les gestes sont globaux ; les cas particuliers (glisser-déposer de
  véhicules, sélection de zone) reposent sur la traduction en souris et
  devront être validés fenêtre par fenêtre sur appareil.
