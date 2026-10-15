# Strikers Vita — renderer, particelle e prestazioni, 15 ottobre 2026

La build corrente usa VitaGL, frontend asincrono adattato da RE4 e mixer MusyX nativo. Restano le precedenti conferme del proprietario su audio, modelli e profondità delle scintille. Il 15 ottobre il proprietario ha riaperto la correttezza di porte e ombre e fornito un riferimento dell’originale, identificato come Il Palazzo / The Palace. La precedente accettazione dei pali sulla SELF 7af3 non dimostra la fedeltà a quel riferimento. Il proprietario ha poi confermato porte e ombre corrette sulla SELF 0913. La build corrente mantiene quelle correzioni e la simulazione separata dal rendering, e corregge la successiva segnalazione del nero dopo il caricamento.

## Identità corrente

- SELF corrente: `971aadfbc977fb751f228595966f6df4b2a054d881ec38ac8a09c0fc45d55592`.
- VPK: `f7dc040fe58adaf77262f80d8c41c5baf81b14a7f706f1991c71ec9902c20212`; eboot interno identico alla SELF.
- Provider VitaGL: `e107159de830f005120ce2bdc40d0f0be567d087946d13f0d700bbdefd12e544`.
- Directory Vita unica e riutilizzata: `smstrikers-port/build-vita-gxm-latest`.
- Un solo set locale `.previous`: SELF 091304ce, con porte/ombre accettate e simulazione in tempo reale verificata, ELF/VELF/map/SELF/VPK nello stesso build. Nessun nuovo set binario nelle evidenze.
- Flag richieste, effettive su tutte le 30 unità VitaGL: `READBACKS_SPEEDHACK=1 HAVE_SHADER_CACHE=1 NO_DEBUG=1 DRAW_SPEEDHACK=1`.
- Flag di correttezza aggiunta: `STORE_DEPTH_STENCIL=1`. `NO_DEBUG` implica anche `SKIP_ERROR_HANDLING`, come upstream. Rimangono i controlli applicativi di ownership, shader e framebuffer.
- Aurora, backend applicativo GXM diretto, GX thread, profiler, autostart e fast profiler: OFF. VitaGL e thread audio: ON.
- Audit ELF: zero simboli grafici Aurora/SDL/GX, zero chiamate a `glFinish`/`glFlush`. Le API pubbliche della libreria rimangono definite; il frontend non le invoca. GXM resta interno a VitaGL.
- INI normale: `3c013d70b7eb2f8a33c805c3343bbe4534782bf7dd93a81e7aea3f9fe05926e2`, diagnostica/profiling/cattura spenti, `fixed_dt=0`. La partita automatica resta una scelta INI (`vita_test_match=1`), mai forzata nel codice.
- Clock rilevati nelle prove: CPU 444, GPU 222, bus 222, xbar 166 MHz. La richiesta CPU 500 ricade a 444 sul dispositivo.

Identità di compilazione e installazione corrente: `evidence/VITAGL_2026-10-15/loading-handoff/`. La precedente validazione visiva, della simulazione e delle prestazioni normali della SELF 0913 resta in `goal-shadow/`. Le identità delle prove precedenti restano storiche e non sono sovrascritte.

## Correzioni grafiche e prova

1. Il culling front è espresso con winding inverso e culling back, combinazione supportata dal provider VitaGL. Ha ripristinato campo, personaggi, texture e HUD. Il successivo confronto dello stesso fotogramma 1170 con culling normale/disabilitato non ha mostrato una correzione del pallone disabilitando il culling. La configurazione finale mantiene il culling normale.
2. `DrawableBall` inizializza orientamento corrente/precedente; il motion blur ripristina orientamento e validità della matrice. Anche il delta della rotazione fisica viene inizializzato. Le nuove prove non mostrano i precedenti NaN del pallone.
3. La rampa luce dei personaggi usa `lighting/range=2` impostato dal gioco. Il ponte aveva omesso il fattore. Ora viene applicato alla fase diffuse, prima di riflessi e autoilluminazione.
4. La rampa luce BumpLocal deve usare clamp su U e V indipendentemente dai bit del materiale, come in `glx_SwitchTextureState`. I 47 pacchetti Mario/Toad e relative mesh blend hanno tutti il valore memorizzato 0 (WrapWrap), come il default del gioco; il ponte lo applicava erroneamente anche alla rampa. Ora conserva il contratto originale. Ai limiti 0/1, repeat mescolava le estremità opposte della texture. Il test verifica i parametri GL per tutti i quattro valori di wrap e gli stati delle texture ordinarie. Contratto sorgente in `ramp-clamp-source-contract.json`.
5. Quando cambia la mappa dei registri delle ossa, il registro sorgenti cambia generazione e il ponte ritira i VBO precedenti nella coda del renderer. Mappe identiche conservano la cache; mappe rifiutate conservano dati e generazione validi. Il test copre tutti questi casi. Questo difetto di invalidazione è corretto, ma non è stato isolato come causa specifica delle immagini dell’utente.
6. Le sequenze NIS dichiarano fino a tre palloni. Il port gestiva solo ID 0 e un limite 1; ora conserva due oggetti aggiuntivi nel solo snapshot di presentazione, azzerati tra scene, e accetta ID0/1/2 entro i limiti del file. Il replay serializzato non cambia. I log hardware confermano tre pose distinte. Alcune pose sono intenzionalmente sotto il campo o fuori camera; la loro assenza in quei fotogrammi non dimostra uno scarto GPU.

