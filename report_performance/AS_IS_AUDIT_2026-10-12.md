# Strikers Vita — audit dell'as-is e percorso verso 60 FPS

Audit iniziato l'11 ottobre 2026; report completato il 12 ottobre 2026.
Ambito: port nativo GXM, sorgenti correnti, configurazione effettiva,
identità dell'eseguibile sulla Vita e risultati sperimentali già conservati.
Nessuna modifica al renderer, nuova compilazione o installazione durante l'audit.

## 1. Esito e decisione tecnica

Il lavoro prioritario è **ridurre la preparazione ripetuta di ogni draw lungo
GLX → GX → GXM**. La geometria residente e il rendering GXM esistono già;
non eliminano automaticamente callback del gioco, emissione dei comandi GX,
interpretazione dello stato, risoluzione delle risorse e preparazione degli
uniform. Il bypass completo delle famiglie Characters/Shadowed non è stato
implementato.

Una regressione importante è documentata: con lo stesso SELF, il percorso
ordinario GX/GXM misurava circa **49,35 ms/frame**, mentre ricette native,
cache e builder misuravano **66,24 ms/frame**. È circa **34,23% di tempo in
più**, da 20,27 a 15,10 FPS. I controlli e le cache del percorso sperimentale
non avevano rimosso abbastanza lavoro precedente. Questo risultato giustifica
il loro default OFF; non dimostra che ogni rappresentazione nativa sia più lenta.

**I 60 FPS restano un obiettivo, non una capacità dimostrata del port attuale.**
Anche rispetto ai migliori controlli storici qui esaminati, 24,52–25,33 FPS,
servirebbe eliminare circa il 58–59% del tempo medio per frame. Non basta
accumulare miglioramenti dell'1–3%, rimuovere codice morto o aggiungere worker.
Occorre dimostrare una riduzione sostanziale del percorso critico, preservando
simulazione, qualità del rendering e comportamento del gioco.

Limite dell'audit: **non è disponibile una nuova cattura FPS completa del
SELF corrente**. La partita automatica è stata confermata dall'utente, ma il
CSV atteso non è stato recuperato. Le cifre seguenti sono storiche e hanno
identità esplicita; nessuna è presentata come FPS misurato della build corrente.

## 2. Stato verificato

| Oggetto | Identità verificata |
| --- | --- |
| Repository, branch `main` | `a175e0a15ce0215d94075c9142162fa5be9d3f82` |
| Aurora incorporata, `vita-experiment` | `be08d453b852df6b25e2a0a841cf7a431d13201b` |
| SELF locale e `ux0:/app/SMSVITA01/eboot.bin` | `25963510ff28f4ab8eaaf79e2550f21dc6c85e160229a5ddc0e8d93c9b879e54` |
| INI sulla Vita, 2.033 byte | `300651a644c7e4e3f0400d54a6a8c09e0a151776258af24a3c0ecdd090588db4` |
| Directory di build | `smstrikers-port/build-vita-gxm-latest` |

Il checkout era pulito prima dell'audit. Sono stati aggiunti soltanto questo
report, il piano operativo, piccoli file di evidenza e uno script locale di
analisi delle catture. SELF e INI sono stati
riletti anche alla ripresa del 12 ottobre. L'INI originale è ancora identico;
non è stata attivata la configurazione diagnostica preparata durante l'audit.

La cattura avviata prevedeva 600 frame di gioco esclusi e 300 campioni, con
`fixed_dt=0`, stesso seed e nessun cambio delle opzioni grafiche. Il vecchio
`r3a_quiet_gx.csv` è stato archiviato sul computer e rimosso dal percorso remoto
prima del lancio, per impedire che un file precedente fosse scambiato per un
nuovo risultato. Alla ripresa non era presente un nuovo CSV. Il vecchio file
rimane **non attribuito** e non entra nei risultati.

Evidenze: [identità del lancio](evidence/AS_IS_AUDIT_2026-10-11/run-1-identity.json),
[rilettura del dispositivo](evidence/AS_IS_AUDIT_2026-10-11/resume-device-identity.json),
[INI esatto](evidence/AS_IS_AUDIT_2026-10-11/strikers.ini),
[stato e opzioni della build](evidence/AS_IS_AUDIT_2026-10-11/STATUS.json).

### Configurazione effettiva rilevante

