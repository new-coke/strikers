# Costo CPU e priorità di ottimizzazione, 2026-10-11

Il primo bersaglio è la preparazione dei draw sul percorso GLX → GX → GXM.
La geometria già residente sulla GPU non elimina la produzione dei comandi
GX, la loro interpretazione e la preparazione di stato/materiali per draw.
La regressione delle ricette native non giustifica aggiungere ulteriori
lookup e guard allo stesso percorso.

## Evidenza corrente e diagnostica precedente

La build attuale `1200ffa8…` ha prodotto 18,757 FPS e 53,313 ms medi per
fotogramma nella cattura quiet. Le mediane della fase tasks e di present
sono rispettivamente 50,348 e 0,328 ms. Tasks comprende anche attese:
questo dato da solo non misura CPU attiva o tempo GPU.

Il CSV diagnostico precedente `nu-d-gx-consumer.csv` contiene 120 campioni,
frame consumer 618–737. È stato ricalcolato per questa analisi:

| Scope | Mediana per frame | Interpretazione |
| --- | ---: | --- |
| Frontend draw GX | 26,798 ms | Scope inclusivo della preparazione dei draw |
| Traduzione dello stato | 6,994 ms | Pipeline/layout/matrici e stato shader |
| Tempo trascorso nell'invio GXM | 5,125 ms | Timer sul thread chiamante, con preparazione, driver e possibili attese; non è tempo GPU |
| Contatore vertex_decode | 2,915 ms | Nei percorsi fusi comprende anche trasformazione e impacchettamento, oltre alla decodifica |
| Cache geometrica | 1,932 ms | Lookup, validazione e preparazione |
| Risoluzione texture | 0,990 ms | Binding/risoluzione, non solo upload |
| Costruzione uniform | 0,727 ms | Preparazione dei parametri shader |
| Pubblicazione uniform | 0,917 ms | Snapshot e pubblicazione |
| Attesa del producer | 19,513 ms | Il thread del gioco attende uno slot libero nella coda del worker GX |
| Contatore consumer_wait | 1,060 ms | Il chiamante attende il completamento di un job; non misura inattività del worker |
| Display queue | 0,032 ms | Attesa della coda display registrata |

**Non sommare le righe:** gli scope sono inclusivi/annidati e i due thread
si sovrappongono. Questi tempi diagnostici sono precedenti alla build
attuale, quindi non sono una scomposizione esatta dei suoi 53,313 ms.
Indicano dove iniziare, senza attribuire arbitrariamente il tempo restante
ad animazione, fisica o decoder.

La verifica del sorgente distingue inoltre i due contatori FIFO:
`producer_wait_us` misura la coda piena in `enqueue_job`; il nome
`consumer_wait_us` è ambiguo perché viene incrementato da `wait_serial`
sul chiamante che attende il completamento. Non interpretare quest'ultimo
come inattività del consumer.

Nella diagnostica le mediane sono 272 draw, 64.722 vertici totali e 62.744
vertici sul percorso geometrico GPU per frame: rapporto circa 96,94%.
Gli upload texture hanno mediana zero. La provenienza offline dei buffer
non è stabilita da questo rapporto. Il costo continuo di stato e invio
rimane anche sui buffer residenti.

## Significato esatto dei timer e conversione nativa

`ScopedTelemetryPhase` usa `sceKernelGetProcessTimeWide()` all'ingresso e
all'uscita. È un tempo trascorso, non un conteggio dei cicli CPU attivi o
dei cicli GPU. Lo scope Submit avvolge `renderer_->execute(stream_)` o
`renderer_->draw(...)`: comprende lookup dei buffer/pipeline, gestione
della scena, binding di programmi/texture/uniform, viewport/scissor,
vertex stream e `sceGxmDraw`. L'esecuzione di una command stream può anche
incontrare barriere con `finish`. I 5,125 ms sono la mediana del totale
accumulato per frame diagnostico, non il costo di una singola `sceGxmDraw`.

Il renderer invia comandi alla GPU. Il timer non attende necessariamente
la conclusione del rendering GPU e può includere attese quando il driver
o una barriera bloccano il chiamante. Il basso tempo di present non basta
a misurare il tempo GPU. Non aggiungere finish per draw soltanto per
ottenere una misura: cambierebbe il parallelismo che si vuole valutare.

La diagnostica `nu-d-gx` ha `split_vertex_phases=0`. Nel percorso streamed
`prepare_streamed_draw_into` sceglie `decode_transform_pack_range` per il
fallback CPU oppure `decode_pack_range` per fixed-vertex GPU. Entrambi
sono temporizzati come VertexDecode. Nel primo caso il contatore include
decodifica, trasformazione e impacchettamento finale; non si possono
attribuire tutti i 2,915 ms al solo formato GX dei vertici.

