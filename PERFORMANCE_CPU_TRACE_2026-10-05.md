# Analisi CPU Strikers Vita del 5 ottobre 2026

Questo documento registra la prima fase CPU. L'integrazione successiva, i test
hardware e il GPU crash della build più recente sono descritti in
[PERFORMANCE_NATIVE_WORKFLOW_2026-10-05.md](PERFORMANCE_NATIVE_WORKFLOW_2026-10-05.md)
e [GPU_CRASH_ANALYSIS_2026-10-05.md](GPU_CRASH_ANALYSIS_2026-10-05.md). Le frasi
su deploy/commit assenti più sotto descrivono lo stato alla fine di questa fase.

Il trace individua costi concreti nelle copie di memoria, nella preparazione dei draw, nelle pose e nel mixer audio. Gli interventi nuovi riducono il lavoro ripetuto in questi percorsi. Il maggior costo di copia rilevato, dentro `GXCallDisplayList`, è già affrontato dal commit Aurora presente nel checkout. Questa cattura non dimostra un raddoppio degli FPS e non consente di misurare il costo degli shader sulla GPU.

## Identità della cattura e del sorgente

| Campo | Valore |
| --- | --- |
| Database originale, letto in sola lettura | `/Users/robin994/Downloads/trace (3).db` |
| SHA256 database | `b35e53d379630cc41487d556c81f4d8b4b38310fdff4c2ce5bbabae866259000` |
| Titolo | PlayStation(R) Vita Snapshot 5 |
| Data UTC dichiarata nell'export | 2026-10-03 21:15:31 |
| Capture ID | `-2106976322392918221` |
| Durata | 2,122882 secondi |
| Clock CPU / conversione tick | 333.000.000 Hz |
| SELF dichiarato | `F:\Ubuntu\command-ring\build-vita\strikers_debug_debug.self` |
| Strikers di partenza | `369c539dd7a5fc27f82e0a52c8b3df49773c8a40` |
| Aurora di partenza | `910c4b89977652a8e3770c1f5e256fc87758db5a` |

Il database non contiene l'hash del SELF, l'INI installato o la revisione del sorgente della cattura. Il suo timestamp precede entrambi i commit di partenza, pubblicati il 4 ottobre alle 01:45 CEST. Il nome dell'eseguibile contiene `debug`, ma non basta a ricostruire i flag del compilatore. I valori descrivono questo eseguibile durante questa breve cattura; non sono una baseline verificata del checkout attuale.

L'export contiene 83 righe `Controller Sync` e una `Dummy Sync`, senza `GPU Flip`. Non vanno trasformate in FPS. Anche il numero di chiamate a Render/FixedUpdate risente dei bordi della cattura e non misura i frame presentati.

L'analisi ripetibile è in [analyze_vita_cpu_trace.py](smstrikers-port/tools/analyze_vita_cpu_trace.py). Il risultato completo, inclusi i chiamanti e i thread, è salvato nell'artefatto locale [analysis.json](ab-artifacts/cpu-trace-20261005/analysis.json). Il database originale non viene aggiunto a Git.

```sh
python3 smstrikers-port/tools/analyze_vita_cpu_trace.py "/Users/robin994/Downloads/trace (3).db" --output ab-artifacts/cpu-trace-20261005/analysis.json
```

## Costi esclusivi misurati

I millisecondi sono cumulati sull'intera cattura, aggregando i thread del processo del gioco. Il tempo esclusivo attribuisce una funzione senza sommare di nuovo i suoi figli. Le barre inclusive dello screenshot comprendono i figli e non devono essere sommate tra loro. Il tempo di una chiamata GXM sul processore non è il tempo di esecuzione GPU.