La precedente ipotesi di animazione NIS ferma dopo il primo frame è stata scartata: `mPrevState` registra l’ultima transizione, non il frame precedente, e i log mostrano tempi/pose in avanzamento. Il predicato condiviso NIS/replay corregge un caso di transizione, ma non è la spiegazione del pallone invisibile.

La SELF 2478 ha completato quattro prove grafiche, con zero errori GPU e nessun nuovo crash. `mario-toad-visual` scambia temporaneamente squadre/comprimari tramite INI per mostrare Mario e Toad: le grandi facce grigie dei cappelli non compaiono più. Nel fotogramma 1170 il pallone è visibile davanti a Mario, con pannelli chiari e scuri. Il proprietario ha accettato la resa dei modelli. Il provider `DRAW_SPEEDHACK` usa correttamente gli indici riflessi degli attributi: è rimasto attivo.

## Scintille della barriera e profondità

La traccia opt-in `vita_particle_trace=1` registra template, flag infront e stato di profondità. Le scintille della barriera (`fx_electricity_sparks`, `fx_electricity_ball_anim`, `fx_electricity_spark_flare`) hanno template_front0/spec_front0/depth_test1. Non sono forzate in primo piano dagli script.

VitaGL inizializzava le superfici senza `STORE_DEPTH_STENCIL`. Le copie intermedie cambiano framebuffer/scene; la conservazione esplicita della profondità è ora abilitata nel provider. Le quattro flag richieste rimangono attive. Non sono aggiunte sincronizzazioni globali.

Il confronto al fotogramma 180 tra SELF 0d0e (storage temporaneo ottimizzato, profondità non forzata) ed eb4f (stesso codice applicativo, conservazione della profondità) mostra:

- 248 record delle particelle identici fino al fotogramma controllato, inclusi quantità e stati di profondità;
- differenze nello 0,453% dei pixel, limitate alla barriera e alle sue occlusioni, rettangolo 238,272–575,395; campo e cielo identici;
- nessun errore GPU e ripristino dell’INI dopo entrambe le catture.

Il primo piano al fotogramma 1170 resta regolare con la conservazione della profondità: pallone e modelli visibili, differenze nello 0,210% dei pixel rispetto alla versione2478, prevalentemente dettagli occlusi. Le catture sono in `particle-depth-control`, `particle-depth-candidate` e `particle-depth-frame1170`. Queste prove sono visive e non costituiscono misure FPS. Il proprietario ha poi confermato «Scintille ora corrette», accettando la correzione nella propria scena.

## CPU e misure senza diagnostica

Il ponte alloca ora intervalli e matrici delle ossa in array temporanei limitati, invece di creare vettori a ogni pacchetto. `Range` è un POD: prima, il prologo ARM di `consume_writes` azzerava 32.768 byte a ogni chiamata, prima di controllare se ci fossero scritture. Ora copia/legge solo il prefisso valido e non esegue quell’azzeramento. Evidenza ARM prima/dopo in `packet-scratch/arm-invalidation-before-after.txt`. Lo snapshot 1170 della SELF 0d0e è identico pixel per pixel alla SELF 2478.

Tutte le misure seguenti usano 300 frame di partita live 600–899, stesso seed 0x53545249, `fixed_dt=16.666666667`, CPU 444/GPU 222/bus 222/xbar 166 MHz, diagnostica 0, timing 0, overlay 0, snapshot 0. Questo fixed timestep identifica la partita di prova; la configurazione normale usa 0. Nessun FPS è dedotto da compilazione, affinità o carico per core.

| Versione | Prove | FPS | Mediana frame | p95 |
|---|---:|---:|---:|---:|
| Modelli corretti 2478 | 3 | 25,111 / 24,992 / 25,228 | 39,454 / 39,519 / 39,237 ms | 43,342 / 44,880 / 43,212 ms |
| Storage CPU ottimizzato 0d0e | 1 | 28,862 | 34,235 ms | 41,546 ms |
| Profondità corretta eb4f, precedente | 3 | 28,529 / 28,669 / 28,457 | 34,720 / 34,668 / 34,803 ms | 40,686 / 39,350 / 40,757 ms |
| Materiali con UV per significato 7af3, precedente | 1 | 28,905 | 34,521 ms | 39,122 ms |

La media dei tre controlli è 25,110 FPS. La versione eb4f raggiunge **28,552 FPS medi**, intervallo 28,457–28,669: **+13,70%** rispetto al controllo, con una riduzione del tempo medio per frame del 12,05%. La differenza supera la variabilità fra le tre prove di ciascuna versione. Il campione CPU 0d0e senza conservazione esplicita della profondità rimane un confronto intermedio singolo, non la build consegnata.

Sui 900 frame complessivi della versione eb4f: media 35,025 ms, mediana 34,708 ms, p95 40,576 ms, p99 59,723 ms. Il 23,22% dei frame rientra nel budget di 30 FPS; nessuno rientra nel budget di 60 FPS. **30 FPS stabili e 60 FPS non sono raggiunti.** Il vecchio singolo campione cb3 a 25,119 FPS è storico, separato da questi controlli ripetuti.

Tutte e tre le prove eb4f hanno completato la finestra richiesta, ripristinato l’INI normale e chiuso soltanto Strikers con risposta `Killed.`. Nessun nuovo coredump e nessun errore GPU. Analisi, hash dei CSV e parametri condivisi sono in `particle-depth/quiet-summary.json`; ogni prova conserva la propria identità e il CSV completo in `particle-depth-quiet-1`, `-2`, `-3`.