| Voce | Stato | Conseguenza |
| --- | --- | --- |
| Renderer | GXM, GX asincrono ON | Un worker interpreta GX e possiede il renderer |
| LTO | ON | Build ottimizzata; nessun profiler di compilazione attivo |
| Direct draw submit | ON | Evita una parte della mediazione della command stream |
| Direct stream write / immediate draw view / distinct cores | OFF | Non attivare in blocco come parte di una pulizia |
| Geometria statica | 8 MiB; native assets e native GPU static ON | Riusa geometria compatibile; il frontend del draw rimane |
| Native model, replay, early, prepared | OFF | La regressione nota delle ricette non è intenzionalmente attiva |
| Native PN palette / raster uniform reuse | OFF | Esperimenti non promossi per risultati insufficienti |
| Fragment prepare cache / fixed uniform pool / geometry preflight | ON | Esistono già ottimizzazioni mirate; non riproporle come nuove |
| `gxm_disable` | `0x8` | Disabilita il riuso degli snapshot fixed-vertex; non disabilita tutto il renderer |
| Diagnostica / overlay FPS | OFF | Esclusi profili e log ordinari |
| Menu debug Vita | ON | Il registro shader mantiene contatori per draw anche a menu chiuso |
| Shader profile | INI `WARM` | Prevale sul default CMake `SEALED`; compilazione runtime non sigillata dal profilo |
| Risoluzione | INI `auto`, GC native resolution OFF | Il sorgente seleziona il profilo Vita; non è stata misurata qui una riduzione di risoluzione |
| CPU / GPU richieste | 500 / 222 MHz | Le catture precedenti riportavano CPU effettiva 444; richiesta e frequenza effettiva vanno distinte |
| Simulazione | `fixed_dt=0` | La progressione segue il tempo reale; non equivale ai test a passo fisso |

`task_profile=1` e `task_profile_buffered=1` sono presenti nell'INI, ma
`configure_diagnostics()` rimuove i relativi selettori dall'ambiente quando
`diagnostics=0`. Non costituiscono prova di logging attivo. Il vecchio recorder
`benchmark` è sotto `#if 0`: i relativi valori rimasti nell'INI non provano
l'esecuzione di quel recorder.

## 3. Cosa dimostrano i confronti precedenti

| Esperimento | Risultato hardware conservato | Conclusione utilizzabile |
| --- | --- | --- |
| Controllo del 6 ottobre, SELF `4836a409…`, passo fisso | 24,52 e 25,33 FPS, 1.200 frame per run | Riferimento storico; non baseline equivalente al SELF attuale a tempo reale |
| Ricette native e builder, SELF `1d167223…` | GX 20,27 FPS; native 15,10 FPS, due run per modalità | Regressione importante osservata; esperimento da non promuovere |
| Early R3-A static/unlit | Zero ammissioni nella prova documentata | Il percorso non copre il carico Characters/Shadowed interessato |
| Snapshot ridotti del decoder, `44052765…` → `1200ffa8…` | 18,09 → 18,76 FPS; p95 e p99 peggiorano | Segnale positivo sulla media in una coppia; beneficio stabile non dimostrato |
| Raster uniform reuse, SELF `dbc5a1a…` | 18,90 → 18,27 FPS | Una coppia senza beneficio; OFF mantenuto |
| Palette PN native, SELF `ebbc7f2b…` | Tre coppie: +6,60%, +0,50%, −0,16%; aggregato +2,24% | Trasporto nativo attivo dimostrato, guadagno non consistente; OFF mantenuto |
| Pulizia as-is, SELF `a9e5b3bc…`, poi default INI `25963510…` | Oracoli host e installazione verificati | Correttezza locale verificata; confronto FPS hardware mancante |

Non è corretto affermare che tutte le modifiche peggiorino: esistono segnali
positivi e risultati neutri. È invece corretto dire che **non è ancora
dimostrato un miglioramento complessivo ripetibile verso 60 FPS**.

Non usare la differenza 25 → 19 FPS tra epoche diverse per attribuire una
regressione a una singola patch: cambiano SELF, configurazione, durata delle
catture e passo di simulazione. Anche con seed identico, un test a tempo reale
può produrre pose e situazioni diverse. Servono confronti alternati e una
scena riproducibile; il passo fisso serve a confrontare il costo, non a
nascondere un gioco che avanza più lentamente del tempo reale.

