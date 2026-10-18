# Preparare il PSARC e avviare Strikers Vita

Questa guida riguarda il runtime **VitaGL attuale**, con grafica nativa,
audio PCM e video H.264. Lo script consigliato e'
[`prepare_strikers_vita.py`](smstrikers-port/tools/prepare_strikers_vita.py).
Prepara e verifica l'archivio completo; non basta rinominare una ISO o creare
un PSARC contenente soltanto i file GameCube originali.

## Requisiti sul PC

- Una propria immagine **ISO/GCM GameCube non compressa** di Super Mario
  Strikers / Mario Smash Football. ID riconosciuti: `G4QE01` (USA), `G4QP01`
  (Europa), `G4QJ01` (Giappone). Il riconoscimento dell'ID non garantisce che
  ogni revisione o lingua sia stata validata in partita su Vita.
- Python **3.10 o successivo**, CMake **3.24 o successivo**, un compilatore
  **C++20** e FFmpeg con encoder **libx264**. Consigliato FFmpeg 6 o successivo.
- macOS o Linux; su Windows usare **WSL2** con gli strumenti Linux.
- Connessione Internet alla prima compilazione per scaricare
  gli header xxHash, fissati alla versione 0.8.3 con controllo SHA-256.
- Spazio libero per ISO, estrazione, conversioni temporanee e archivio finale.
  Riservare almeno **8 GiB liberi oltre alla ISO** sul PC; sulla Vita riservare
  circa **2 GiB** per il PSARC, piu' lo spazio per applicazione e salvataggi.
  La dimensione esatta dipende dal disco e dalle opzioni di conversione.

Non servono VitaSDK, SDL, Aurora, librerie Python aggiuntive o una build
desktop del gioco per eseguire questo comando.

Su macOS, installare gli Xcode Command Line Tools (`xcode-select --install`)
e, se si usa Homebrew:

```sh
brew install python cmake ffmpeg
```

Su Ubuntu/WSL2, con una distribuzione che fornisca le versioni richieste:

```sh
sudo apt update
sudo apt install build-essential cmake python3 ffmpeg
```

Controllare le versioni con `python3 --version`, `cmake --version` e
`ffmpeg -version`. RVZ, NKit e altri formati compressi devono essere prima
convertiti in un'immagine GameCube completa con uno strumento adatto.

## 1. Creare l'archivio

Scaricare/clonare questa repository, aprire un terminale nella sua radice
ed eseguire, adattando i due percorsi:

```sh
python3 smstrikers-port/tools/prepare_strikers_vita.py \
  --iso "/percorso/Mario Smash Football.iso" \
  --output "local-data/sms.psarc"
```

Su WSL2 un disco Windows e' accessibile, per esempio, come
`/mnt/c/Games/Mario Smash Football.iso`. Tenere repository, build e output
nel filesystem Linux puo' ridurre il costo delle molte operazioni sui file.

Lo script compila i due convertitori host in
`smstrikers-port/build-asset-tools`, estrae il disco in una directory
temporanea, prepara modelli e texture per il renderer, converte tutto
l'audio previsto dalla pipeline in PCM e i filmati in H.264 Baseline,
poi crea e verifica il PSARC. Le directory temporanee vengono rimosse
alla conclusione; la ISO sorgente resta intatta.

L'output finale comprende:

```text
local-data/
  sms.psarc
  sms.psarc.json
```

Il file JSON contiene identita' della sorgente, SHA-256 dell'archivio e
resoconto delle conversioni. Conservarlo sul PC. Non occorre copiarlo sulla
Vita. Il comando puo' richiedere diversi minuti e stampa l'hash finale
solo dopo la verifica. Un output esistente viene rifiutato: per rigenerarlo
intenzionalmente aggiungere `--replace`.

Opzioni facoltative:

```sh
python3 smstrikers-port/tools/prepare_strikers_vita.py \
  --iso "/percorso/gioco.iso" --output "local-data/sms.psarc" \
  --jobs 4 --video-crf 18 --ffmpeg "/percorso/ffmpeg"
```

`--jobs` controlla compilazione e conversione video; `--video-crf` ha valore
predefinito 18. Lasciare questi valori per la prima preparazione.
ISO, PSARC e conversioni generate sono esclusi da Git.

## 2. Verificare un archivio gia' preparato

```sh
python3 smstrikers-port/tools/prepare_strikers_vita.py \
  --verify "local-data/sms.psarc"
```

Questo comando richiede il JSON accanto al PSARC, confronta il suo SHA-256,
controlla il marker di grafica nativa atteso dal runtime e legge/verifica
il contenitore completo. Non necessita dei convertitori o di FFmpeg.
La verifica del file non sostituisce una prova di avvio e di partita.

La pipeline a basso livello resta disponibile in
[`vita_prepare_assets.py`](smstrikers-port/tools/vita_prepare_assets.py), ma
le sue opzioni consentono anche archivi generici o formati storici che
non soddisfano il runtime attuale. Per un nuovo setup usare il comando
di questa guida, che seleziona `--native-graphics-compiler`, audio `all`
e video nativo insieme. Il vecchio `--native-compiler` da solo non basta.

## 3. Installare sulla PS Vita

