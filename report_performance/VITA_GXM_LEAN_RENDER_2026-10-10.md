# Strikers Vita: profilo GXM lean e isolamento degli esperimenti GLX
Data: 2026-10-10

## Motivazione e baseline verificabile

La precedente baseline gameplay intorno a 25 FPS era una lettura manuale, non un A/B abbinato. L'A/B quiet Vita del 9-10 ottobre (stesso SELF, 600 frame di warmup e 300 campioni, CPU 444 MHz/GPU 222 MHz, diagnostics OFF) riportava **20,265 FPS GX/GXM ordinario** e **15,098 FPS con ricette native/cache/uniform-builder**. Quindi non abbiamo motivo di tenere R1-R3 attivo in una build da usare per giocare. Il test R3-A early aveva zero draw ammesse. Il successivo controllo quiet GX era inizialmente privo del CSV; è stato recuperato dopo il freeze Lean ed è analizzato sotto.

## Modifica eseguita

**Nuova opzione:** `STRIKERS_VITA_GXM_LEAN_RENDER=ON` (opt-in dopo un freeze hardware, non si applica al desktop). In CMake è propagata come `PORT_DEFINES` ai translation unit del gioco, non altera Aurora-Vita, l'owner GXM, i worker GX, il fallback GX, la geometria statica valida, la traduzione TEV o il fix dei reference count atomici.

Nel profilo lean i sorgenti `glxSend.cpp`, `glxDisplayList.cpp`, `glxMemory.cpp` **non compilano**:
- dispatch/registrazione/ricerca/cache/replay per native-model R1/R2/R3-A e A6; il codice GXCallDisplayList esistente resta il percorso normale.
- registrazione di generazioni delle DL e callback di retirement introdotte specificamente per R3.
- timer/stage accounting per packet Characters/Shadowed (gli entry point usati dal resto del gioco restano disponibili come stub senza lavoro).
- dump per-draw delle primitive e del routing 3D.
- scansioni e OSReport delle texture mancanti nella build lean, senza rimuovere né cambiare la risoluzione delle texture o il fallback bianco/magenta.

Nella variante sperimentale (`STRIKERS_VITA_GXM_LEAN_RENDER=OFF`) i meccanismi originali restano compilati e accessibili via INI. Anche in tale variante la stampa delle texture mancanti viene ridotta quando `diagnostics=0`.

**Script manuale** `smstrikers-port/build-vita-native-gxm.sh`: dopo il freeze il default Lean è tornato OFF e l'assetto di controllo R3-A è fissato esplicitamente: GX async ON, DIRECT_STREAM_WRITE OFF, DIRECT_DRAW_SUBMIT ON, DISTINCT_CPU_CORES OFF, IMMEDIATE_DRAW_VIEW OFF, LTO jobs auto. Il candidate Lean può essere richiesto con `STRIKERS_GXM_LEAN_RENDER=ON`, senza modificare altri flag. Sono valori al momento della configurazione, non il contenuto del SELF già installato.

**Nota di compatibilità:** con lean ON l'INI può contenere flag `gxm_native_model_*`/A6, ma tali flag non possono riattivare codice non compilato. Il profilo è opt-in inverso al tempo di configurazione CMake, non al runtime. Resta selezionato solo GXM, non VitaGL.

## Verifiche effettuate, senza generare una build Vita

- Sintassi reale dei tre TU GLX con VitaSDK GCC 15.2, `-fsyntax-only`: **6/6 PASS** (3 lean e 3 sperimentali). Nessun ELF, SELF o VPK prodotto.
- `cmake -S ... -B build-vita-gxm-lean-configcheck` completata **senza build**: `STRIKERS_VITA_GXM_LEAN_RENDER=ON`, `AURORA_VITA_RENDERER_GXM=1`, `AURORA_VITA_ASYNC_GX=1`, direct-stream e direct-submit OFF, `STRIKERS_VITA_GXM_LEAN_RENDER=1` visibile nei `CXX_DEFINES` di `strikers_game`.
- Aurora-Vita host `ctest`: **37/37 PASS**, durata ~5,90 secondi.
- Test host `vita_default_config_test.c` con `src/platform/config.c`: **PASS**.
- `zsh -n build-vita-native-gxm.sh`, `git diff --check` root e submodule: **PASS**.
- Nessun cambiamento nel codice Aurora-Vita o nelle modifiche sperimentali preesistenti. Nessun commit/push, deploy, modifica INI o build Vita eseguita.