Fonti numeriche:
[review storica](evidence/PERFORMANCE_REVIEW_2026-10-10/review.json),
[decoder](GX_DECODER_HARDWARE_2026-10-11.md),
[raster reuse](evidence/FRONTEND_RASTER_REUSE_2026-10-11/STATUS.json),
[palette PN](evidence/NATIVE_PN_PALETTE_2026-10-11/STATUS.json),
[pulizia](evidence/AS_IS_CLEANUP_2026-10-11/STATUS.json).

## 4. Percorso che viene ancora pagato

1. Il gioco prepara packet, trasformazioni, pose e dati della vista.
2. `glx_SendFrame_cb` applica cambi di programma, materiale, texture, raster,
   user data, matrici e stream; `glx_DrawPacket` emette il draw GX.
3. Il producer serializza stato e display list nella coda GX. Quando la coda
   è piena attende il consumer.
4. Il worker interpreta GX e chiama `DrawSink::submit`.
5. Il sink risolve layout, stato pipeline, texture, geometria, uniform e
   prepara il comando GXM. Molto di questo lavoro esiste anche su cache hit.
6. Il renderer risolve gli handle, prepara/binda programmi e parametri,
   invia la draw e gestisce scene, EFB e presentazione.

La Vita esegue già il codice del gioco e i calcoli delle matrici su ARM.
"Rendere nativo" deve quindi significare **eliminare un formato intermedio
o una fase ripetuta**, non soltanto rinominare o duplicare gli stessi dati.
Una matrice animata deve ancora essere calcolata; si può evitare di
serializzarla come comandi XF per poi ricostruirla nel consumer.

La cache statica già può caricare buffer GXM pronti dagli asset AVNR.
Un hit geometrico evita parte del decode/repack, ma non elimina il lavoro
di materiale, pipeline, uniform e invio. La percentuale di vertici sul
percorso GPU non è la percentuale di modelli interamente preconvertiti offline.

## 5. Rilievi sul sorgente attuale

### A — Diagnostica per vista non affidabile con il worker asincrono

**Priorità: alta per la validità dell'audit.** In
[`glPlat.cpp`](../smstrikers-port/src/NL/gl/glPlat.cpp),
`PortDebugRenderViewTimer` (righe 271–341) legge `telemetry().frame()` e
`fifo_profile_accumulator()` prima/dopo una vista. Sono strutture aggiornate
dal worker GX quando i contatori del backend sono abilitati, non snapshot
pubblicati per quella vista. Con GX asincrono e quei contatori attivi
le letture possono sovrapporsi alle scritture e non delimitano il lavoro
consumer della vista del producer.

Questo non riguarda la configurazione quiet attuale, che disabilita il
profilo. Impedisce però di usare quei delta come prova del costo di
Characters o Shadowed. `performance_snapshot()` usa invece `run_sync`:
evita la lettura concorrente, ma introduce un punto di sincronizzazione e
non è un osservatore gratuito.

Correzione necessaria: timer del producer su dati di sua proprietà e
contatori consumer pubblicati a frame/vista completati, con identificatori
espliciti. `completed_performance_snapshot()` e
[`vita_performance_capture.hpp`](../smstrikers-port/include/port/vita_performance_capture.hpp)
sono la base già disponibile per l'attribuzione per frame. Non inserire
attese sincrone per ogni vista o draw solo per misurare.

### B — Il fallimento della cattura FPS può essere silenzioso

**Priorità: alta per rendere verificabile la baseline.** In
[`benchmark.c`](../smstrikers-port/src/platform/benchmark.c),
`finish_capture()` (riga 30), `s_captureDone` viene impostato prima di
`fopen`. Se l'apertura fallisce, i campioni vengono liberati senza rendere
distinguibile il fallimento da una cattura conclusa. Non sono verificati
nemmeno gli esiti finali di scrittura e chiusura.

È un difetto verificato nel sorgente; **non è dimostrato che sia la causa
del CSV assente in questa sessione**. Mancano stato live della cattura e
codice di errore. Non attribuire arbitrariamente l'assenza a un freeze,
all'INI o al filesystem.

Correzione circoscritta: stato idle/collecting/complete/error e conteggio
campioni consultabili senza scrivere ogni frame; verifica degli errori;
conservazione del buffer su errore; un tentativo esplicito successivo,
senza retry continuo sullo storage. Testare anche errori di apertura,
scrittura e chiusura. La cattura non deve rallentare il gioco durante il
campionamento né produrre una nuova cartella o file a ogni tentativo.

