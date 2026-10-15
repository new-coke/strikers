# Strikers Vita: primo percorso nativo per draw statiche

Data: 2026-10-09. Richiesta: proseguire l'ottimizzazione di Aurora/Strikers verso
60 FPS, dopo la verifica degli asset nativi e le prove di memoria.

## Stato e criterio di accettazione

È implementata una prima cache di ricette di draw, attivabile con
`gxm_native_model_draw=1`. Il valore predefinito è **0**. Il percorso copre
display list statiche con una sola draw, materiale unlit senza texture e array
di posizione/colore. Riutilizza la pipeline e i buffer geometrici residenti;
aggiorna matrici, camera, uniformi, viewport e scissor con lo stato corrente.

La conversione completa di modelli e materiali in record nativi persistenti
**non è completata**: restano la validazione dello stato, la decodifica dei
comandi di prelude/postfix della draw e il normale invio GXM. Personaggi,
skinning, texture, materiali illuminati, indiretti e copie EFB non rientrano
in questa prima implementazione. Il numero di hit sul dispositivo stabilisce
la copertura reale; un test host o una build non dimostrano un aumento degli FPS.

Le prove hardware e lo stato finale della console sono registrati nella sezione
risultati e in `ab-artifacts/native-model-20261009/experiment-result.json`.

## Implementazione

1. Il produttore intercetta una callback GLX idonea e registra i suoi comandi
   originali, senza forzare ulteriori scritture GX. La prima invocazione esegue
   l'intera sequenza registrata. Se cambia materiale o binding degli array, la
   callback rimane nel percorso originale.
2. Le invocazioni successive con stato del materiale invariato eseguono le
   eventuali scritture di matrice tramite la funzione originale e accodano una
   ricetta tipizzata nello stesso FIFO. Il produttore non chiama GXM direttamente.
3. Il consumatore verifica identità della display list, configurazione canonica
   della pipeline, layout degli array, revisioni della memoria e disponibilità
   dei buffer. Risolve l'entry prima di dereferenziare il suo puntatore.
4. Se la ricetta è valida, pubblica una nuova snapshot immutabile delle uniformi
   e accoda un DrawPacket con i buffer residenti. Se una verifica fallisce,
   esegue la display list GX originale nello stesso punto del FIFO.

Le display list con più draw non possono alimentare la cache di draw singole.
Overflow della registrazione e chiamate esterne sincrone riproducono i byte già
registrati prima di proseguire. Restano invariati ordine FIFO/EFB, scissor esatto,
reset completo dello stato GXM a BeginScene e ritiro dei buffer con attesa GPU.

File principali:

- `smstrikers-port/src/NL/glx/glxSend.cpp`: ammissione, cache GLX limitata a 64
  ricette, confronto dei campi semantici del packet e matrice corrente.
- `smstrikers-port/extern/aurora-vita/lib/gx/native_model_recipe.hpp` e
  `lib/gx/fifo.cpp`: proprietà della ricetta, trasporto ordinato e fallback.
- `smstrikers-port/extern/aurora-vita/platforms/vita/gx/aurora_vita_draw_sink.cpp`:
  preparazione e riuso dei DrawPacket con guard sullo stato corrente.
- `smstrikers-port/extern/aurora-vita/platforms/vita/gfx/vita_static_geometry.hpp`:
  validazione delle entry residenti anche fra frame diversi.
- `smstrikers-port/include/port/vita_performance_capture.hpp`: contatori cumulativi
  `native_model_attempts`, `native_model_draws`, `native_model_fallbacks`,
  `native_model_compiled`. Le differenze fra gli endpoint sono il numero di
  eventi nell'intervallo; non sommare i valori cumulativi riga per riga.

## Memoria

La cache geometrica rimane a **8 MiB**. Questo esperimento non ripete la prova
a 16 MiB che ha preceduto il blocco completo della console. Quel blocco, privo
di coredump nuovo, non stabilisce una causa certa di esaurimento della CDRAM.

Le ricette sono strutture CPU: massimo 96 ricette vive, comprese quelle trattenute
da job accodati; massimo 8 KiB per ciascuna sequenza prima/dopo la draw. Il limite
dei payload di stato è quindi 1,5 MiB, oltre a strutture, mappe e metadati FIFO.
La cache dei packet compilati è limitata a 64. Questo è un limite di contenuto,
non una garanzia sulla memoria totale dell'applicazione. Nessuna nuova copia di
buffer geometrici GPU viene introdotta dalle ricette.

La prova hardware usa l'arena sorgente esistente da 48 MiB in RAM cached, uguale
per OFF e ON. La precedente prova ABBA RAM/CDRAM aveva misurato circa 20,19
contro 19,38 FPS aggregati, con dispersione fra run superiore al vantaggio:
non costituisce un miglioramento ripetibile accertato.

## Validazione locale