| Funzione | Tempo esclusivo | Chiamate | Implicazione |
| --- | ---: | ---: | --- |
| `memcpy` | 630,357 ms | 375.795 | È il maggiore costo aggregato; serve identificare i chiamanti. |
| `salCtrlDsp` | 210,881 ms | 413 | Il DSP software audio ha un costo significativo, su un thread separato. |
| `fifo::process` | 98,063 ms | 2.005 | Il frontend GX continua a interpretare il flusso. |
| `DrawSink::submit` | 95,433 ms | 10.568 | Preparazione dei draw frequente, oltre ai costi dei figli. |
| `nlMultMatrices` | 91,696 ms | 121.291 | Wrapper molto frequente; eliminata la protezione alias duplicata. |
| `memcmp` | 71,864 ms | 156.926 | In gran parte dentro il binding della pipeline GXM. |
| `memset` | 67,151 ms | 189.067 | Una parte importante azzera gli snapshot fixed vertex. |
| `decode_vertex_ops` | 63,992 ms | 78.925 | Decodifica dei vertici; valutare di nuovo dopo gli interventi già presenti. |
| `sampleAt` | 63,725 ms | 335.543 | Risoluzione dei dati e controlli ripetuti per campione audio. |
| `HandleReverb` | 52,934 ms | 824 | DSP audio; non riscritto senza un confronto del risultato audio. |
| `cPoseAccumulator::operator=` | 46,310 ms | 1.284 | Copie di vettori di matrici e accumulatori. |
| `PSMTX44Concat` | 45,980 ms | 41.936 | Manteniamo l'aritmetica e la gestione alias del percorso originale. |

Attribuzione delle principali copie e dei confronti:

| Operazione e chiamante | Tempo esclusivo dell'operazione | Chiamate | Stato |
| --- | ---: | ---: | --- |
| `memcpy` da `GXCallDisplayList` | 363,758 ms | 8.953 | Commit Aurora di partenza: shadow immutabile con lifetime conservato attraverso il job async. |
| `memcpy` da `Renderer::bind_pipeline` | 53,763 ms | 21.974 | Commit di partenza: identità/revisioni degli uniform. |
| `memcmp` da `Renderer::bind_pipeline` | 65,428 ms | 124.059 | Riduzioni già presenti nel commit di partenza. |
| `memcpy` da `DrawSink::submit` | 42,513 ms | 73.131 | Diverse strutture; il solo totale non autorizza a eliminare tutte le copie. |
| `memcpy` da `enqueue_job` | 40,560 ms | 830 | Trasporto FIFO e lifetime; preservati ordinamento e fence. |
| `memset` dal builder degli uniform fixed vertex | 28,926 ms | 16.370 | Nuova patch: eliminati l'azzeramento completo per draw e il reset superfluo degli slot riutilizzati. |
| `memcpy` dal builder degli uniform fixed vertex | 23,360 ms | 8.185 | Rimane la copia necessaria per pubblicare lo snapshot immutabile. |

Le righe di attribuzione sono sottoinsiemi dei totali sopra. Non rappresentano ulteriori tempi da sommare. Nessuna patch promette di recuperare integralmente un costo: parte delle copie resta necessaria e la strumentazione del profiler altera il lavoro osservato.

## Distribuzione del lavoro sui core

| Core | Processo del gioco sul core | Idle del core |
| --- | ---: | ---: |
| CPU0 | 76,94% | 21,57% |
| CPU1 | 30,02% | 69,22% |
| CPU2 | 60,96% | 37,50% |
| CPU3 | 1,78% | 82,97% |

Il thread principale occupa CPU0 per 1.577,961 ms, il frontend GX CPU2 per 1.183,210 ms, il worker audio CPU1 per 413,155 ms. CPU3 esegue `aurora_cpu_3` per 37,851 ms. Altri processi e intervalli non registrati spiegano il resto del tempo; `100 - idle` non coincide con l'utilizzo del gioco.

Il trace dimostra che CPU3 viene usata da questo eseguibile, con una quota piccola. Non dimostra che spostare arbitrariamente lavori su CPU3 dimezzi il frame time. Serve misurare i punti di attesa e la granularità dei job nella nuova build; la politica del budget CPU3 resta quella del port.

## Modifiche implementate

### Snapshot fixed vertex

`FixedVertexUniformBuilder` conserva un payload completo e aggiorna gli input attivi a ogni draw. Pulisce palette, luci e matrici texture solo quando passano da attive a inattive. Legge i dati a ogni chiamata, anche senza cambiamenti di revisione: non presuppone che un contatore intercetti tutte le scritture GX.

`FixedUniformPool::publish_from` copia il candidato completo nello slot immutabile senza azzerare prima uno slot già allocato. La pubblicazione mantiene indirizzi stabili e revisioni distinte. Quando la condivisione è abilitata, resta il confronto esatto dei byte del payload. Gli sprite ricevono snapshot distinti prima di impostare i parametri di espansione. L'API pubblica `emplace_back()` continua a restituire un oggetto inizializzato.

