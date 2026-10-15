# Strikers Vita — interventi conseguenti all'audit

Base: [audit dell'as-is](AS_IS_AUDIT_2026-10-12.md).
Questi ticket descrivono lavoro successivo; non sono modifiche già implementate.
Non attribuiscono FPS previsti a patch ancora da misurare.

## T0 — Esito affidabile della cattura FPS

**Scopo.** Distinguere raccolta incompleta, successo ed errore di output
senza aggiungere scritture durante ogni frame.

**File.** `src/platform/benchmark.c`, `include/port/benchmark.h`,
`tests/frame_capture_test.c`, sotto `smstrikers-port`.

**Modifica delimitata.** Pubblicare stato, frame eleggibili/campionati ed
errore; verificare allocazione, apertura, scrittura e chiusura. Mantenere
il buffer se il salvataggio fallisce e consentire un tentativo esplicito
successivo, senza retry nel loop di gioco. Percorso remoto fisso.
Non modificare clock, simulazione, renderer o preset grafici.

**Accettazione.** Test di apertura negata, scrittura corta/fallita e
chiusura fallita; dati recuperabili e nessun successo falso. Su Vita,
un nuovo CSV completo correlato a SELF/INI e progressione live. Finché
questo manca, nessun numero FPS della build corrente viene promosso.

**Fallback.** Cattura OFF; dati di errore in RAM, gioco funzionante.
Dipendenze: nessuna. È infrastruttura di misura, non una patch FPS.

## T1 — Attribuzione sicura con GX asincrono

**Scopo.** Misurare producer e consumer senza leggere strutture mutate
dal worker e senza aggiungere un'attesa a ogni vista/draw.

**File.** `src/NL/gl/glPlat.cpp`, `src/NL/nlTask.cpp`,
`include/port/vita_performance_capture.hpp`; Aurora
`platforms/vita/aurora_vita_backend.cpp`, `lib/gx/fifo.cpp` e telemetria.

**Contratto.** Producer: timer locali di pose, preparazione packet/stato,
enqueue e attese. Consumer: scope e contatori propri, con pubblicazione a
completamento e identificatori di frame/vista. Dichiarare quali scope sono
inclusivi ed evitare di sommarli. Tenere separati CPU attiva, tempo
trascorso e attese quando le API consentono di misurarli.

**Accettazione.** Nessuna lettura diretta concorrente di `telemetry().frame()`
o dell'accumulatore FIFO nel profilo per vista. Sequenza e coerenza degli
snapshot verificate; nessun `run_sync` introdotto per draw/vista. Cattura
limitata in RAM, contatori OFF nel controllo quiet. I profili diagnostici
non vengono confrontati come FPS con le prove quiet.

**Fallback.** Profilo per vista disattivato; snapshot completati per frame.
Dipendenza: T0 per la successiva verifica quiet, non per il contratto interno.

## T2 — Controllo quiet e costo del menu debug

**Scopo.** Separare variabilità del controllo e costo residuo del registro shader.

**Metodo.** Nessuna nuova build necessaria se il SELF corrente resta valido.
Prima ottenere almeno due controlli identici; poi almeno tre coppie
alternate cambiando soltanto `vita_debug_menu=0/1`. Preservare seed,
clock effettivi, passo simulazione, archivi e frame window. Un solo CSV
remoto riutilizzato, con evidenza piccola sul computer e ripristino esatto
dell'INI. Verificare che nessuno shader sia soppresso durante la misura.

**Accettazione.** Guadagno superiore alla dispersione del controllo;
media/p95/p99 riportate anche se il risultato è nullo. Nessuna inferenza
di 60 FPS dal semplice spegnimento del debug.

**Fallback.** Configurazione originale; nessuna modifica permanente ai
default durante l'esperimento. Dipendenza: T0 e baseline completa.

## T3 — Contratto preparato per una classe reale di Characters

**Scopo.** Saltare lavoro ripetuto prima della callback GLX/GX, coprendo
una classe di mesh `3d_pointlit` realmente presente in partita.

**File interessati.** `src/NL/glx/glxSend.cpp`, caricamento/ownership GLG,
interfacce FIFO Aurora, `aurora_vita_draw_sink.*` e risorse GXM preparate.
Congelare l'interfaccia prima di estendere i caricamenti o altri pass.

**Contratto minimo.** Handle/generation della mesh e materiale persistenti;
layout e indici equivalenti; identità shader/TEV; binding texture e relative
revisioni; palette posizione/normali, camera, luci, texgen e parametri
dinamici in uno snapshot posseduto dal job. L'owner GXM resta il worker.
Il successo deve evitare emissione e traduzione GX coperte; una cache
consultata dopo averle eseguite non soddisfa il ticket.

**Prima del codice.** Dal profilo T1 selezionare una classe con tempo e
copertura significativi. Elencare dipendenze statiche/dinamiche e stato GX
osservabile dalla draw di fallback successiva. Non riusare il prototipo
cross-mesh che ha fallito l'oracolo. Non copiare implicitamente assunzioni
static/unlit nel percorso lit/skinned.

**Accettazione di correttezza.** Confronto contro preparazione GX fresca
su cambi camera/posa/luci/materiale, alternanza nativo/fallback, retirement
e risorse in-flight. Stesso numero/ordine delle draw e indici; nessuna
draw di riscaldamento aggiuntiva. Verifica su Vita di campo, personaggi,
ombre, effetti, HUD e gameplay avanzato, oltre ad audio/input/stabilità.

**Accettazione prestazionale.** Dimostrare runtime admission/hit/fallback
e riduzione del lavoro coperto, contando anche guardie e miss. Poi almeno
tre coppie quiet alternate sulla stessa build. Riduzione del tempo totale
e delle attese coerente con il lavoro eliminato, senza promozione di un
guadagno minore del rumore né peggioramento sistematico delle code.

**Fallback.** Opt-in OFF; GX originale nello stesso ordine e slot, senza
resettare il renderer globalmente. Dipendenze: T0, T1 e T2 come controllo.

## T4 — Shadowed e riduzione delle draw compatibili

**Scopo.** Estendere soltanto dopo T3, con un contratto specifico per le
ombre. Successivamente misurare opportunità di raggruppare draw consecutive.

**Vincoli.** Palette/parametri equivalenti, primitive compatibili, stessa
politica depth/blend/scissor, nessun attraversamento di EFB, pass o barrier.
Mesh di personaggi diversi non sono automaticamente raggruppabili.

**Accettazione.** Oracolo di ordine/dati e visivo hardware; meno draw reali
oppure minore preparazione totale, mai un costo nascosto per mantenere la
cache. Nuovo A/B isolato. Fallback: T3 e GX per Shadowed.

## T5 — I/O e validazione delle risorse, sul costo residuo

**Scopo.** Ridurre riletture e picchi dimostrati senza sottrarre memoria
alla geometria/texture utile. Estendere la misura alle identità dei blocchi
PSARC prima di scegliere una cache; separare shader e salvataggi.

**File.** `src/platform/asset_archive.c`, `src/platform/native_assets.cpp`,
`include/port/asset_io_capture.hpp`; cache geometrica Aurora se il nuovo
profilo dimostra che la validazione rimane rilevante.

**Accettazione.** Byte equivalenti, errori/bounds corretti, cache limitata,
riduzione di letture/decompressioni ripetute e miglioramento p95/p99 misurato
senza peggiorare memoria, eviction o frame medio. Per gli handle geometrici,
lifetime e invalidazione devono sostituire i controlli rimossi.

**Fallback.** Lettura originale e cache corrente. Dipendenza: evidenza
specifica di riuso/costo; non introdurre una cache generica preventivamente.

## Regola di consegna per ogni ticket

Usare sempre `smstrikers-port/build-vita-gxm-latest`, impostando esplicitamente
profiling ON/OFF. Conservare al massimo una build precedente con nomi
`.previous` nella stessa directory. Nessuna copia completa in `ab-artifacts`.
Consegnare hash SELF/INI, opzioni, piccole catture e decisione
promosso/rifiutato/inconcludente. Il criterio finale dei 60 FPS riguarda
frame reali entro 16,667 ms e qualità/stabilità del gioco, non un valore
forzato del timestep o la sola media di una sequenza favorevole.