## Frontend RE4 e audio

Riferimento fornito: https://github.com/Rinnegatamante/re4, commit 24c03d65b2a5896ef6d56dab1d8e9110ae19e0ae, letto tramite accesso GitHub autenticato. `platform/renderer/gx_command_buffer.h` è copiato invariato in `extern/re4-renderer`, con licenza CC0 e hash e639b754e40c1400b54ed955d66b6d3d560647568d050bf502d988a81c3f12d5. Il consumatore adatta la coda limitata e la pubblicazione ogni 16 draw; i comandi contengono dati tipizzati Strikers e il consumatore chiama VitaGL.

| Thread | CPU | Compito |
|---|---|---|
| Principale | 2 | Gioco, preparazione e pubblicazione dei comandi |
| Renderer | 1 | Unico proprietario del contesto VitaGL |
| Helper/audio | 0 | Lavori CPU e mixer/AudioOut |

Otto slot da 2 MiB possiedono uniform, matrici e geometria dinamica fino al consumo. Dati presi a prestito e chiamate con risultato sono sincronizzati con il consumatore. Geometrie oltre lo slot seguono il percorso sincrono. Mutex/condizioni pthread hanno TLS VitaSDK valido. L’attesa della coda è CPU e non invoca glFinish/glFlush.

Il mixer software era escluso da `audio_mix.cpp` quando Aurora era spento; ora è incluso nel runtime nativo. AudioOut usa due blocchi alternati, allineati 64 byte, adattati da RE4; ricampionamento stereo 32→48 kHz con pesi con segno. Apertura/volume/output sono controllati; gli errori arrestano il produttore. I log mostrano campioni non nulli, zero underrun/errori e il proprietario ha confermato audio regolare. Le latenze durante caricamenti comprendono lock/I/O e non sono puro costo CPU del mixer.

## Validazione e recupero

ASan/UBSan: driver VitaGL, coda differita, trasporto audio, worker, registro sorgenti, geometria e test del predicato NIS. Il driver verifica upload posseduti, stato GL, scissor, uniform e pulizia dopo errori. La coda esercita 500 draw con riuso immediato dei dati, pressione sugli slot, geometria oltre 2 MiB, errori e reinizializzazione. Il trasporto audio verifica allineamento, conservazione del buffer precedente e arresto senza spin. Restano valide le verifiche precedenti di 763 texture/3650 mip; i test host non provano i pixel GPU. L’analizzatore dei frame ha 5 test passati.

Nelle finestre controllate resta un singolo avviso iniziale `skin_selectors`, senza errori GPU. Nel successivo controllo esteso della configurazione normale sono comparsi scarti `packet_shape` per primitive 5 (line strip) nella vista 28: questo limite residuo è registrato in `goal-net/normal-before-native_rejected_packets.log` ed è distinto dal materiale dei pali. Non viene classificato come risolto dalla correzione dei materiali.

I quattro vecchi crash (splash, worker senza TLS pthread, pixel-store non supportato, buffer temporaneo non allineato) sono conservati con le rispettive evidenze. Tutti furono chiusi manualmente prima del recupero. Nessun nuovo coredump in queste prove. Il provider usa allocazioni circolari allineate 16 byte.

Il primo avvio della build a tre palloni non aveva prodotto il log iniziale nonostante la risposta Launched di Companion. Il controllore venne terminato lasciando il test INI; dopo «gioco sta girando» sono stati recuperati i log e la chiusura mirata ha risposto Killed. Il nuovo avvio diagnostico controlla il log univoco e conferma una sola volta Start se manca; le prove senza diagnostica usano una conferma esplicita prima del warm-up.

Il fallback remoto storico resta eboot.bin.vitagl-previous, hash ccfe94c1521d8e5667a74d8471d0304bcbb4b992e5ff8c7338d20149f2505517. Quella consegna conservava eb4f nel set locale `.previous`; il fallback corrente è indicato nella sezione Identità corrente. Asset, salvataggi e sorgenti preesistenti sono preservati. Le sezioni storiche precedenti descrivono lo stato prima della consegna Git richiesta il 15 ottobre.

## Consegna eb4f verificata prima dei materiali

Dopo le tre misure, la SELF eb4f è stata riavviata con l’INI normale. Il file di stato precedente è stato conservato e rimosso prima dell’avvio, quindi sono stati osservati nuovi frame 60 e 120, con successivo avanzamento fino a 1560 e zero errori GPU. Il gioco era stato lasciato in esecuzione prima delle nuove prove sui materiali. Nessun nuovo coredump.

La lettura indipendente dell’eseguibile installato conferma SHA256 `eb4f47bdedc146836aa8726262b941e6303d0ccc5db015ba5a36ae90f0e6ea79`. L’INI è ripristinato byte per byte con SHA256 `3c013d70b7eb2f8a33c805c3343bbe4534782bf7dd93a81e7aea3f9fe05926e2`: diagnostica, overlay, timing, catture, snapshot e profiling spenti; `fixed_dt=0`. La partita automatica resta abilitata esclusivamente dalla prima impostazione INI `vita_test_match=1`.

All’avvio, il gestore dei default riscriveva soltanto un commento e righe vuote del blocco gestito. La prima verifica byte per byte ha rilevato questa differenza; il confronto di tutte le righe di impostazione ha confermato valori e ordine identici. Dopo l’avvio è stato ripristinato il file originale e verificato nuovamente, senza cambiare le impostazioni già caricate. Entrambe le letture sono conservate in `particle-depth/normal-launch-first-readback.json` e `particle-depth/final-device-readback.json`.