Il nuovo fallback `GxmDisableFixedBuild=0x10000` ripristina il builder completo. È indipendente da `GxmDisableFixedSnapshot=0x8`, che mantiene il significato precedente per condivisione e revisioni degli snapshot. Con il default del port `gxm_disable=0x8`, il nuovo builder è attivo e la condivisione rimane disattivata. Nessun binding GXM viene conservato oltre BeginScene.

### Matrici e pose

`nlMultMatrices` chiama direttamente `PSMTX44Concat`: la funzione Dolphin protegge già entrambi gli alias di output. Rimosso il secondo temporaneo e la protezione duplicata del wrapper, mantenendo operazioni e ordine dell'aritmetica originali.

`Vector<T>` usa una copia in blocco quando `T` è sia trivially copyable sia trivially copy assignable. Matrici e accumulatori delle pose soddisfano queste condizioni. I tipi con operatori di assegnazione significativi conservano la copia per elemento. Dimensione, capacità, regola di crescita e layout del contenitore restano invariati. L'autoassegnazione non copia più i propri dati.

### Mixer audio

Il mixer risolve puntatore, formato e dati aggiuntivi del campione una volta per voce e tick, dopo l'eventuale inizializzazione della voce. Il ciclo per campione usa questa vista e non richiama `sampleBytes`/`PortAramResolve` a ogni avanzamento. La vista non sopravvive al tick, così una successiva sostituzione del ring buffer può essere riletta.

Restano i clamp dei campioni normali e il comportamento dei formati stream, le storie ADPCM e i loop, il resampling e i volumi. La conversione PCM8 negativa usa la moltiplicazione per 256, evitando lo shift a sinistra di un valore negativo. Non è stata riscritta la catena DSP/reverb.

### Analisi del trace

Il nuovo tool legge le tabelle del database in sola lettura, converte i tick con `ClockRate`, filtra gli oggetti funzione del processo del gioco e conserva l'attribuzione ai chiamanti e ai thread. Riporta occupazione per core, idle, identità della cattura e hash del file. Il campo FPS rimane esplicitamente sconosciuto.

## Verifiche locali

- Port: 16/16 test host superati. Il nuovo test esegue 1.082 controlli con la reale implementazione delle matrici: alias, copia/growth/self-assignment dei vettori, operatori non banali, accumulatori della posa, PCM8/PCM16, endian, clamp e sostituzione dei buffer.
- Aurora: 13/13 test host superati, includendo frontend GX, command stream, submission, worker CPU e regressioni. Il test del nuovo builder esegue 12.342 controlli, con 4.096 sequenze confrontate esattamente con il builder originale, cambi di input senza revisione, palette, luci/bump, texture/postmatrix, selettori fuori intervallo, NaN/signed zero e snapshot immutabili attraverso overflow/reset.
- AddressSanitizer/UBSan: test dei nuovi percorsi di matrice/vettore/PCM e del builder/pool grafico superati. Questa verifica non esercita l'intero mixer audio o l'esecuzione su Vita.
- Aurora standalone: configure e build `vita-gxm` superati.
- Build completa Strikers Vita GXM: VPK generato. Audit dell'ELF e della link map superato su 12.676 simboli eseguibili: draw/present GXM presenti, nessuna API o libreria GL/vgl/vita2d nel confine verificato. Il SELF e l'eboot nel VPK sono identici.
- Disassembly ARM del binario finale: `nlMultMatrices(nlMatrix4&,...)` è diventata quattro movimenti di registri e un branch diretto a `PSMTX44Concat`, senza stack frame del wrapper.

La build emette warning del progetto, inclusi mismatch degli attributi ABI wchar/enum al link e lo shift a 32 bit in `ScriptCaching.h`. Gli stessi messaggi sono presenti nel log del build del 2 ottobre; non sono stati risolti da questo intervento. Il log della build corrente non contiene errori. La compilazione e l'audit non sostituiscono il controllo visuale/audio e il profiling sul dispositivo.

## Artefatti della build finale

| Artefatto | SHA256 |
| --- | --- |
| [VPK candidato](ab-artifacts/cpu-trace-20261005/strikers-cpu-trace-20261005.vpk) | `d7dc19b9110f695cc7dfdab5da2807f92d886a6d3c62273a841e808474bf849c` |
| [SELF / eboot candidato](ab-artifacts/cpu-trace-20261005/strikers-cpu-trace-20261005.self) | `87e058e15b5983729a783bc3aa1bf2ec8944f208657b6418c624a16d83d09fa8` |
| [ELF candidato](ab-artifacts/cpu-trace-20261005/strikers-cpu-trace-20261005.elf) | `2128de749817140c30383a957c48f067a973a845be973096a9346360e60b9b0f` |
| VPK locale precedente conservato | `27ca378d0db49830f5eeaeed4a92bbcc04bcc0914b1772fca971c145e1a2066d` |
| SELF locale precedente conservato | `0aa49a3297c893167c5716ae6239473ca19878387d5b4d02ed337a67a712edd3` |

