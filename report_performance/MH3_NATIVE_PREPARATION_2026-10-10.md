# Strikers: prima implementazione della preparazione nativa

Data: 2026-10-10. Stato: sorgenti e verifiche host completati; verifica Vita da eseguire con build manuale.

## Risultato e perimetro

Questa tranche applica al percorso nativo esistente il principio della
[documentazione MH3](https://github.com/Grarak/mh3-wii/blob/main/docs/optimizations/README.md):
preparare il lavoro riutilizzabile e aggiornare solo gli input modificati.
Riduce le chiamate CPU di traduzione vertex/fragment e risoluzione delle
texture nei draw nativi ammessi con stato invariato.

Il test sintetico confronta ogni risultato con una traduzione completa in
storage nuovo: dopo la preparazione iniziale, 1.000 riusi dello stesso modello e stato
non richiedono altre traduzioni vertex/fragment. Nel test integrato, un
secondo draw nativo identico non incrementa nemmeno `textureResolves`.
Sono conteggi di lavoro host, non misure del guadagno FPS su Vita.

La preparazione del materiale avviene al primo uso valido, perché GLX
dipende anche dalla vista e dallo stato corrente. Il loader registra la
generazione e il proprietario della display list permanente. La conversione
della geometria continua a usare la cache residente esistente al primo draw.
Non viene ancora compilata tutta la geometria durante il caricamento GLG.

## Modifiche

- `glxDisplayList.cpp`: registra le display list permanenti dopo la loro
  costruzione, con identità, intervallo di byte e livello del resource marker.
- `glxMemory.cpp`: prima di riavvolgere l'arena, ritira le sorgenti dei livelli
  rilasciati attraverso una callback ordinata nella FIFO GX.
- `glxSend.cpp`: include la generazione nel descrittore del modello. Con la
  nuova opzione attiva, una lista temporanea o non registrata conserva il
  draw GX originale, anche quando il catalogo delle sorgenti è pieno.
- Aurora `native_model_sources.hpp`: catalogo a capacità fissa di 1.024
  sorgenti, generazioni monotone, collisioni e tombstone, senza espulsione di
  asset ancora vivi. Le generazioni attestano il ciclo di vita; non attestano
  l'immutabilità dei byte. La memoria del catalogo è inclusa nei conteggi
  dei metadati producer.
- Aurora `vita_native_model_uniform_cache.hpp`: conserva un solo snapshot CPU
  vertex/fragment del modello corrente e aggiorna i domini cambiati. Identità
  GX, programma vertex, ricetta, revisioni e invalidazioni esplicite fanno
  ricostruire gli input necessari. Nessun indirizzo di una riserva uniform
  GXM viene conservato in questa cache.
- Aurora `aurora_vita_draw_sink.cpp`: il percorso nativo riusa il resolver
  delle texture esistente, con controlli su revisioni, sorgenti volatili,
  TLUT ed EFB. Restano i confronti di pipeline/materiale/layout, la verifica
  della geometria residente e il fallback GX nella stessa posizione FIFO.
- Aurora `vita_static_geometry.hpp`: rimuove le identità CPU delle geometrie
  che referenziano una sorgente rilasciata. I buffer GPU restano conteggiati
  e vivi fino al ritiro esistente: quattro frame e barriera GPU idle.
  I metadati consumer nativi vengono invalidati; le mesh di altre sorgenti
  restano residenti e possono essere riammese.

Le dipendenze dei fragment uniform includono la viewport quando è presente
la nebbia e dimensioni/LOD delle texture per il TEV indiretto. Il calcolo
della revisione viene condiviso con il percorso ordinario. Il TEV indiretto
rimane escluso dalla cattura nativa esistente; il test del builder ne verifica
separatamente la dipendenza dagli input. Un draw ordinario che consuma
`stateDirty` senza una nuova revisione invalida anche lo snapshot nativo.

I sanitizer hanno inoltre segnalato una lettura FIFO tramite un puntatore
tipizzato con allineamento non garantito. Il loader a 16/32 bit ora accetta
un puntatore ai byte e mantiene la copia sicura. Il test di equivalenza delle
texture confronta tutti i campi del binding e del sampler, compresi min/max
LOD, invece dei byte di padding della struttura.

## Configurazione e rollback

La nuova opzione è disattivata nei default gestiti:

```ini
gxm_native_model_prepared = 0
```

Per una futura prova A/B sulla stessa build, usare override esterni al blocco
gestito e riavviare il gioco. Mantenere identici modalità nativa, cache,
builder uniform, clock, contenuto, risoluzione e altre opzioni; variare solo
`gxm_native_model_prepared` fra 0 e 1. L'opzione richiede modalità nativa 1
o 2 e il worker GX già richiesto dal trasporto delle ricette. Non richiede
di abilitare lo streaming GPU dei vertici.

Il controllo deve prima verificare la build con la correzione async/shared_ptr
già presente nel checkout e documentata in
[AURORA_VITA_R2_GTHREAD_COREDUMP_2026-10-10.md](AURORA_VITA_R2_GTHREAD_COREDUMP_2026-10-10.md).
Quella correzione non ha ancora una conferma hardware nella presente sessione.

Rollback del nuovo esperimento: impostare solo
`gxm_native_model_prepared = 0` e riavviare. Le correzioni delle dipendenze
fragment e della lettura FIFO restano attive. Nessun INI sul dispositivo è
stato modificato.

## Verifiche completate

| Verifica | Risultato | Limite |
| --- | --- | --- |
| Baseline Aurora prima delle modifiche | 35/35 test host | Checkout già modificato |
| Suite Aurora finale Release | 37/37 | Frontend reale, renderer host VitaGL |
| AddressSanitizer + UndefinedBehaviorSanitizer | 7/7 test pertinenti | Host macOS, `halt_on_error=1`; LeakSanitizer non disponibile |
| Configurazione Vita | PASS | Generazione default, refresh e conservazione override |
| Sintassi dei tre file GLX modificati | 3/3 | Clang host; controlli ABI Vita e define piattaforma adattati |
| Controllo whitespace dei diff | PASS | Repository principale e Aurora incorporato |

I test includono riuso, cambio di posa/normal/light, materiale e identità GX,
uniform immutabili già in coda, passaggi ordinario/nativo, maschere di
riferimento, viewport/nebbia, texture/TLUT/EFB, pressione della cache,
mutazione della sorgente, rilascio con draw pendente e ricaricamento allo
stesso indirizzo. Il catalogo verifica anche capacità piena e rimozione
della testa di una catena di collisioni.

L'adattamento GLX usa gli include del database di compilazione esistente,
mantiene `PORT_VITA` e rimuove i controlli ABI a 32 bit del prelude,
i define specifici del dispositivo e le opzioni ARM/LTO. Verifica sintassi e
interfacce sul computer; non sostituisce una compilazione/link Vita.

Le evidenze, i comandi, le revisioni base e gli SHA-256 dei sorgenti sono in
[evidence/MH3_NATIVE_PREPARATION_2026-10-10](evidence/MH3_NATIVE_PREPARATION_2026-10-10).
Il checkout conteneva già lavoro R1/R2 e VitaGL non committato: è stato
conservato. I manifest identificano il contenuto corrente completo dei file,
compresi gli interventi precedenti dove si sovrappongono.

## Lavoro ancora necessario per l'approccio completo

Questa tranche non riduce il numero di draw GPU né elimina i callback GLX o
il setup GX precedente al draw. Restano da sviluppare la compilazione della
geometria/materiale al caricamento, un ingresso nativo che sostituisca il
producer GLX per i casi coperti, il batching dei draw adiacenti compatibili e
lo skinning GPU dedicato. Le callback di posa, le copie EFB e le barriere
devono conservare ordine e semantica.

Prima di attribuire un guadagno di prestazioni, servono una build Vita
manuale, identità esatta del SELF/INI, partita funzionante e confronto A/B
con catture equivalenti. Le prove diagnostiche di invalidazione vanno
separate dalle misure quiete di FPS. Nessuna build Vita, installazione,
commit o push è stata eseguita in questa tranche.
