# Strikers Vita: pulizia e prova di avvio normale

La build usa VitaGL e parte dal frontend originale, senza partita automatica.
Il proprietario ha confermato: «Menu e comandi regolari, audio presente».

## Pulizia

Sono stati rimossi il renderer applicativo GXM ritirato, i probe, il vecchio ponte
Aurora per Vita, l'overlay diagnostico escluso dalla build, le vecchie opzioni
CMake, il vecchio template INI e gli strumenti/test esclusivi dei percorsi rimossi.
I rami condizionali irraggiungibili sono stati eliminati mantenendo il runtime
VitaGL e i percorsi condivisi usati dalle altre piattaforme. Il checkout legacy
non è più un sottomodulo del progetto; le sue tre modifiche preesistenti sono
preservate byte per byte nella copia locale ignorata.

Gli audit correnti omettono riferimenti esterni e inventari del progetto di
riferimento. I testi storici esportati hanno etichette normalizzate; contatori e
misure numeriche non sono stati reinterpretati come nuove misure. Gli originali
rimangono in un archivio locale ignorato. VitaGL resta identificata come
dipendenza tecnica e le licenze/attribuzioni obbligatorie delle dipendenze sono
conservate. La cronologia Git precedente non è stata riscritta.

## Setup sulla Vita

La cartella precedente è conservata in
`ux0:/data/strikersVita.before-clean-20261015`, inclusi i precedenti INI, log e
salvataggi. I due archivi asset sono stati spostati nella nuova cartella dati,
senza duplicarli o modificarli. La cache shader del solo titolo è stata
conservata nella cartella di backup e rimossa dal percorso attivo; le cache
condivise di altri titoli restano intatte.

Prima dell'avvio la nuova cartella conteneva soltanto `sms.psarc` e
`sms-native-gxm-static.psarc`. Il gioco ha generato da zero il profilo
`2026-10-14-vitagl`: `vita_test_match=0`, `vita_frameskip=0`, `fixed_dt=0`,
audio/video nativi attivi, diagnostica, overlay, profiling, benchmark e catture
spenti. Non è presente un limite a 20 FPS. Il target resta 60 FPS e la simulazione
usa il tempo trascorso indipendentemente dal numero di immagini presentate.

Il primo avvio ha richiesto un'attesa evidente, poi il proprietario ha confermato
menu, comandi e audio regolari. Dopo il primo avvio sono presenti 42 elementi
nella cache shader del titolo. Un secondo avvio, con INI invariato e cache
presente, ha prodotto il primo stato fresco del renderer dopo 8,151 secondi
dalla richiesta Companion, inclusa la conferma LiveArea al secondo 3,611.
Questa misura riguarda l'arrivo del primo stato del renderer, non il tempo
esatto alla comparsa del menu. Il primo avvio non è stato cronometrato in modo
strumentato: il ritardo non è attribuito interamente alla compilazione shader.

## Verifiche

- 38 test host registrati passati; otto test esclusivi dei percorsi ritirati eliminati.
- Driver VitaGL e frontend differito passati con AddressSanitizer/UndefinedBehaviorSanitizer.
- Build Release e pacchetto VPK completati nella directory condivisa.
- Le quattro flag richieste sono attive su tutte le 30 unità VitaGL; resta attivo
  `STORE_DEPTH_STENCIL=1` per la correttezza della profondità.
- Zero simboli grafici Aurora/SDL/GX e zero chiamate a glFinish/glFlush nell'ELF.
- SELF installata verificata byte per byte; eboot del VPK identico.
- INI generato verificato e invariato al secondo avvio. Nessun nuovo crash o errore GPU rilevato.

Questa prova valida il setup pulito e il frontend normale. Non misura un
incremento degli FPS e non sostituisce una prova completa di una partita avviata
manualmente, dei salvataggi o di tutte le modalità di gioco.

Le identità esatte di build, installazione, configurazione e avvii sono in
[evidence/CLEAN_SETUP_2026-10-15](evidence/CLEAN_SETUP_2026-10-15).