| Dato | Preparazione nativa proposta | Lavoro dinamico che rimane |
| --- | --- | --- |
| Geometria statica e indici | Buffer con layout finale GXM, preparati offline/al caricamento | Selezione del buffer e invio |
| Stato materiale e shader | Materiale persistente, programmi e varianti pronti | Parametri variabili, risorse dinamiche e scelta del pass |
| Matrici statiche/bind pose | Float e layout richiesti dal percorso nativo, una volta | Nessuno finché non cambiano |
| Camera, trasformazioni e palette delle ossa | Calcolo diretto su ARM e snapshot nel layout shader | Calcolo della posa e pubblicazione dei valori correnti |
| Mesh animate | Attributi di base e selettori della palette preparati | Trasformazione shader dove il contratto lo consente; aggiornamento diretto dei buffer per deformazioni software |

Il gioco calcola già matrici su ARM. `glud_Skin` le passa però a
`GXLoadPosMtxImm`/`GXLoadNrmMtxImm`, che emettono comandi XF con i float;
il consumer li interpreta e prepara lo stato shader. Per i draw nativi
coperti si può eliminare questo passaggio scrivendo direttamente snapshot
dei parametri dell'istanza destinati a GXM, nello stesso ordine della coda.
Le matrici dipendenti dalla partita devono comunque essere aggiornate:
il loro valore non è precomputabile una volta per tutte.

La conversione AVNR v2 già evita decode/repack durante l'ammissione delle
geometrie statiche compatibili. Il converter `geometry_requests` esclude
esplicitamente i chunk SKIN e VERTEX_ANIM. Estenderlo a quei dati richiede
il contratto di skinning/animazione corretto, con selettori, palette,
attributi e lifetime preservati. I buffer del percorso geometrico GPU
possono anche essere costruiti a runtime e riusati: il rapporto 96,94%
non rappresenta la percentuale di tutti i modelli preconvertiti offline.

## Perché il percorso nativo sperimentale può costare di più

`glx_SendFrame_cb` continua a eseguire view, programma, texture, raster,
matrici e stream del gioco. L'ammissione della ricetta alla draw e la
validazione sul consumer introducono ulteriore lavoro. Sul miss si ritorna
alla GX display list nel suo slot originale. Il consumer deve ancora
risolvere texture, stato dinamico e uniform.

Il confronto quiet precedente ha misurato circa 49,35 ms con GX ordinario
contro 66,24 ms con ricette e builder incrementale. Preparare la geometria
o aumentare gli hit non equivale a rimuovere il lavoro precedente al draw.
La candidata early R3-A ha inoltre registrato zero ammissioni; il suo
contratto statico/unlit non copre la famiglia dominante Characters/Shadowed.

## Intervento CPU da sviluppare

1. Separare, sulla stessa build, lavoro del producer, attese FIFO,
   elaborazione del consumer e preparazione GXM. Nel producer distinguere
   pose/skinning, generazione dello stato e draw; nel consumer distinguere
   decoder e preparazione del draw. Usare scope esclusivi o dichiarare
   esplicitamente l'annidamento. Registrare per famiglia draw, hit e fallback.
   Riutilizzare percorsi temporanei e ripristinare l'INI originale.
2. Sviluppare un percorso per la famiglia effettivamente costosa,
   Characters/Shadowed, che riceva mesh/materiale persistenti e i dati
   correnti dell'istanza. Geometria, layout, shader e stato statico si
   preparano al caricamento; camera, palette delle ossa, matrici, luci e
   altri parametri dinamici mantengono aggiornamenti espliciti.
3. Per i casi coperti eliminare realmente l'emissione e la traduzione GX
   ripetute. La selezione del percorso deve precedere il lavoro che vuole
   evitare. Una cache consultata dopo quel lavoro non soddisfa l'obiettivo.
4. Verificare equivalenza e ordine dei draw, snapshot e lifetime delle
   risorse, cambi di materiale, EFB, scissor e fallback. La cache non deve
   introdurre draw aggiuntivi. Misurare il costo totale della guardia e dei
   miss, oltre agli hit.
5. Accettare il percorso solo con ammissioni effettive nelle famiglie
   coperte e misure quiet alternate di media, p95 e p99. Host e contatori
   di hit non costituiscono prova di maggiore FPS.

## Interventi specifici sul frontend DrawSink

`DrawFrontend` avvolge `DrawSink::submit`: è uno scope inclusivo della
preparazione del draw e può includere invii/flush effettuati al suo interno.
I 26,798 ms non sono un costo aggiuntivo rispetto ai contatori di stato,
geometria e vertici. Nel ramo delle ricette native il timer comincia dopo
una parte delle guardie di ammissione: il confronto dei soli timer frontend
non misura tutto il costo aggiunto dal percorso sperimentale.

