# Prova GXM diretta senza GX: contratto nativo isolato
Data: 13 ottobre 2026

## Scopo reale

L'obiettivo di questa prima prova e' verificare che una draw gia' descritta in
termini GXM possa essere inviata alla GPU Vita senza percorrere
`glx_SendFrame_cb -> GXBegin/GXCallDisplayList -> GX FIFO -> GX decoder ->
DrawSink::submit`.

**Non e' ancora la conversione dell'intero gameplay di Strikers.** I modelli
del gioco richiedono materiali, texture, palette di skinning, effetti, HUD,
render target EFB e ordine di rendering: nessuno di questi puo' essere
semplicemente eliminato senza produrre frame mancanti o scorretti.

Questo esperimento e' volutamente *standalone* e senza fallback. Fallire o
terminare il probe non cancella la build stabile. Nessun cambiamento nei
sorgenti Aurora-Vita o nelle funzionalita' normali di Strikers.

## Architettura della draw

- **Doppio gate**: configurazione CMake
  `STRIKERS_VITA_GXM_DIRECT_PROBE=ON` (default OFF) e INI
  `gxm_direct_probe=1`. Se uno dei due e' assente, il gioco prosegue nel
  percorso originale.
- **Ingresso anticipato**: in `src/Game/main.cpp`, subito dopo
  `PortConfigLoad()` e prima di inizializzare audio, engine, Aurora GX o
  worker FIFO.
- **Contratto dei dati GPU**:
  `include/port/native_gxm_direct_data.hpp` ammette soltanto un
  `MeshView` di vertici *gia' impacchettati* (posizione float4 + colore
  float4; stride 32), indici U16 e un `gfx::PipelineDesc` immutabile
  unlit con un solo TEV raster-color e zero texture. Controlla limiti
  degli indici e layout; non esegue alcuna conversione dai dati GX.
  I materiali, layout e indici non supportati vengono rifiutati in modo
  esplicito.
- **Prova con fixture**: 8 vertici e 36 indici gia' in formato GXM in
  una piccola mesh colorata. Nella prova non vengono decodificati dati
  GLG/BMD reali.
- **Submission nativa**:
  `src/platform/native_gxm_direct_probe.cpp` inizializza il solo
  `aurora::vita::gxm::Renderer`, crea pipeline e vertex/index buffer una
  sola volta, conserva un `gfx::DrawPacket` e chiama
  `renderer.draw(packet, true)`. Il percorso termina in
  `platforms/vita/gxm/gxm_renderer.cpp:1717` con `sceGxmDraw`.
  Nessuna `GX*`, `GXCallDisplayList`, registrazione recipe o fallback
  per la draw di prova.
- **Pipeline**: la descrizione e' gia' nativa e la compilazione Cg/GXM
  avviene una volta prima del loop. Non c'e' interpretazione GX o TEV
  *per draw*; resta il normale binding di shader/uniformi e lo scheduling
  GXM, necessari anche nel renderer nativo.
- **Frame loop finito**: 60 warmup + 300 campioni di una draw per
  frame, con START+SELECT per interrompere. Lo stadio GPU non e' misurato
  separatamente; il contatore e' la differenza di
  `sceKernelGetProcessTimeWide()` durante submission/present, non un
  timestamp GPU o una misura certificata del frame sul display.
  Il probe termina automaticamente e scrive un solo CSV a conclusione o
  in caso di errore:
  `ux0:data/strikersVita/gxm_direct_probe.csv` con intestazione,
  stato, ragione di errore, clock reali e righe
  `sample,frame_us,native_draws`. Non fa I/O per frame.
- **Oneri invariati**: il backend effettua ancora GXM shader
  patching/binding, uniform uploads, eventuale scene setup e present.
  "Senza traduzioni" significa senza *GX-to-GXM per draw*, non
  assenza di qualsiasi preparazione hardware.

## File modificati o nuovi

- `smstrikers-port/CMakeLists.txt`: gate OFF per default.
- `smstrikers-port/src/Game/main.cpp`: entrata prima di GX/game.
- `smstrikers-port/build-vita-native-gxm.sh`: pass-through esplicito di
  `STRIKERS_GXM_DIRECT_PROBE`, OFF per default, preservando i flag
  sicuri R3-A.
- `smstrikers-port/include/port/native_gxm_direct_data.hpp`: contratto
  typed e fixture prepacked.