### C — Debug shader ancora presente nel percorso ordinario

**Priorità: bassa come guadagno atteso, alta come chiarezza del profilo.**
`vita_debug_menu=1` attiva `aurora_vita_shader_debug_capture()` in
[`main.cpp`](../smstrikers-port/src/Game/main.cpp), riga 1290.
[`overlay_vita.cpp`](../smstrikers-port/src/platform/overlay_vita.cpp),
riga 423, non lega quel flag a `diagnostics`.

[`gxm_renderer.cpp`](../smstrikers-port/extern/aurora-vita/platforms/vita/gxm/gxm_renderer.cpp),
righe 1663–1673, consulta il registro shader per ogni draw di gioco.
[`vita_shader_debug_registry.hpp`](../smstrikers-port/extern/aurora-vita/platforms/vita/gfx/vita_shader_debug_registry.hpp),
`skip_draw()`, incrementa un contatore atomico anche quando lo shader è
abilitato e la draw viene eseguita normalmente. Dopo la registrazione non
prende un mutex per ogni draw: non sovrastimare il problema.

Esperimento semplice: stesso SELF, modificare solo `vita_debug_menu` e
misurare OFF/ON alternati. Nessuna stima di millisecondi recuperati è
giustificata prima del test. Non è una spiegazione sufficiente del salto
da circa 20 a 60 FPS.

### D — Traduzione e ricostruzioni rimangono nel percorso caldo

**Priorità: alta per il lavoro strutturale.**
[`aurora_vita_draw_sink.cpp`](../smstrikers-port/extern/aurora-vita/platforms/vita/gx/aurora_vita_draw_sink.cpp):
`submit()` riga 560; miss di traduzione 596–660; ricostruzioni vertex e
fragment 810–839; cache geometrica 875–890.

Un miss della pipeline può forzare ricostruzioni vertex/fragment oltre
alle rispettive revisioni. La cache principale mantiene lo stato tradotto
corrente; esiste anche memoizzazione della traduzione, quindi non ogni
miss equivale a ricostruire tutto da zero. Le transizioni tra materiali
restano comunque lavoro per draw. Il raster reuse già sperimentato non ha
mostrato il beneficio richiesto: non riattivarlo come soluzione acquisita.

Serve un contratto persistente di mesh/materiale con dipendenze esplicite
per vista, istanza, palette, luci, texgen e risorse. Un hash o una generazione
insufficiente non giustificano il riuso. Il precedente prototipo uniform
cross-mesh ha fallito l'oracolo di traduzione fresca: resta escluso.

### E — Il bypass anticipato non copre la famiglia dominante

**Priorità: massima tra le ottimizzazioni da sviluppare.**
[`glxSend.cpp`](../smstrikers-port/src/NL/glx/glxSend.cpp), righe 3296–3378,
contiene un bypass early limitato a static/unlit, matrice identità, vista
`UnsortedPerspective`, senza userdata e con stato pulito. Il normale
skinning e le famiglie Characters/Shadowed non rientrano in quel contratto.

Il lavoro utile è scegliere un percorso preparato **prima** degli switch
GLX/GX evitabili, per una classe effettivamente usata, inizialmente
Characters con `3d_pointlit`. Il producer pubblica risorse persistenti e
dati dinamici dell'istanza; l'unico owner GX/GXM conserva ordine e lifetime.
Il fallback deve mantenere lo stato coerente quando si alternano draw
native e GX.

I criteri di successo sono tempo totale eliminato, copertura reale e
fallback economico. Numero di cache hit o build riuscita non bastano.
Shadowed richiede un contratto separato, non una deduzione automatica dal
pass Characters. Il censimento storico dell'85% delle callback nelle due
viste misura frequenza, non quota di tempo CPU.

### F — Geometria residente non significa validazione gratuita

**Priorità: subordinata a misure di hit/miss e costo.**
[`vita_static_geometry.hpp`](../smstrikers-port/extern/aurora-vita/platforms/vita/gfx/vita_static_geometry.hpp),
`get()` riga 132, costruisce una chiave e verifica layout, indici, revisioni
e sorgenti. Sul percorso stabile un epoch invariato evita parte della
validazione; non è corretto sostenere che ricontrolli sempre tutti i byte.
I miss possono preparare/snapshotare risorse, ma esiste già un preflight
del budget.

