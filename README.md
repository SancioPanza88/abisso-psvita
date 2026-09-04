# ABISSO — Port PSVita (VPK)

Port **1:1** del roguelike multiplayer web su **PS Vita** (homebrew `.vpk`).

- Gioco originale: versione web in un unico file (`index.html`, ~9000 righe JS) + sprite in `assets/`.
- Questo port riscrive il gioco in **C + SDL2/SDL2_image** con gli **stessi asset PNG originali**, stessa logica (stesso RNG `FNV-1a + mulberry32`, stesso scaling, stesse 9 classi, stessi mostri/boss, boss ogni 5 piani in ordine D,X,L,M,R,K, mercante, forzieri, rarità, permadeath, modalità C64 con stanza `64`/`c64`, viste topdown/isometrica).

## Stato rete (onesto)

La versione web usa **Trystero + Nostr + WebRTC** (CDN + relay `wss://`). Su PSVita **non esiste uno stack WebRTC** di sistema e la CPU/RAM non reggono `libwebrtc`: il multiplayer P2P **non è disponibile** sulla Vita.

Per restare 1:1 dove conta:
- **Stesse tabelle e formule**: classi, mostri, boss, equip, rarità, mercante, pozioni, FOV, dungeon — copiati dalla web.
- **Mondo fresco casuale** a ogni run, come il `bootstrapFreshWorld` della web.
- Badge `OFFLINE` in HUD.
- Tutte le meccaniche single-player sono complete (classi, abilità, boss, mercante, loot, record).

## Controlli (Vita)

| Vita | Azione (web) |
|---|---|
| Stick sinistro / D-pad | WASD/frecce (movimento) |
| X (croce) | Spazio (attacco) |
| O (cerchio) | E (interagisci: scale/forzieri/mercante) |
| Quadrato | Q (pozione HP) |
| Triangolo | R (pozione mana) |
| R1 | F (abilità classe) |
| L1 | V (vista topdown/isometrica) |
| Start | M (minimappa) |
| Select | aiuto |
| Touch / mouse | menu, OSK, mercante |

Nome/stanza si inseriscono con la tastiera su schermo (touch) o tastiera USB/PC. Su PC funziona anche la digitazione diretta.

## Build locale (PC, test)

Serve SDL2 + SDL2_image.

```sh
cmake -B build-pc -DCMAKE_BUILD_TYPE=Release
cmake --build build-pc -j
./build-pc/abisso   # asset caricati da ./assets
```

## Build VPK (Vita)

Ogni `push` compila il `.vpk` in GitHub Actions (immagine `vitasdk/vitasdk`):

1. Fai push su `main`.
2. Apri la tab **Actions** → workflow **Build VPK** → scarica l'artifact **abisso-vpk**.
3. I tag `v*` creano anche una Release con il `.vpk` allegato.

In locale con Docker:

```sh
docker run --rm -v "$PWD:/workspace" vitasdk/vitasdk:latest \
  sh -c 'cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)'
```

## Installazione su Vita

1. Vita moddata (HENkaku/h-encore/Trinity), VitaShell installato.
2. Copia `abisso.vpk` in `ux0:data/` via FTP/USB.
3. In VitaShell premi X sul `.vpk` per installare la bolla **Abisso** (`ABSS00001`).
4. Salvataggi in `ux0:data/ABISSO/save.txt`.

## Struttura

```
src/        main.c, abisso.h, data.c, game.c, render.c, audio.c, input.c (+headers)
assets/     sprite PNG originali (copiati 1:1 dalla web)
sce_sys/    icon0, livearea
.github/    workflow build VPK
```

## Parità contenuti

- 9 eroi con statistiche/abilità originali (Guerriero, Ladro, Mago, Ranger, Paladino, Negromante, Bardo, Monaco, Prof al plasma).
- 17 tipi comuni + 6 boss con attacchi dedicati (soffio, cariche, evocazioni, telegrafi).
- Affix (veloce/esplosivo/rigenerante), split gelatina, veleno, lifesteal, dash, erratic.
- Torce tremolanti + fog-of-war, particelle, screen-shake, float damage, boss-bar, minimappa, log/toast/loot-banner, downed/permadeath, record best-depth.
- Audio 100% sintetico (SFX + musica generativa, variante chiptune in C64) come il WebAudio originale.
