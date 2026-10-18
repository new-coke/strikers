# Strikers Vita

Port di **Super Mario Strikers / Mario Smash Football** per PlayStation Vita.
Il runtime Vita attuale usa VitaGL, asset grafici preparati sul PC, audio PCM
e filmati H.264. Il progetto e' in sviluppo: i 60 FPS sono un obiettivo,
non una prestazione raggiunta nelle partite misurate.

- [Preparazione del PSARC e installazione sulla Vita](ASSET_SETUP.md)
- [Stato delle prestazioni e prossimi interventi](PERFORMANCE.md)
- [Configurazione normale di esempio](smstrikers-port/strikers-vitagl.ini.example)
- [Sorgenti LiveArea](smstrikers-port/assets/vita/sce_sys)

Il gioco richiede una propria immagine GameCube compatibile. ISO, PSARC e asset
estratti non sono distribuiti in questa repository e non fanno parte del VPK.

## Avvio rapido

Con Python 3.10+, CMake 3.24+, un compilatore C++20 e FFmpeg con `libx264`,
dalla radice della repository:

```sh
python3 smstrikers-port/tools/prepare_strikers_vita.py \
  --iso "/percorso/Mario Smash Football.iso" \
  --output "local-data/sms.psarc"
```

Installa il VPK e copia `sms.psarc` in **`ux0:data/strikersVita/sms.psarc`**.
Copia il file di configurazione di esempio come
**`ux0:data/strikersVita/strikers.ini`**. La [guida completa](ASSET_SETUP.md)
include dipendenze, verifica dell'archivio e ripristino degli asset LiveArea.

## Build Vita per sviluppatori

Nell'ambiente macOS/VitaSDK configurato per il progetto:

```sh
smstrikers-port/build-vita-vitagl.sh
```

La build riusa `smstrikers-port/build-vita-gxm-latest` e disabilita esplicitamente
il profiler. Il VPK risultante e' `strikers_vita.vpk` in quella directory.
Lo script presuppone VitaSDK in `/usr/local/vitasdk`; non serve per la sola
preparazione degli asset sul PC.

Le licenze e gli avvisi delle dipendenze sono conservati nei rispettivi
file della repository. Il renderer usa [vitaGL](https://github.com/Rinnegatamante/vitaGL).
