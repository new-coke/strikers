# Strikers Vita

Port di **Super Mario Strikers / Mario Smash Football** per PlayStation Vita.
Il runtime usa VitaGL, grafica preparata sul PC, audio PCM e filmati
**MP4 H.264/AAC letti dal PSARC tramite SceAvPlayer**. Il progetto e' in
sviluppo: l'obiettivo intermedio e' **30 FPS stabili** nelle cutscene
renderizzate, poi 60 FPS. Questi obiettivi non sono ancora raggiunti.

- [Preparazione del PSARC e installazione sulla Vita](ASSET_SETUP.md)
- [Misure, stato MP4 e obiettivo 30 FPS](smstrikers-port/performance/MIDTERM_30FPS_MP4_2026-10-22.md)
- [Candidata locale per i tester](TESTING_RC.md)
- [Configurazione normale di esempio](smstrikers-port/strikers-vitagl.ini.example)
- [Sorgenti LiveArea](smstrikers-port/assets/vita/sce_sys)

Il gioco richiede una propria immagine GameCube compatibile. ISO, PSARC e asset
estratti non sono distribuiti in questa repository e non fanno parte del VPK.

## Avvio rapido

Sul PC servono Python 3.10+, CMake 3.24+, un compilatore C++20, FFmpeg
con gli encoder `libx264` e `aac`, e `ffprobe`. Su Windows usare WSL2.
Dalla radice della repository:

```sh
python3 smstrikers-port/tools/prepare_strikers_vita.py \
  --iso "/percorso/Mario Smash Football.iso" \
  --output "local-data/sms.psarc"

python3 smstrikers-port/tools/prepare_strikers_vita.py \
  --verify "local-data/sms.psarc"
```

Lo script compila i convertitori host, prepara grafica e audio nativi,
converte i filmati in MP4 e verifica l'archivio. Produce `sms.psarc` e
`sms.psarc.json`; il JSON va conservato sul PC per la verifica.
Il formato video predefinito e' `mp4`. Un PSARC generico con i soli file
originali del disco non e' sufficiente per questo runtime.

Sulla Vita:

1. Predisponi VitaShell e `ur0:data/libshacccg.suprx`, come spiegato nella
   [guida di installazione](ASSET_SETUP.md#3-installare-sulla-ps-vita).
2. Installa il **VPK completo di questa build**, compatibile con MP4/SceAvPlayer.
3. Copia `sms.psarc` in **`ux0:data/strikersVita/sms.psarc`**, senza estrarlo.
4. Copia [la configurazione di esempio](smstrikers-port/strikers-vitagl.ini.example)
   come **`ux0:data/strikersVita/strikers.ini`**.
5. Avvia Strikers Vita dalla LiveArea e segui le schermate originali del gioco.

La [guida completa](ASSET_SETUP.md) contiene dipendenze, trasferimento,
impostazioni INI, aggiornamento senza perdere i salvataggi e problemi frequenti.

## Glifi dei pulsanti

I glifi PlayStation sono attivi per default. Per scegliere i simboli nel file
`ux0:data/strikersVita/strikers.ini`:

```ini
vita_playstation_glyphs = 1
```

Impostare `0` per mantenere i glifi GameCube originali e riavviare il gioco
dopo la modifica. La scelta riguarda i font dei comandi e le immagini delle
legende supportate; non cambia la mappatura dei tasti.

| Comando originale | Pulsante Vita |
| --- | --- |
| A | Croce × |
| B | Cerchio ○ |
| X | Quadrato □ |
| Y | Triangolo △ |
| L / R | L / R |
| Z | SELECT |
| START | START |

I simboli incorporati nei filmati e gli eventuali testi che nominano i tasti
restano parte degli asset originali.

## Build Vita per sviluppatori

Nell'ambiente macOS/VitaSDK configurato per il progetto:

```sh
smstrikers-port/build-vita-vitagl.sh
```

La build riusa `smstrikers-port/build-vita-gxm-latest` e disabilita esplicitamente
il profiler. Il VPK risultante e' `strikers_vita.vpk` in quella directory.
Per una candidata con diagnostiche e profiler disabilitati:

```sh
smstrikers-port/build-vita-vitagl.sh --release-candidate
```

Richiede sorgenti committati e produce accanto al VPK un INI dedicato e
`release-candidate.json` con commit e hash. Il VPK resta locale e va passato
ai tester privatamente, senza caricarlo su GitHub. Vedi [TESTING_RC.md](TESTING_RC.md).

Lo script presuppone VitaSDK in `/usr/local/vitasdk`; non serve per la sola
preparazione degli asset sul PC.

Le licenze e gli avvisi delle dipendenze sono conservati nei rispettivi
file della repository. Il renderer usa [vitaGL](https://github.com/Rinnegatamante/vitaGL).