- `smstrikers-port/include/port/native_gxm_direct_probe.hpp`: interfaccia.
- `smstrikers-port/src/platform/native_gxm_direct_probe.cpp`: loop
  render e CSV senza fallbacks.
- `smstrikers-port/tests/gxm_direct_probe_contract_test.cpp` e
  `smstrikers-port/tests/CMakeLists.txt`: verifiche host.
- `smstrikers-port/strikers.ini.example`: opzioni e istruzioni.

## Verifiche di codice, non misura FPS Vita

1. Test host del contratto e generazione shader Cg/GXM: PASS.
   `build_tev_cg()` ha prodotto vertice e frammento per input color0,
   con textureMask=0 e colorMask=1. Indici corrotti, buffer incompleti,
   layout/materiale incompatibili sono rifiutati.
2. Cross-compiler VitaSDK GCC 15.2 `-fsyntax-only` per
   `native_gxm_direct_probe.cpp` e `Game/main.cpp` con gate ON: PASS.
3. CMake configure-only del gate ON e dei parametri hardware R3-A:
   PASS, macro `STRIKERS_VITA_GXM_DIRECT_PROBE=1`, backend GXM e
   GX asincrono previsti dal resto della build; il target include il
   nuovo sorgente. Nessun SELF o VPK prodotto.
4. Suite host Aurora-Vita: 38/38 PASS.
5. `git diff --check` root e submodule: PASS.

**Limiti della verifica:** compile/syntax host e shader generator non
garantiscono l'esecuzione della pipeline su hardware. Non abbiamo ancora
provato il rendering, FPS, correttezza dei pixel o comportamento al
termine del test su PS Vita. La fixture di un draw/frame non e' un
benchmark sostitutivo dei ~250-300 draw/frame di Strikers, ne' misura
i costi residui del gioco e della simulazione. Nessun miglioramento di
FPS e' dichiarato.

## Come provarla, solo dopo verifica esplicita e build manuale

Questo comando e' per esecuzione locale da parte dell'utente, *non eseguito*
dall'assistente:

```sh
cd /Users/robin994/Documents/Code/strikersVita/smstrikers-port
STRIKERS_GXM_DIRECT_PROBE=ON ./build-vita-native-gxm.sh
```

Dopo un'eventuale installazione richiesta esplicitamente, porre
`gxm_direct_probe=1` nella parte iniziale dell'INI Vita e riavviare il
titolo. In assenza del gate runtime si avvia Strikers normalmente.
Backup del SELF sempre obbligatorio prima di installare.

## Cosa manca per rimuovere GX dal **gioco completo**

1. Identificare modelli e famiglie reali dominanti (Characters e Shadowed,
   oltre a campo, HUD e menu); non introdurre un fallback invisibile.
2. Compilare offline asset `MeshGpu` completi, non soltanto canonical
   vertex GLG/AVNR: buffer GXM pronti, materiali/shader e layout GPU,
   primitive/index corrette, provenienza, revisioni e durata delle
   risorse. Caricarli una sola volta tramite manifest.
3. Sostituire a monte i packet GX con `NativeInstance`: handle mesh,
   `pipelineKey`, texture residenti, palette/matrici/luci e
   viewport/scissor *aggiornati direttamente* in typed uniforms GXM.
   Matrici/skinning/animazioni e fisica devono continuare a funzionare;
   cambia il trasporto, non la matematica o la resa finale.
4. Creare percorsi GPU per copy EFB/shadowmap e ordine dei pass senza
   reintrodurre GX. Un renderer senza framebuffer copy equivalente puo'
   perdere HUD/ombre anche se ogni draw mesh riesce.
5. Una volta raggiunta la copertura delle draw del gioco al 100% e
   superati i test visivi/host, fare una prova separata in modalita'
   no-fallback con **errore evidente e arresto del frame** sul primo
   materiale/mesh non compilato. Fino a quel momento non usare
   questa modalita' standalone come renderer completo.
6. Misurare prima full-game correctness, poi FPS quiet e CPU/thread,
   confrontando SELF e CMake options invarianti. Target 60 FPS non
   dimostrato.

L'implementazione attuale e' un **proof of execution GXM diretto**,
non un port completato del disegno di Strikers.

## Risultato hardware del primo probe (13 ottobre)

