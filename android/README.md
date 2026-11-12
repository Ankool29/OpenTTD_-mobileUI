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

1. Android SDK 34, build-tools 34.0.0, NDK r26b ;
2. vcpkg au commit épinglé dans `vcpkg.json`, triplet
   `arm64-android-shared` (dépendances partagées pour que SDL2 soit
   chargeable par Java) ;
3. CMake avec le NDK (`ANDROID_ABI=arm64-v8a`,
   `ANDROID_PLATFORM=android-28`, `ANDROID_STL=c++_shared`) ;
4. récupération des classes Java SDL et de l'icône depuis la source SDL
   `2.32.10` ;
5. assemblage Gradle et signature de l'APK.

L'APK est publié comme artefact `openttd-mobile-apk-<run>` avec son
empreinte SHA-256.

### Reproduire localement

Prérequis : JDK 17, SDK Android, NDK r26b, CMake ≥ 3.17, Ninja, vcpkg.

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

## Données de jeu

OpenTTD a besoin d'un baseset graphique (OpenGFX). Au premier lancement,
la fenêtre de démarrage d'OpenTTD propose de télécharger les données
officielles (connexion Internet requise). Les fichiers sont stockés dans
le dossier privé de l'application (`$HOME/.openttd`).

## Limites connues de cette première version

- Seule l'architecture `arm64-v8a` est produite.
- L'APK expérimental est signé avec la clé publique fournie ; ne pas
  l'utiliser pour une distribution de production.
- Pas encore d'intégration du cycle de vie Android (pause/reprise
  explicite) au-delà de ce que SDL gère.
- Les gestes sont globaux ; les cas particuliers (glisser-déposer de
  véhicules, sélection de zone) reposent sur la traduction en souris et
  devront être validés fenêtre par fenêtre sur appareil.