Versione runtime: `1.3.0-cpu-trace-20261005`. Il [manifest](ab-artifacts/cpu-trace-20261005/build-manifest.json) registra gli hash degli artefatti, i flag CMake, il toolchain e l'identità delle modifiche locali, includendo separatamente il sorgente Aurora embedded. L'hash dell'eboot installato rimane sconosciuto: nessuna installazione è stata eseguita.

I [log dei test del port](ab-artifacts/cpu-trace-20261005/strikers-host-tests.log), [test Aurora](ab-artifacts/cpu-trace-20261005/aurora-host-tests.log), [build Vita](ab-artifacts/cpu-trace-20261005/vita-build.log) e [audit GXM](ab-artifacts/cpu-trace-20261005/binary-audit.log) sono conservati insieme agli artefatti. I controlli sanitizer sono in `strikers-sanitizers.log` e `aurora-sanitizers.log` nella stessa directory.

Le suite host usano i target CMake del repository. Per il port, il progetto host isolato in `/tmp/strikers-host-contracts` fornisce `port_flags` con include del prelude, `TARGET_PC=1`, `-fdeclspec`, `-fshort-wchar`, `-fno-strict-aliasing` e `-fsigned-char`, poi aggiunge `smstrikers-port/tests`. Non richiede dati del gioco.

```sh
# Aurora, dalla directory smstrikers-port/extern/aurora-vita
cmake --preset vita-host-tests
cmake --build --preset vita-host-tests --parallel 8
ctest --preset vita-host-tests --parallel 4
VITASDK=/usr/local/vitasdk cmake --preset vita-gxm
cmake --build --preset vita-gxm --parallel 8

# Strikers, dalla root: cache locale precedente, stessa topologia async
VITASDK=/usr/local/vitasdk cmake -S smstrikers-port -B smstrikers-port/build-vita-gxm-latest -DSTRIKERS_VERSION=1.3.0-cpu-trace-20261005
cmake --build smstrikers-port/build-vita-gxm-latest --target strikers_vita.vpk-vpk --parallel 8
```

Il build locale conserva `Release`, GXM, GX thread/async ON, direct stream/write submission ON, distinct CPU cores ON, immediate draw view ON, LTO ON, shader profile SEALED e CMPR nativo OFF. Il vecchio VPK e SELF sono conservati in `ab-artifacts/cpu-trace-20261005/control-build/`. Non corrispondono necessariamente al SELF del trace allegato.

## Confronto su Vita e lavoro successivo

Per isolare il nuovo builder nel medesimo binario: mantenere la configurazione attuale, aggiungere `0x10000` alla sua maschera `gxm_disable` per il controllo, poi rimuovere solo quel bit per il candidato. Nel profilo predefinito il confronto è `0x10008` contro `0x8`; la condivisione degli snapshot resta OFF in entrambi. Riavviare il gioco tra i run.

Per il risultato complessivo di matrici, pose, audio e builder occorre confrontare il vecchio e il nuovo binario, con stesso INI, scena, squadre, replay/input, clock effettivi e cache shader calda. Registrare hash di eboot installato, hash INI, log di avvio e almeno tre run, confrontando median/p95/p99, screenshot e qualità audio. Usare frame realmente presentati o i dati del benchmark pertinente, senza ricavare FPS dai Controller Sync. Il nuovo trace va acquisito sul candidato per trovare ciò che resta costoso.

Questo applica al port la parte del workflow Monster Hunter supportata dal trace: intervenire sui percorsi nativi caldi e adattare trasporto e preparazione dati alla Vita. Strikers è già C/C++ nativo ARM e non ha un software MMU PPC o un ABI del recompiler da eliminare. Una riscrittura TEV/materiali, conversioni di texture o nuovi batch permanenti richiedono profiling GPU e confronti visuali specifici; la cattura CPU non ne misura il beneficio. LOD escluso dalla presente modifica.

Le patch e il report rimangono nel working tree. Non è stato effettuato deploy sulla Vita, né commit o push. Il guadagno FPS rimane da misurare sul dispositivo.