Risultato recuperato dalla PS Vita via FTP, file
`ab-artifacts/native-gxm-direct-probe-20261013/device-run-01/gxm_direct_probe.csv`,
SHA-256 `467379dea54de122f2e0c1250e224e431643a2a281b86a3a9593c5c9c90c3fbb`.
Il recorder indica `status=completed`, `failure=none`, 60 warmup e 300/300
frame campionati, una draw nativa per frame, zero GX e zero fallback nel
modulo standalone. Tempo medio CPU submit+present = **16.66645 ms**,
mediana 16.666 ms, p95 16.710 ms, massimo 16.718 ms, equivalente a
**60.00 FPS a vsync**. Clock effettivi: CPU 444, GPU 222, bus 222,
xbar 166 MHz. Non ci sono misure GPU-clock/occlusione effettiva dei
frammenti, pixel equivalence o carico di gameplay; quindi **non prova
60 FPS durante la partita**.

## Secondo passo preparato: mesh statiche reali, sempre senza GX

L'INI puo' selezionare **due** modalita' standalone nella build futura:

- `gxm_direct_probe=1`: il test originale di una mesh/una draw.
- `gxm_direct_probe=2`: carica `gxm_direct_static_scene.bin` e invia
  piu' draw per frame da geometrie reali statiche GLG gia' pronte in
  layout GPU. `gxm_direct_scene_draws=64` (range valido 1..256).

Il pacchetto e' stato generato **offline** dal PSARC locale, per mezzo di
`smstrikers-port/tools/build_direct_gxm_scene.py`. Il convertitore
verifica gli AVNR v2 per header, bounds, indici e XXH64; seleziona
esclusivamente la variante prepacked 20B per vertice, `position=f32x4`,
`color=unorm8x4`. Il risultato viene scritto in un formato bounded
`SMGXM001`, con FNV1A-64 e bounds per mesh. Il lettore Vita valida
integralmente il pacchetto e non ricorre a GX se e' mancante o malformato.
La descrizione della pipeline native GXM richiede esattamente quel layout
e `RasColor`, non usa texture e shader/conversioni GX per draw. La CPU
prepara una volta sola le matrici d'istanza, poi il loop usa `DrawPacket`
tipizzati persistenti con buffer GPU gia' caricati.

**Pacchetto preparato per prova e verificato anche sulla Vita:**
`ux0:data/strikersVita/gxm_direct_static_scene.bin`, 58.530 byte,
SHA-256 `1a1cdaf3d7078950d01ea507bee5439fb51dfab94512a90f30568f3da314ea94`.
Contiene 12 mesh (2.307 vertici, 5.991 indici) provenienti dal pack
`sms-native-gxm-static.psarc` di 4.822 geometrie statiche. Non e'
possibile attribuire queste mesh a una posizione precisa del campo
senza il collegamento a GLG/scene/materiale originali. Il pacchetto
privato rimane in `ab-artifacts/native-gxm-direct-20261013/`, non e'
destinato alla distribuzione insieme alla repository.

Sono cambiati/aggiunti:
`include/port/native_gxm_scene_pack.hpp`,
`include/port/native_gxm_direct_scene.hpp`,
`src/platform/native_gxm_direct_scene.cpp`, la modalita' `2` in
`native_gxm_direct_probe.cpp`/`Game/main.cpp`,
`tools/build_direct_gxm_scene.py`, test host e documentazione INI.

Verifiche: roundtrip del pack reale in un decoder C++ indipendente,
`build_tev_cg(scene_pipeline())` con `textureMask=0`/`colorMask=1`,
test ostili di bounds/checksum/indici e contract test host PASS;
VitaSDK cross-compiler `-fsyntax-only` di scene/probe/main PASS;
CMake configure-only gate ON, modulo incluso, con topology R3-A safe PASS;
Aurora host `ctest` 38/38 PASS; `git diff --check` PASS.

**Questa non e' ancora una partita.** Manca tutta la connessione di
gameplay `nlTaskManager/WorldManager -> NativeInstance` e la
precompilazione di matrici/palette/luci, texture/TEV, HUD, EFB e shadow.
Attivare `gxm_direct_probe=0` nell'INI avvierebbe la partita preesistente,
ma tramite **GX -> GXM**, e non andrebbe dichiarato una prova del nuovo
renderer completamente nativo.

La build attiva sulla Vita rimane la **precedente versione del primo
probe** (`eb0768...`) e l'INI e' lasciato a `gxm_direct_probe=1`.
La modalita' `2` e' presente solo nei sorgenti locali. Nessuna nuova
build, SELF, deploy o modifica INI dopo la prima prova: il file di
mesh e' stato solamente preparato su ux0. Per testarlo si richiede
una build manuale esplicita e una successiva installazione verificata.