- Suite Aurora host: **25/25** test superati, inclusa la nuova ricetta nativa.
- Il nuovo test confronta una draw nativa con l'esecuzione GX originale nello
  stesso stato e verifica camera, pose indipendenti, scissor, transizioni raster,
  scritture della display list, array INDEX16 di posizione/colore, cambio binding,
  scritture degli array, display list composte, overflow e limiti di proprietà.
- Test della configurazione predefinita: superato; la nuova opzione è disattivata.
- Analizzatore dei contatori: 3/3 test superati, inclusa la corretta interpretazione
  dei quattro contatori nativi come differenze fra endpoint cumulativi.
- Build Strikers Vita e probe GXM standalone: superate.
- Audit Strikers: 12.824 simboli eseguibili, draw/present GXM presenti, nessuna
  entry point o libreria GL/vgl/vita2d. Audit del probe GX: 2.269 simboli, superato.
- L'eboot incluso nel VPK coincide byte per byte con il SELF candidato.

Il codice parte da un working tree già modificato. Non sono state eliminate
le modifiche precedenti. Le patch prima dell'intervento sono conservate in
`ab-artifacts/native-model-20261009/pre-edit/`; gli artefatti e le patch della
versione candidata sono in `candidate/`. Le patch complete comprendono anche
le modifiche precedenti, perciò il solo HEAD non identifica questo candidato.

## Identità e protocollo hardware

- Root HEAD: `031923fbe23240eea4b698e5c9befc893e006372`.
- Aurora embedded HEAD: `6acc6a889d0bec1fb206b1c73cb693a434c132df`.
- SELF candidato: `578efa942ec08bf2a0b0306f7979af4ca06bc731b9d07419972913a304e899c9`.
- VPK candidato: `19bf4c0add9be796b2e347ba59bc628d313762db70765584e824766656dd93eb`.
- SELF originale: `0de405e4fa3891e04b298d6e8b8198ca8fda7e7ddb3b57d08444f635337e45f3`.
- INI originale: `efb59a686ba0c3e0ffca1fbbb952d053859361ef556e0d59872fe2ff7bb0a40c`.

Prima dell'installazione è riuscito il ripristino verificato del binario e
dell'INI originali, che nella precedente sessione era bloccato dalle scritture
FTP. L'installazione atomica del candidato è stata verificata tramite readback.
ELF, map e VELF corrispondenti sono conservati localmente.

Si conserva la configurazione di build trovata nel checkout:
`AURORA_VITA_ASYNC_GX=ON`, `STRIKERS_VITA_GX_THREAD=ON`. Nessuna nuova modalità
di scheduling viene attivata da questa prova. Il riferimento quiet OFF e la
diagnostica ON hanno modalità e finestre differenti e **non costituiscono un
confronto FPS A/B**. Replay A6, census dei vertici e streamed GPU sono
disattivati in entrambe le acquisizioni. Cache geometrica 8 MiB, arena sorgente RAM
cached 48 MiB, seed `0x53545249`, dt reale, frameskip 0. La demo è selezionata
esclusivamente attraverso l'INI temporaneo.

Le acquisizioni quiet usano 600 frame di gameplay di riscaldamento e 1.200 frame
misurati, diagnostica e overlay disattivati. Le acquisizioni diagnostiche servono
alla copertura e all'attribuzione; i loro tempi non sono una misura quiet di FPS.
I clock effettivi devono provenire dall'header del CSV, non dall'impostazione INI.

## Risultati hardware e prossimi passi

Risultato della prima variante: **nessuna copertura del nuovo percorso rilevata**.
Con il flag ON, 120 snapshot distinti di frame completati, dal frame renderer
403 al 689, riportano zero tentativi, zero draw native, zero fallback e zero
ricette compilate a entrambi gli endpoint. Poiché i contatori sono cumulativi,
anche il primo endpoint esclude compilazioni precedenti fino a quel momento.
Il log conferma `gxm_native_model_draw=1`. Il consumer non rifiuta ricette: nessuna
ricetta gli viene inviata. L'ammissione sul produttore deve essere diagnosticata
per motivo; la restrizione a due viste e a modelli unlit privi di texture è
una possibile causa della mancata copertura, non una classificazione misurata
di ciascun packet del gameplay.

Il solo riferimento quiet OFF è completo: 1.200 frame di gameplay, frame 600–1799,
clock effettivi 444/222/222/166 MHz, **21,122 FPS** aggregati, media **47,344 ms**,
mediana **46,168 ms**, p95 **56,827 ms** (quantile inferiore sul campione ordinato).
Non è un guadagno attribuibile al candidato e non dimostra un miglioramento
rispetto alle precedenti prove. Non sono stati avviati ripetuti run quiet ON/OFF
per una variante che non dispatcha draw nella scena campionata.

La diagnostica ON ha completato anche 120 frame di gameplay, dal 60 al 179,
e una snapshot al frame live 120. La cattura USB OFF mostra il titolo nella
partita; la snapshot ON mostra campo, HUD e personaggi. Questo è riscontro
visivo qualitativo, non un confronto pixel fra varianti né una verifica completa
di audio, input, tutte le viste o sospensione/ripresa.