## Valutazione prestazionale e gate

È **dimostrato che il producer del gioco non chiama più R1/R3/replay né i timer di packet quando viene compilato lean**. Non abbiamo ancora un nuovo ELF/SELF lean né una misura hardware: **nessuna stima numerica di FPS recuperati può essere attribuita a queste modifiche**. Le altre strutture opzionali di Aurora sono lasciate compatibili per non introdurre regressioni agli altri consumer.

Prossimo step affidabile: non reinstallare il candidato Lean fino a quando non siano isolati il code-path e tutti gli switch di compilazione; serve successivamente un test controllato mantenendo il resto identico a R3-A. Non riattivare esperimenti di scheduler/direct draw per coincidenza attraverso cache CMake residue. Evitare affermazioni di 60 FPS senza frame time riproducibili.

## Freeze hardware della build Lean e ripristino (10 ottobre, sera)

Il SELF Lean `build-vita-gxm-latest/strikers_vita.self`, SHA-256 `b57c6bdb2c9c81d599c70d84f5ca1d272a71ce62016f5013bb42202d470b1cc2` (3.573.020 byte), è stato installato alle 23:16 circa sulla PS Vita. L'utente ha segnalato freeze. La porta FTP 1337 inizialmente rifiutava le connessioni. A connettività ristabilita non risultavano nuovi `psp2core` rispetto al dump del 10 ottobre 13:40; `diagnostics=0` impedisce di ottenere il log ordinario del run. Non si dispone dunque del punto esatto di blocco.

**Importante: NON era un A/B isolato.** Il `CMakeCache.txt` della build Lean aveva ereditato altri switch dal preset Aurora candidate, assenti nell'ultima R3-A con test gameplay completi:

| Opzione | R3-A precedente | Lean che ha freezato |
| --- | --- | --- |
| `STRIKERS_VITA_GXM_LEAN_RENDER` | non presente / OFF | ON |
| `AURORA_VITA_DISTINCT_CPU_CORES` | OFF | **ON** |
| `AURORA_VITA_GXM_IMMEDIATE_DRAW_VIEW` | OFF | **ON** |
| `STRIKERS_VITA_GXM_DIRECT_DRAW_SUBMIT` | ON | **OFF** |
| `STRIKERS_VITA_LTO_JOBS` | auto | 1 |

La modalità distinct core cambia la strategia CPU dei helper; immediate draw view viene attivato solo quando un percorso direct-synchronous lo usa. Nessuno dei due può essere dichiarato causa del blocco senza dump/trace; **il freeze non dimostra un errore del solo taglio GLX**. Le opzioni candidate nella cache CMake e il cambiamento di direct submit rendono impossibile l'attribuzione causale. Il CMake e lo script sono stati aggiornati per rendere Lean OFF di default e fissare l'assetto R3-A, senza eseguire una nuova build.

Il CSV `r3a_quiet_gx.csv` (SHA-256 `048dde0ec5cb3833e8a6c1331ce0e34f8ed2420d494c407ccefb25ecf47aa404`) è stato recuperato da FTP: timestamp **anteriore al SELF Lean**, quindi è un test della precedente R3-A con ricette native disabilitate. Contiene 300 frame live-play (`match_frame=600..899`), CPU 444 MHz/GPU 222 MHz, `diagnostics=0` e `fps_overlay=0`. Media 52.802 ms/frame = **18,939 FPS**, ultimi 200 frame **18,572 FPS**, mediana 52,66 ms, p95 62,11 ms, peggiore 74,18 ms. CSV e INI recuperati sono salvati in `ab-artifacts/native-model-registry-20261010/device-r3a-lean-freeze/`.

**Ripristino riuscito e verificato con un'altra connessione FTP:** `ux0:/app/SMSVITA01/eboot.bin` è tornato alla R3-A SHA-256 `55897b45ca677e9c62c236bed4f41231c73a0451481043b8b57ce14e18ead1b5` (3.601.063 byte). Anche il backup precedente `eboot.bin.backup-lean-55897b45ca67` è integro. Il binario che ha freezato è conservato in `eboot.bin.backup-freeze-b57c6bdb2c9c`. Nessun cambio all'INI o ai dati del gioco, nessuna nuova build o push.