Ottimizzare soltanto il lavoro rimasto: handle persistenti con ownership
e generation provate possono evitare lookup o validazioni ripetute.
Non eliminare i controlli senza sostituirli con un contratto di lifetime.
Ogni cache deve preservare numero e ordine delle draw: nessuna draw extra
per mantenere o scaldare la cache durante la misura.

### G — Una parte delle regressioni nasce da esperimenti non isolati

Il freeze della precedente build Lean non era un A/B del solo Lean:
erano cambiati anche switch di scheduler/submission. Il report
[`VITA_GXM_LEAN_RENDER_2026-10-10.md`](VITA_GXM_LEAN_RENDER_2026-10-10.md)
documenta questa contaminazione. Non prova che togliere i rami morti causi
un freeze, né che Lean sia una soluzione pronta.

Gli script attuali fissano le opzioni rilevanti e riusano la stessa
directory. Il protocollo futuro deve confrontare anche la cache CMake e
l'INI effettivo, non soltanto il nome della build. Le istruzioni storiche
che suggeriscono una directory nuova per candidata sono superate da
`AGENTS.md`.

## 6. Budget CPU: cosa sappiamo e cosa manca

La diagnostica storica `nu-d-gx` ha 120 campioni consumer, frame 618–737.
Le mediane seguenti sono **tempi trascorsi**, con scope annidati e thread
sovrapposti, non una somma di CPU attiva del SELF corrente.

| Scope storico | Mediana | Interpretazione |
| --- | ---: | --- |
| Frontend draw GX | 26,798 ms | Preparazione inclusiva del draw, compresi eventuali invii interni |
| Traduzione stato | 6,994 ms | Sottoinsieme del lavoro frontend |
| Invio GXM | 5,125 ms | Tempo sul chiamante: preparazione, driver ed eventuali attese |
| Vertex decode | 2,915 ms | Può comprendere decode + trasformazione + packing fusi |
| Cache geometrica | 1,932 ms | Lookup/validazione/preparazione |
| Risoluzione texture | 0,990 ms | Risoluzione e binding, non solo letture/upload |
| Uniform build / publish | 0,727 / 0,917 ms | Preparazione e pubblicazione dei parametri |
| Attesa producer | 19,513 ms | Coda piena; non lavoro CPU utile aggiuntivo |
| `consumer_wait` | 1,060 ms | Il chiamante aspetta il completamento di un job |
| Display queue | 0,032 ms | Questo punto specifico di attesa |

Non sommare frontend, sottofasi e attese. Ridurre un costo del consumer
può ridurre anche l'attesa producer: contarli entrambi come risparmio
sarebbe un doppio conteggio. Allargare la coda può spostare l'attesa e
aumentare la latenza senza migliorare il throughput.

La stessa diagnostica vede mediane di 272 draw, 64.722 vertici, 62.744 sul
percorso geometrico GPU e zero upload texture. Sono dati utili per
orientare l'audit, non una prova che la GPU sia gratuita o che tutto sia
preconvertito offline. Fonte: [budget e limiti](evidence/CPU_COST_2026-10-11/budget.json).

Mancano ancora sulla build corrente: CPU attiva per thread distinta dalle
attese, correlazione affidabile producer/consumer per famiglia, costo GPU
e dipendenze di scena, variazione ripetibile del workload. Un basso tempo
di presentazione non esclude limiti GPU. I timer di `sceGxmDraw` non misurano
quanto impiega la GPU a finire il frame. Inserire `finish` per draw
serializzerebbe il lavoro e altererebbe il fenomeno misurato.

## 7. I/O: importante per startup e picchi, non spiegazione provata di tutto

La cattura archive I/O sul SELF `1cd15f8…` osservava, nei 299 intervalli
tra live frame 600 e 899:

- 66 letture fisiche, circa 1,95 MB complessivi;
- 1,538 ms medi/intervallo nelle API di lettura; massimo 37,006 ms;
- 0,420 ms medi/intervallo di inflate; massimo 11,018 ms;
- 54 intervalli con letture e 245 senza; nessun errore registrato.

I frame senza letture avevano ancora circa 43,86 ms medi. I picchi I/O
sono reali, ma quei dati non mostrano lettura continua satura come unica
causa del costo in partita. La misura copre PSARC gioco/native, non ogni
accesso a shader, salvataggi o altri file. I tempi su thread diversi
possono sovrapporsi: non sottrarre meccanicamente 1,538 + 0,420 ms dal frame.