Le revisioni dello stato, i buffer residenti, la risoluzione pipeline e i
binding texture riusati esistono già. Il lavoro utile deve ridurre i casi
che li invalidano o saltare la preparazione, invece di aggiungere una
seconda cache generica di tutto lo stato.

1. **Separare le ricostruzioni dai cambi di stato non pertinenti.** In
   `DrawSink::submit`, `rebuildVertex` e `rebuildFragment` includono entrambi
   `!translatedCacheHit`. Un cambiamento che impedisce il riuso della
   pipeline può quindi ricostruire anche quei dati. Il primo esperimento
   circoscritto deve evitare questo lavoro per transizioni provate di solo
   raster/blend, mantenendo le ricostruzioni per cambi reali delle dipendenze
   vertex, texgen, luci, palette, layout e parametri fragment. Non rimuovere
   semplicemente il termine: confrontare l'intero risultato con una
   traduzione fresca, incluse transizioni tra materiali e mesh.
2. **Usare risorse preparate con identità persistenti.** Il percorso
   nativo deve conservare riferimenti a geometria, pipeline/materiale e
   binding con generation/lifetime espliciti. I casi coperti devono evitare
   traduzione del layout, ricostruzione delle chiavi e validazione completa
   delle sorgenti immutabili a ogni draw. Pose e risorse dinamiche mantengono
   aggiornamenti e dipendenze espliciti.
3. **Preparare i parametri per istanza e vista.** Condividere snapshot
   immutabili fra le parti dello stesso personaggio quando hanno davvero
   gli stessi requisiti, senza confondere palette, luci o texgen di materiali
   diversi. Un precedente prototipo di riuso uniform cross-mesh non ha
   superato l'oracle di traduzione fresca ed è stato escluso: non riattivarlo.
4. **Ridurre i draw compatibili.** Valutare batching soltanto fra draw
   consecutivi con stato e parametri equivalenti e geometria compatibile,
   preservando l'ordine e i confini EFB/pass/barriera. Characters con palette
   differenti non sono automaticamente un gruppo unico. La cache geometrica
   non deve aggiungere draw. Il flag di batching esistente da solo non è una
   prova di guadagno.

Il primo bypass strutturale deve coprire una classe reale di Characters
con `3d_pointlit`, texture, normali/UV e palette corrente, poi il pass
Shadowed con il proprio contratto. Il precedente censimento osservava circa
85% delle callback nelle due viste: è una misura di frequenza, non una
percentuale di tempo CPU. Il percorso statico/unlit early, con zero
ammissioni misurate, non costituisce copertura utile di quella classe.

Per una mesh coperta il producer deve scegliere il percorso prima degli
aggiornamenti GLX/GX evitabili. Il consumer riceve risorse preparate e
parametri correnti, con coerenza di stato preservata anche quando la draw
successiva usa il fallback GX. Ogni patch resta un esperimento indipendente:
misurare frontend completo, attese della coda, draw/hit/fallback e FPS quiet
con media e code, senza attribuire al solo contatore inclusivo il guadagno.

Non esiste ancora una misura sufficiente per promettere 60 FPS. Il budget
totale è 16,667 ms; dalla candidata corrente serve eliminare circa il
68,74% del tempo per fotogramma. Il lavoro deve mirare a togliere fasi
complete di preparazione, mantenendo separato il costo del gioco.

## Sorgenti e prove

- [Ricalcolo del budget diagnostico](evidence/CPU_COST_2026-10-11/budget.json).
- [Cattura e installazione della build attuale](GX_DECODER_HARDWARE_2026-10-11.md).
- [Confronto precedente GX/ricette e significato dei contatori](NATIVE_MODEL_UNIFORMS_2026-10-09.md).
- [R3-A e ammissioni nulle del percorso early](AURORA_R3A_EARLY_PRODUCER_2026-10-10.md).
- Producer: `smstrikers-port/src/NL/glx/glxSend.cpp`, `glx_SendFrame_cb` e `native_model_draw_only`.
- Consumer: `smstrikers-port/extern/aurora-vita/platforms/vita/gx/aurora_vita_draw_sink.cpp`, `DrawSink::submit` e `submit_native_model_recipe`.
- Attese: `smstrikers-port/extern/aurora-vita/lib/gx/fifo.cpp`, `take_wait_stats`.

Questa analisi ha riletto sorgenti e CSV già disponibili. Non ha modificato
il renderer, compilato nuove build o cambiato lo stato della Vita.