## Pali delle porte e coordinate dei materiali — 15 ottobre

Segnalazione del proprietario: «i pali sono opachi e sembrano fuori contesto». Le mesh `peach_toad_stadium/end_frame` e `peach_toad_stadium/belltower_goal` hanno texture Diffuse + Gloss, con stream 3 e 7, mentre gli stream intermedi sono assenti. Il ponte decodificava correttamente gli stream per significato, poi li ricompattava in base al numero delle texture attive. La mappa Gloss finiva così sulle coordinate del Detail assente, `(0,0)`, perdendo la variazione del materiale. La ricompattazione è rimossa: dati dei vertici e attributi dello shader mantengono lo stesso significato.

Anche il calcolo speculare conservava solo la prima luce, non normalizzava la direzione ricevuta e usava una potenza al posto della curva razionale originale. Ora conserva fino a quattro luci, normalizza le direzioni, applica il fattore d’intensità originale 1,25 e usa la curva di `GXInitSpecularDir`/`glx_LoadSpecular`, espressa nello shader VitaGL. Per i materiali illuminati con gloss, il riflesso è aggiunto al colore di base con il peso originale `GlossLevel/63`; i due materiali citati usano 12/63 e 13/63. Il primo tentativo aveva erroneamente attenuato anche il colore di base: la cattura ha rilevato tetti scuri ed è stato corretto prima della versione corrente. Nessuna dipendenza GX o Aurora viene introdotta nel rendering.

Test ASan/UBSan passati: geometria con stream Diffuse + Gloss senza intermedi; upload di tutte e quattro le luci e del peso materiale; invalidazione della cache uniform quando cambia la quarta luce; coda differita e proprietà degli upload. L’audit dell’eseguibile corrente conferma zero chiamate `glFinish`/`glFlush` e zero simboli grafici Aurora/SDL/GX. Le quattro flag richieste e `STORE_DEPTH_STENCIL=1` rimangono attive.

Al fotogramma controllato 360, la versione 7af3 cambia il 10,620% dei pixel rispetto a eb4f; il confronto con la versione intermedia senza la correzione UV isola un cambiamento del 10,126%. Nelle regioni dei pali cambiano il 61,79–66,25% dei pixel; campo e cielo nelle regioni di controllo sono identici. Nel primo piano 1170 il pallone è identico pixel per pixel alla versione eb4f; Mario e Toad restano integri nelle catture. Queste immagini documentano i cambiamenti e il controllo delle regressioni geometriche. Il proprietario ha poi confermato «Ora sono corretti», accettando l’aspetto dei pali sulla SELF 7af3.

Evidenze: `goal-net/source-contract.json`, `semantic-uv-frame360-comparison.json`, `semantic-uv-frame1170-comparison.json`, catture e identità nelle rispettive directory. La prova di 300 frame live 600–899 sulla SELF 7af3 ha completato la cattura: 28,905 FPS, media 34,597 ms, mediana 34,521 ms, p95 39,122 ms, p99 50,854 ms. CPU 444/GPU 222/bus 222/xbar 166 MHz, diagnostica e timing spenti. È un solo campione: pur usando lo stesso seed, timestep e finestra live, la progressione osservata della partita differisce da eb4f. Non dimostra un ulteriore guadagno FPS né un confronto ripetuto a scena identica. I 30 FPS stabili non sono raggiunti. INI ripristinato e chiusura mirata verificata dopo la cattura; nessun nuovo crash o errore GPU.


## Consegna precedente 7af3 verificata

Dopo le catture visive e la misura breve, la SELF 7af3 è stata riavviata con la configurazione normale: nuovi frame 60 e 120, zero errori GPU e nessun nuovo coredump. Il gioco è lasciato avviato. Eseguibile installato e INI sono stati letti e verificati separatamente: SELF `7af37271783528d3c294d8c3a2c3935567e7892afa9c21a64e1bf124c283fd2f`, INI `3c013d70b7eb2f8a33c805c3343bbe4534782bf7dd93a81e7aea3f9fe05926e2`. L’eventuale normalizzazione iniziale dei commenti è verificata come priva di modifiche alle impostazioni, poi l’INI originale è ripristinato byte per byte. Diagnostica, timing, profiling e catture spenti; `fixed_dt=0`; partita automatica scelta soltanto dall’INI. Evidenza: `goal-net/final-device-readback.json`.


## Porte, ombre e simulazione in tempo reale — 15 ottobre

Il riferimento `goal-shadow/original-longplay-reference.png` è il Palazzo del gioco originale, confermato dal proprietario. Le due cause dei materiali sono distinte: il programma 0x31 limita il lookup della luce prima di moltiplicarlo per la texture diffuse; il programma 0x39 usa la terza texture per modulare il riflesso, non per aggiungerla direttamente al colore. I materiali 0x21 mantengono invece la loro scala sul risultato diffuse, come nel sorgente originale. Il programma pointlit dirt conserva la propria sequenza di stadi.

Le ombre statiche del terreno con materiali 0x07/0x27 usano `GLTS_ShadowLevel/63` e il suo complemento. Il ponte passava invece i due colori globali delle shadow volume, entrambi bianchi RGB: `saturate(shadow_texture + 1)` cancellava qualsiasi ombra della porta. I modelli originali `The_Palace/pitch_shadow_01` e `Mario_Stadium/netshadow_01/_02` sono geometrie texturizzate, non richiedono l’attivazione delle shadow volume.