Lo startup aveva un costo I/O molto maggiore. L'ultimo frame osservato
includeva inoltre la scrittura della cattura, con 509,5 ms totali: un
picco di output non va attribuito alla GPU. Il dataset originale conserva
sia il risultato grezzo sia l'analisi esplicitamente senza quel campione.
Fonte: [stato e catture I/O](evidence/ASSET_IO_2026-10-11/STATUS.json).

Politica raccomandata: niente log per draw/frame nella misura FPS; accumulo
limitato in RAM e un output finale verificato; un solo percorso remoto
riutilizzato. Prima di aggiungere una cache PSARC, misurare quali blocchi
si rileggono davvero e il rapporto hit/memoria. Precaricare tutto può
consumare memoria necessaria al renderer e introdurre nuovi miss/eviction.

## 8. Quanto manca ai 60 FPS

| Riferimento storico | Tempo medio | Riduzione necessaria per 16,667 ms | Accelerazione richiesta |
| --- | ---: | ---: | ---: |
| Migliore controllo citato, 25,33 FPS | 39,478 ms | 57,78% | 2,37× |
| Altro controllo, 24,52 FPS | 40,779 ms | 59,13% | 2,45× |
| Decoder candidato, 18,76 FPS | 53,313 ms | 68,74% | 3,20× |
| Ricette native, 15,10 FPS | 66,236 ms | 74,84% | 3,97× |

Questi calcoli quantificano il divario, non predicono la performance del
nuovo SELF. Un'ottimizzazione che riguardi pochi millisecondi non può da
sola eliminare 23–50 ms. Per dimostrare che 60 FPS sono raggiungibili serve
misurare il residuo dopo il bypass, insieme a simulazione e GPU, entro lo
stesso frame budget.

Ridurre draw conta quando elimina effettivamente lavoro. Il batching è
ammissibile solo tra draw consecutivi compatibili, senza cambiare ordine,
palette, blending, profondità, EFB, scissor o pass. Un framebuffer esiste
già nel renderer: aggiungerne un altro non elimina preparazione GLX/GX e
può aggiungere copie o banda. Anche abbassare la risoluzione è un test di
sensibilità alla GPU, non una cura dimostrata della CPU.

## 9. Ordine del lavoro successivo

1. **Rendere osservabile e affidabile la cattura attuale.** Risolvere il
   punto B; pubblicare dati coerenti del punto A. Nessun refactor ampio.
2. **Congelare un controllo ripetibile.** SELF, INI, clock effettivi,
   workload, shader cache e frame window; separare FPS quiet e attribuzione.
3. **Misurare il piccolo residuo diagnostico.** Un A/B del solo menu debug;
   mantenere il risultato separato dal lavoro strutturale.
4. **Preparare una famiglia Characters reale.** Mesh/materiale persistenti,
   palette e dati dinamici espliciti; selezione prima degli switch GLX/GX.
   Uno scope coperto bene vale più di un percorso generico con zero hit.
5. **Estendere a Shadowed soltanto dopo i gate del primo percorso.**
6. **Poi valutare batching, validazioni geometriche e I/O mirato**, in base
   al nuovo costo residuo. Non attivare simultaneamente i vecchi esperimenti.

Gate comuni: equivalenza host dei comandi/dati; nessuna draw aggiuntiva;
ordine e lifetime corretti; fallback misto; verifica visiva su Vita anche
in gameplay avanzato; audio/input/stabilità; almeno tre coppie alternate
con media, p95 e p99. Una riduzione deve superare la variabilità del
controllo e non peggiorare sistematicamente le code. Nessuna percentuale
di guadagno promossa da una sola build o da un contatore di hit.

Il dettaglio implementabile è in
[piano operativo e criteri di accettazione](AS_IS_AUDIT_TICKETS_2026-10-12.md).

## 10. Consegna e limiti

- Report e verifiche del sorgente completati; misura hardware nuova incompleta.
- Nessuna nuova ottimizzazione attivata e nessun guadagno FPS dichiarato.
- INI e SELF originali verificati alla ripresa; nessuna nuova installazione.
- Nessuna directory di build, copia SELF/VPK/ELF o pacchetto aggiuntivo.
- La pulizia precedente resta valida come manutenzione con oracoli host;
  non le viene attribuito un miglioramento FPS non misurato.
- I dati storici sono sufficienti per respingere le ricette regressive e
  prioritizzare un bypass reale; non bastano per promettere 60 FPS.