Nelle 120 snapshot diagnostiche, il frontend draw mediano vale **26,032 ms**;
la traduzione dello stato **6,808 ms**, la decodifica vertici **2,667 ms**, la
cache geometrica **1,934 ms**, il submit CPU **4,912 ms**. Sono scope annidati:
non sommarli. La diagnostica modifica l'overlap CPU/GPU e non misura il tempo
GPU completo. Gli hit GPU della geometria nativa precedente crescono di 121
su 1.845 tentativi; gli hit della cache geometrica crescono di 57.511. Questi
contatori esistenti non sono hit della nuova ricetta.

Il log ON conferma l'arena sorgente RAM cached da 49.152 KiB. Nei sei campioni
di memoria del gameplay/intro disponibili, `free_cdram=39845888` (38 MiB),
`free_user` varia fra 27 e 34 MiB e `fallbacks=0`. Il campione finale riporta
28 MiB USER liberi. Non è prova che portare la cache a 16 MiB sia sicuro in
ogni scena. Nessun nuovo coredump è stato trovato rispetto all'elenco salvato
prima delle prove.

Il riferimento quiet corrente richiede 47,344 ms/frame: per 60 FPS il budget
è 16,667 ms, quindi serve eliminare circa **64,8%** del tempo complessivo.
Non esiste al momento una misura che dimostri questo risultato.

### Stato finale del ripristino

L'INI originale è nuovamente ripristinato e verificato byte per byte
(`restore-config-final/restore-config-identity.json`). La nuova funzione torna
disattivata perché non è presente nell'INI originale e il default è 0.
L'app è stata terminata. Il primo ripristino dell'eboot originale ha ricevuto
`FTP STOR 550 File not found`, anche dopo la terminazione e la navigazione
LiveArea. Dopo il riavvio effettuato dall'utente, con Companion attivo senza
aprire Strikers, il ripristino è riuscito. **Binario e INI originali sono ora
entrambi verificati byte per byte**. Identità e backup remoto sono registrati in
`restore-after-reboot/restore-identity.json`. Nessun candidato o test è attivo.
Lo stato finale verificato è in
`ab-artifacts/native-model-20261009/final-state.json`.

I comandi di input usati per verificare la LiveArea provengono dalla
[documentazione ufficiale di Vita Companion](https://github.com/devnoname120/vitacompanion/blob/master/README.md).

### Lavoro successivo e gate

1. **Ammissione del produttore:** contare i rifiuti per vista, programma, texture,
   stream, flag dinamici e indisponibilità del worker. File: `glxSend.cpp`, ricetta
   FIFO e snapshot. Accettazione: censimento della stessa scena con hash del SELF
   verificato e almeno un esempio di packet rappresentativo per motivo. Il gate
   di draw rimane chiuso per le categorie prive di test.
2. **Materiali statici testurizzati:** rappresentare risorse e stato del materiale
   con identità stabili; conservare ordinamento, invalidazioni texture/TLUT/EFB e
   snapshot di uniformi. File: bridge, draw sink, risorse native e test. Dipende
   dal censimento; richiede un oracle GX indipendente su cambi di binding,
   contenuto e target, più copertura ON misurata. Fallback per ogni mancata verifica.
3. **Personaggi e matrici:** estendere separatamente pose/skinning e illuminazione
   dopo il gate dei materiali; mai congelare palette o pointer di uniformi di un
   frame precedente. Dipende dai contratti di identità/snapshot. Richiede test
   su pose diverse nello stesso batch e corretta sequenza Characters/Shadowed.
4. **Performance:** ripetere OFF/ON quiet sullo stesso SELF con 600/1.200 frame,
   clock effettivi uguali, contenuto e topologia identici. Promuovere soltanto
   dopo copertura significativa, correttezza visiva e vantaggio ripetibile oltre
   la dispersione dei controlli. Il target 60 FPS rimane un gate misurato.

L'estensione a materiali testurizzati, luci e personaggi deve essere decisa dalla
copertura misurata e mantenere test separati per binding texture/TLUT/EFB,
matrici indicizzate e snapshot. Aumentare soltanto la capacità della cache
non elimina il lavoro continuativo di preparazione e invio delle draw.

## Aggiornamento: censimento di ammissione completato

Il seguito del 2026-10-09 è documentato in [NATIVE_MODEL_ADMISSION_2026-10-09.md](NATIVE_MODEL_ADMISSION_2026-10-09.md). Due finestre diagnostiche sul nuovo SELF osservano 168.635 draw callback: tutte in contesto di trasporto disponibile, zero ammesse dal filtro, zero discrepanze del classificatore. Texture, texconfig e stream extra escludono il 100% dei packet; Characters/Shadowed costituiscono circa l'85% delle callback. La restrizione del prototipo è ora una causa misurata della mancata copertura, anziché una sola ipotesi. SELF e INI originali sono ripristinati e verificati; nessun nuovo coredump. Questo seguito non costituisce un nuovo confronto quiet FPS.