Le sagome dei personaggi non sono più sostituite di default con blob tondi. Il formato originale della copia è A8: la copertura è nel canale alpha del framebuffer, mentre l’RGB della sagoma è nero. Sono corretti il canale copiato, l’orientamento delle tre righe e le sei righe di padding di ogni cella. Le dieci celle aggiornate sono ora regioni di un solo atlante 320×240; le celle non aggiornate restano in cache. La copia richiede un cambio di destinazione e il ripristino, evitando dieci framebuffer indipendenti. Gli indici per i confini delle ombre planari sono letti come u16 nativi, con limiti e controlli dei dati posizione, anziché come vecchia FIFO GX.

La fisica originale è a 50 Hz (`g_fFixedUpdateTick=0.02`), non un aggiornamento per immagine. Tutti i task nativi ricevono ora lo stesso intervallo monotono a 64 bit per passaggio. I tick della fisica consumano quel tempo: a 20 immagini/s vengono eseguiti due o tre passi da 20 ms. Il recupero è limitato a 16 passi per passaggio e conserva il debito; l’interpolazione resta fra pose già completate. Stop di oltre due secondi sono trattati come discontinuità di caricamento/sospensione. Restano i rallentamenti intenzionali del gioco e gli stati di pausa. È rimosso il pianificatore adattivo che chiamava `glDiscardFrame` per recuperare prestazioni; le soppressioni originali di immagini durante caricamento/transizioni di risorse restano separate da quel meccanismo.

`fixed_dt=0` è il default anche dello strumento di validazione. Un valore non nullo rimane una richiesta esplicita di cattura deterministica: rallenta il gioco quando vengono prodotte meno immagini del periodo scelto. Le vecchie misure intorno a 28–29 FPS usavano 16,67 ms simulati per immagine; non sono un controllo diretto del nuovo gameplay a velocità reale. Il limite di rendering attende soltanto il tempo ancora disponibile e non aggiunge una pausa quando un frame è già oltre il budget.

I test del pianificatore eseguono il vero `nlTaskManager`: 500 passi in 10 secondi con 20 immagini/s, 10 immagini/s, delta identico per task a costi diversi, dilatazione, recupero limitato, sospensione e passaggio alla cattura deterministica. Passano inoltre il test del limite di rendering, il driver VitaGL con tre righe A8 e dieci regioni sullo stesso framebuffer, il frontend RE4 con copie immutabili delle nuove regioni e i confini/vertici dei modelli, con AddressSanitizer e UndefinedBehaviorSanitizer. Questi controlli host non sono prova di pixel o FPS sulla Vita.

La SELF intermedia 6c01 ha registrato 66,888735 s reali, 66,927113 s forniti ai task e 66,820000 s simulati su 32 finestre, con 0,108833 s pendenti alla fine: 99,84% del tempo di simulazione atteso. Era precedente alla correzione dell’attesa del limite a 20 FPS; il rendering diagnostico di quella prova era 7,684 FPS, quindi non dimostra 20 FPS grafici. Le prove finali 0913 e la consegna normale sono registrate separatamente sotto `goal-shadow/`.


### Verifica finale 0913 prima della segnalazione sul caricamento

Il proprietario ha confermato «Sì, ora sono corretti» per porte e ombre del Palazzo, confrontando la cattura `validated-realtime20/debug_frame_play_1.png` con il longplay originale. Il nome della cattura indica la soglia minima richiesta; lo scatto è avvenuto al frame live 65, con pallone x=21,114 vicino alla porta. Identità e accettazione sono in `owner-visual-acceptance.json`.

La prova diagnostica finale usa esclusivamente nell’INI temporaneo un tetto a 20 FPS. Il rendering misurato è 15,708 FPS. Nelle 26 finestre attive, i task ricevono 52,912547 secondi e la simulazione esegue 52,900000 secondi: 99,9763% del tempo atteso, con 12,161 ms residui. È esclusa soltanto la finestra del readback diagnostico iniziale, che interrompe il gioco per scrivere lo screenshot ed è conservata nel CSV. Tutte le altre finestre, inclusi i normali rallentamenti, partecipano al calcolo. `validated-realtime20/simulation-summary.json` documenta selezione e valori. Questa è prova della progressione temporale, non di 20 o 60 FPS grafici.

La prova normale senza il limite a 20 FPS e con `fixed_dt=0` registra 240 immagini live (120–359): **21,3915 FPS**, media 46,747 ms, mediana 43,904 ms, p95 63,187 ms, p99 110,201 ms. Diagnostica, overlay, timing e profiling spenti; CPU 444/GPU 222/bus 222/xbar 166 MHz. Il target resta **60 FPS** e non è raggiunto. Il carico a velocità reale differisce dalle vecchie catture a tempo fisso, quindi il confronto con 28–29 FPS non permette di attribuire una regressione o un guadagno. Evidenza: `normal-realtime-quiet/run-identity.json` e CSV completo.

Dopo le prove, la SELF 0913 è stata avviata e osservata sui nuovi frame 60 e 120, con zero errori GPU e nessun nuovo dump. SELF e INI sono verificati indipendentemente; l’INI originale è ripristinato byte per byte (SHA256 3c013d70b7eb2f8a33c805c3343bbe4534782bf7dd93a81e7aea3f9fe05926e2), senza limite di prova a 20 FPS. Il gioco è stato lasciato avviato prima della nuova segnalazione sul nero alla fine del caricamento. Evidenza: `goal-shadow/final-device-readback.json`.


## Schermo vuoto alla fine del caricamento — 15 ottobre