La console deve poter eseguire homebrew e installare VPK tramite VitaShell.
Il renderer richiede anche il compilatore shader `libshacccg.suprx`
estratto e decrittato sulla console, normalmente in
**`ur0:data/libshacccg.suprx`**. Si vedano i requisiti ufficiali di
[vitaGL](https://github.com/Rinnegatamante/vitaGL#prerequisites) e il progetto
[ShaRKF00D](https://github.com/OsirizX/ShaRKF00D) per l'estrazione.

1. Installa il **VPK completo** di Strikers Vita con VitaShell.
   Il titolo dell'app e' `Super Mario Strikers Vita`, Title ID `SMSVITA01`.
2. Crea la directory `ux0:data/strikersVita/` con VitaShell o tramite FTP/USB.
3. Copia **`sms.psarc`** in quella directory, senza estrarlo.
4. Copia [`strikers-vitagl.ini.example`](smstrikers-port/strikers-vitagl.ini.example)
   nella stessa directory, rinominandolo **`strikers.ini`**.
5. Verifica che nell'INI ci sia una sola voce `asset_archive` e che punti
   al percorso indicato sotto. Se esiste gia' un INI personale, conservarne
   una copia prima di sostituirlo.
6. Avvia il gioco dalla sua bolla nella LiveArea e segui le schermate del gioco.

La disposizione finale e':

```text
ux0:data/strikersVita/
  sms.psarc
  strikers.ini
```

Impostazioni essenziali della configurazione normale:

```ini
asset_archive = ux0:data/strikersVita/sms.psarc
vita_native_audio = 1
vita_native_video = 1
vita_test_match = 0
vita_frameskip = 0
diagnostics = 0
vitagl_timing = 0
vitagl_affine_uv = 0
vitagl_vertex_fog = 0
```

La partita di test si abilita solo tramite INI quando serve una misura;
non e' necessaria per l'avvio normale. Gli esperimenti shader rimangono
disabilitati nella configurazione distribuita.

## 4. LiveArea: icona, sfondo e pulsante di avvio

I sorgenti si trovano in
[`smstrikers-port/assets/vita/sce_sys/`](smstrikers-port/assets/vita/sce_sys/):

| File nel VPK | Funzione |
| --- | --- |
| `sce_sys/icon0.png` | Icona della bolla, 128 x 128 |
| `sce_sys/pic0.png` | Immagine dell'app, 960 x 544 |
| `sce_sys/livearea/contents/template.xml` | Layout `a1` e riferimenti alle immagini |
| `sce_sys/livearea/contents/bg.png` | Sfondo LiveArea, 840 x 500 |
| `sce_sys/livearea/contents/startup.png` | Immagine del pulsante di avvio, 280 x 158 |

Il template associa esplicitamente `bg.png` allo sfondo e `startup.png`
al gate di avvio. Le immagini sono PNG indicizzati, il template e' UTF-8
con terminazioni CRLF e `content-rev="3"`. Questi vincoli seguono le
[indicazioni VitaSDK](https://github.com/vitasdk/samples#notes-on-images).
`template.xml` non controlla l'icona della bolla: quella dipende da `icon0.png`.

La build controlla asset sorgenti e contenuto del VPK, inclusa la
corrispondenza tra `eboot.bin` e SELF appena generato. Controllo manuale:

```sh
python3 smstrikers-port/tools/validate_vita_package.py \
  --vpk smstrikers-port/build-vita-gxm-latest/strikers_vita.vpk \
  --self smstrikers-port/build-vita-gxm-latest/strikers_vita.self
```

Per aggiornare la LiveArea installare il VPK completo. La sola copia di
`eboot.bin`, usata durante lo sviluppo, non aggiorna gli asset della bolla.
Chiudere l'app e la relativa pagina LiveArea prima di reinstallare.
Se la shell continua a mostrare la grafica precedente, riavviare la console
dopo la reinstallazione e ricontrollare i file installati sotto
`ux0:app/SMSVITA01/sce_sys/`. Evitare di disinstallare l'app come primo
tentativo, per non rimuovere dati che si vogliono conservare.

## Problemi frequenti

| Sintomo | Controllo |
| --- | --- |
| ISO rifiutata immediatamente | Verificare formato completo e ID; non passare RVZ/NKit o un disco di un altro gioco. |
| Compilatore, CMake o FFmpeg non trovato | Installare le dipendenze nel medesimo ambiente del terminale; su Windows usare WSL2. |
| FFmpeg senza `libx264` | Usare una build di FFmpeg che includa quell'encoder. |
| Output gia' esistente | Scegliere un nuovo percorso oppure usare `--replace` intenzionalmente. |
| SHA-256 diverso dal JSON | Non usare quel trasferimento/output; rigenerare o ricopiare da una coppia verificata. |
| Archivio senza marker nativo | Rigenerare con `prepare_strikers_vita.py`; un PSARC generico o di una vecchia pipeline e' insufficiente. |
| Mancato avvio o errore di caricamento | Controllare percorso INI, PSARC completo, modulo shader e `ux0:data/strikersVita/vitagl.log`. |
| Bolla o LiveArea ancora precedenti | Verificare e reinstallare il VPK completo; il template e' sotto `sce_sys/livearea/contents/`. |