Segnalazione del proprietario: dopo il caricamento compare qualche istante di nero prima di entrare in partita. Il controllo sulla Vita (SELF 40b1, diagnostica soltanto di osservazione rispetto alla 0913) registra 470 passaggi e tre `glplatAbortFrame`: tutti e tre venivano comunque presentati dal ciclo esterno VitaGL, con zero draw. I due iniziali sostituivano lo schermo di caricamento con il framebuffer appena azzerato; fino al primo contenuto trascorrevano circa 3,7 secondi. Un altro avveniva nell’intro, con circa 2,9 secondi fino al contenuto successivo. Sono intervalli di presentazione osservati nel trace applicativo, non una misura esterna della latenza del display.

`glplatAbortFrame` notifica ora l’invalidità al runtime. La chiusura della coda differita attende il consumo dei comandi ma omette blit e swap soltanto per quel passaggio invalido; il display conserva l’ultima immagine valida. Le immagini normali continuano a essere presentate. Il pianificatore adattivo di frameskip del gameplay resta rimosso. All’avvio diretto di una partita viene mostrata tramite VitaGL la bitmap originale e localizzata di caricamento del gioco, così esiste un’immagine da mantenere anche prima del primo menu o della prima scena. Nessun nuovo testo di interfaccia o accesso GXM applicativo.

Il clock comune viene inoltre ribasato dopo i callback sincroni di cambio stato: un caricamento caldo di 750 ms non accelera l’animazione d’uscita nel passaggio seguente. I normali rallentamenti del rendering continuano a contribuire per intero al tempo della simulazione, verificato dal test con un’interruzione di 400 ms. Il target grafico rimane 60 FPS; non viene introdotto alcun limite a 20 FPS.

Sulla SELF finale 971a, il trace completo registra 496 passaggi, tre annullamenti e **zero passaggi annullati presentati**, con **zero immagini presentate prive di draw**. La cattura `loading-handoff/validated/debug_frame_3d_1.png`, acquisita dopo il primo passaggio invalido, mostra la scritta originale arancione LOADING ancora sul display; è stata ispezionata. La partita prosegue e completa la finestra di 30 frame live. Nessun errore GPU e nessun nuovo coredump. Il readback diagnostico interrompe intenzionalmente la prova: questo run non misura le prestazioni normali. Trace, hash e limiti sono in `loading-handoff/transition-summary.json`.

Passano con ASan/UBSan: 100 annullamenti consecutivi sul driver VitaGL senza blit/swap e successiva ripresa; passaggio dell’annullamento sul thread proprietario attraverso la coda RE4 dopo tutti i comandi precedenti; pianificatore reale con caricamento caldo escluso e normali hitches conservati. Le verifiche di risorse, uniform, geometria, scissor e gestione errori dei due driver restano attive negli stessi test. Python di validazione verificato. L’audit finale conferma tutte le 30 unità del provider con le flag richieste e `STORE_DEPTH_STENCIL=1`, nessuna chiamata applicativa a glFinish/glFlush, nessun simbolo grafico Aurora/SDL/GX e identità fra SELF e eboot del VPK. Evidenza: `loading-handoff/host-validation.json` e `build-identity.json`.


La misura normale della SELF 971a usa `fixed_dt=0`, nessun tetto di prova a 20 FPS, diagnostica/overlay/timing/profiler/snapshot spenti e 240 frame live 120–359: **19,140 FPS**, media 52,246 ms, mediana 50,3455 ms, p95 69,999 ms, p99 106,252 ms. CPU 444/GPU 222/bus 222/xbar 166 MHz. È un singolo campione con progressione della partita non allineata pixel per pixel alla 0913: non permette una misura causale della differenza rispetto ai precedenti 21,392 FPS. Il target di 60 FPS resta aperto; questa consegna non prova un guadagno di prestazioni. CSV completo e identità in `loading-handoff/quiet/`. La finestra è completata, l’INI normale è ripristinato e la chiusura mirata ha risposto Killed, senza nuovi crash o errori GPU.


Sul successivo avvio normale della SELF 971a il proprietario ha confermato: «LOADING resta visibile, nessun intervallo nero». Il difetto di transizione è quindi accettato anche sulla visualizzazione reale, oltre ai trace e alla cattura. Evidenza: `loading-handoff/owner-visual-acceptance.json`.


Consegna finale 971a verificata con lettura indipendente della SELF installata e dell’INI originale, ripristinato byte per byte dopo l’eventuale normalizzazione dei soli commenti. Nuovi frame 60 e 120, zero errori GPU, nessun nuovo dump. Gioco lasciato in esecuzione con diagnostica, catture, overlay, timing e profiler spenti, `fixed_dt=0`, nessun limite di prova a 20 FPS. Evidenza: `loading-handoff/final-device-readback.json`. Il solo set locale `.previous` conserva la precedente SELF 0913 e i suoi ELF/VELF/map/VPK verificati; nessuna copia di prodotti di compilazione è stata aggiunta alle evidenze.


## Consegna Git, default INI e confronto con RE4 — 15 ottobre

Il codice validato è pubblicato su `origin/main` nel commit `22c311218bdecf30f9fb355a1cea792820d450fc`, verificato con lettura indipendente del riferimento remoto. I 46 test host registrati sono passati. Sono inclusi sorgenti, test, strumenti, provider VitaGL con licenze, documentazione e piccole evidenze JSON/CSV/PNG. Prodotti di compilazione, dump e backup PPM/log rimangono locali. Tre modifiche preesistenti nel sottomodulo Aurora sono preservate localmente; il suo riferimento non cambia e la build VitaGL non le usa.

Il generatore del file `ux0:data/strikersVita/strikers.ini` è già aggiornato al profilo VitaGL in `src/platform/config.c`, tabella `s_vita_defaults`. Il profilo nominale conserva il nome `2026-10-14-vitagl`: le correzioni del 15 ottobre non richiedono nuove impostazioni di rendering. Da zero genera `fixed_dt=0`, `vita_frameskip=0`, audio/video nativi attivi, diagnostica/overlay/profiling/timing/benchmark/catture spenti. Non genera un limite a 20 FPS. `vita_test_match=0` è il default; l’avvio automatico sulla Vita di prova è un override esplicito, preservato prima del blocco gestito. Il test `vitagl_default_config` verifica generazione, migrazione del vecchio blocco GXM, caricamenti ripetuti e conservazione degli override. Le quattro flag VitaGL richieste sono definizioni della libreria in CMake, non impostazioni INI. CPU richiesta 500 MHz, fallback effettivo 444 MHz sul dispositivo; GPU 222 MHz.

### Identità del riferimento e limiti

È stato riletto RE4 al commit corrente `1fd8358a4a4d0588427d556de235b5fbbe910f59`, 15 ottobre 08:56 UTC, messaggio “Prepare shared skin poses and palettes on workers”. Il repository è privato: l’endpoint web pubblico restituisce 404 e la lettura avviene tramite GitHub API autenticata. Hash e nomi dei 25 file primari consultati sono in `loading-handoff/re4-current-reference-identity.json`; i loro sorgenti completi non sono ricopiati nelle evidenze. Il command buffer importato da Strikers resta quello del commit 24c03d65, identificato separatamente in `extern/re4-renderer/SOURCE_IDENTITY.json`.

La misura normale più recente di Strikers è 19,140 FPS, 52,246 ms medi, su 240 frame live con `fixed_dt=0`. Il budget a 60 FPS è 16,667 ms: occorre ridurre il percorso critico medio di circa 35,580 ms, cioè del 68,10% rispetto a quel campione. È un fattore 3,135 nella cadenza. Non è una previsione del beneficio di una singola modifica. Il tempo mediano nei task è 49,495 ms, mentre la presentazione esterna misura 0,701 ms: i task includono anche preparazione, upload e attese della coda. Questi dati non separano costo CPU puro, driver e GPU e non vanno sommati ai timer sovrapposti.

### Cosa è già implementato e cosa manca

| Area | Strikers pubblicato | RE4 corrente e differenza trasferibile |
|---|---|---|
| Proprietario VitaGL | Gioco/frontend su CPU2, renderer su CPU1, un worker su CPU0; mixer e AudioOut separati, anch’essi su CPU0. Contesto GL posseduto soltanto dal renderer. | Stessa separazione principale; RE4 può usare un secondo helper sul core riservato. Quel core non è un quarto core libero garantito. Va misurato il beneficio al netto di audio, sistema e sincronizzazione. |
| Coda di comandi | Otto slot da 2 MiB, dati immutabili, pubblicazione ogni 16 draw; renderer e produttore lavorano in parallelo entro il frame. | Struttura base già importata. Anche RE4 esegue lo swap con `sync` (`gx_backend.h::swap`): non è un riferimento per rimuovere indiscriminatamente tutte le barriere di fine frame. |
| Preparazione nei worker | Pose pre-fisica, snapshot, interpolazione, pose delle skin e frustum culling usano `parallel_for`, attivo di default. Ogni chiamata attende il completamento prima di tornare. AI e fisica conservano le dipendenze seriali. | `gx_prepare_queue.h`, `gx_native_prepare.inc` e `gx_skin_jobs.inc` hanno job con prerequisiti, completion e consumo differito. La modalità native prepare 3 e il consumer differito sono attivi nel bootstrap. Manca un equivalente completo per preparazione grafica Strikers; il vantaggio potenziale è far arrivare l’attesa al primo consumatore invece che alla fine di ogni piccolo lotto. |
| Pose e palette | Skinning dei vertici già sulla GPU, pose CPU parallelizzate; la palette viene ricostruita/copiata nello stato dei singoli pacchetti. | L’ultimo commit RE4 condivide snapshot di pose e palette fra job dello stesso batch, con identità e seriale. La cache persistente opzionale è una funzione distinta. Il trasferimento utile è il riuso di preparazioni compatibili; lo skinning CPU di RE4 non costituisce automaticamente un miglioramento sul nostro skinning GPU. |
| Geometria e copie | Cache di VBO per sorgenti immutabili e invalidazione sulle scritture. I dati dinamici sono preparati in vettori, copiati nello slot e poi caricati con `glBufferData(GL_STREAM_DRAW)`. Il provider copia ancora il contenuto nel pool GPU. Creare modelli/texture richiede una chiamata sincrona al renderer. | RE4 conserva topologia e offset di raccolta e usa memoria posseduta/scratch ritirato dalla GPU, collegata con `vglBufferData`. Mancano piani di preparazione riutilizzabili per i dati dinamici e un percorso con meno copie e allocazioni per draw. Il loro lifetime deve includere l’ultimo utilizzo GPU, oltre al solo consumo CPU del comando. |
| Formato dei vertici | Un `ModelVertex` da 108 byte serve anche i casi che non usano sei UV, normali o ossa. Lo shader vertex è comune; i fragment hanno già 41 programmi preparati all’avvio. | RE4 offre layout packed da 24 byte per mesh ammesse, piccoli vertici GPU e uniform vertex compattate. Occorrono formati distinti per HUD, unlit, lit/skinned e multi-texture, con shader coerenti. I 24 byte di RE4 non coprono automaticamente tutti i materiali Strikers. |
| HUD e particelle | Il backend emette un draw per pacchetto; non aggiunge un sistema di batch/instancing nativo attraverso pacchetti compatibili. Alcuni produttori originali possono già raggruppare la propria geometria. | RE4 abilita CPU quad batch, native sprite/mesh batch e ha un percorso di sprite con istanze GPU. Manca l’equivalente esplicito sul nostro backend. Raggruppare soltanto elementi compatibili e consecutivi preservando ordine, alpha, scissor e confini di vista/copia. |
| Materiali e stato | Cache di programmi, uniform confrontate prima dell’upload, texture/sampler, raster, viewport e scissor. Il frontend ricostruisce ancora il descrittore completo per pacchetto; `SceneMaterial` è 1184 byte. | RE4 ha anche chiavi native di materiale, cache dei programmi per quel contratto e layout uniform selettivi. Possibile riuso di descrittori preparati per revisione del materiale, mantenendo dinamici luci, matrici, alpha, UV animati e invalidazione. |
| Shader e caricamento | Cache disco VitaGL attiva; shader di scena preparati all’inizializzazione, senza nuove compilazioni per ogni draw. Texture e modelli diventano residenti al primo uso; non tutti hanno un record offline disponibile. | RE4 ha anche un pacchetto offline di shader GXP, con validazione e cattura dei miss. Trasferibile per avvio/stutter e distribuzione; non è da solo una spiegazione dei 52 ms nei frame normali già caldi. Precaricare i primi modelli/texture durante il caricamento evita altro lavoro sulla prima scena. |
| Flag e pool | Attive le quattro flag richieste e `STORE_DEPTH_STENCIL=1`, necessario alla correttezza. Allineamento e limiti dei pool già corretti e testati. | Il Makefile RE4 documenta anche `CIRCULAR_POOL_SPEEDHACK=1`, assente qui. La nostra istantanea ha due implementazioni differenti del pool per quella flag: deve passare prima i controlli di lifetime, wrap e riuso GPU, più A/B sulla Vita. Non c’è un risultato che dimostri quanto possa guadagnare Strikers. |

Riferimenti primari fissati: [bootstrap RE4](https://github.com/Rinnegatamante/re4/blob/1fd8358a4a4d0588427d556de235b5fbbe910f59/platform/vita/game_entry.cpp), [coda e swap](https://github.com/Rinnegatamante/re4/blob/1fd8358a4a4d0588427d556de235b5fbbe910f59/platform/vita/gx_backend.h), [preparazione differita](https://github.com/Rinnegatamante/re4/blob/1fd8358a4a4d0588427d556de235b5fbbe910f59/platform/vita/gx_native_prepare.inc), [pose e palette](https://github.com/Rinnegatamante/re4/blob/1fd8358a4a4d0588427d556de235b5fbbe910f59/platform/vita/gx_skin_jobs.inc), [mesh packed](https://github.com/Rinnegatamante/re4/blob/1fd8358a4a4d0588427d556de235b5fbbe910f59/platform/renderer/gx_mesh_packed.h), [batch sprite](https://github.com/Rinnegatamante/re4/blob/1fd8358a4a4d0588427d556de235b5fbbe910f59/platform/vita/native_sprite_packets.inc), [flag del provider](https://github.com/Rinnegatamante/re4/blob/1fd8358a4a4d0588427d556de235b5fbbe910f59/Makefile).

### Priorità per la prossima implementazione

1. Attribuire una stessa finestra live calda con tempo della logica, preparazione dei pacchetti, job e relative attese, attese della coda, bytes copiati, upload, copie framebuffer e indicatori GPU. Una prova diagnostica attribuisce; una prova separata senza diagnostica misura gli FPS. Lo swap rapido non dimostra da solo che la GPU sia scarica.
2. Ridurre lavoro per pacchetto: formati di vertice e shader specifici, piani di topologia/offset conservati e meno passaggi di copia per dati dinamici. Riutilizzare le parti invarianti di materiale e palette. Misurare anche i casi che finiscono su upload sincroni.
3. Raggruppare HUD e sprite/particelle compatibili, mantenendo l’ordine originale delle trasparenze e la correttezza delle ombre. Contare i draw effettivamente rimossi.
4. Spostare preparazioni indipendenti su job con input posseduti, completion esplicita e attese al primo utilizzo. Il renderer rimane il solo proprietario VitaGL; AI, fisica e scritture globali non diventano parallele senza un contratto di dipendenze.
5. Dopo l’attribuzione, valutare pool/circular speedhack, posizionamento della geometria immutabile e costo di ombre/copie sul budget memoria e GPU. Le flag non sostituiscono la riduzione del lavoro. Shader offline e prewarm affrontano soprattutto avvio e stutter.

Questa è una valutazione del codice e delle misure esistenti. Non è stata applicata una nuova ottimizzazione al renderer durante l’audit e non è stato raggiunto il target di 60 FPS. Nessun nuovo glFinish/glFlush è introdotto: RE4 conserva una propria chiamata glFlush nel percorso `re4_graphics_flush`, che non è stata importata. Le prossime modifiche devono mantenere modello/pallone, scintille, porte e ombre accettati, audio regolare e velocità di simulazione reale. Nessun risultato di RE4 è assunto come prova di FPS su Strikers.
