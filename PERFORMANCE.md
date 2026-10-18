# Strikers Vita: percorso verso 60 FPS

Stato al 18 ottobre 2026. Il target e' **16,667 ms per frame, senza frameskip**,
con simulazione in tempo reale, audio e contenuti originali. Le partite
misurate sono ancora intorno a 20 FPS. Una build riuscita non misura gli FPS.

## Misure attuali

Nove run hardware: tre per profilo, 600 frame live di riscaldamento e 300
campioni live per run. Stesso SELF, scenario e seed da INI; `fixed_dt=0`,
frameskip, overlay, diagnostica e timing spenti. Clock effettivi letti nelle
catture: CPU 444, GPU 222, bus 222, crossbar 166 MHz. Il clock richiesto
nell'INI non va confuso con quello effettivamente applicato.

SELF misurato, SHA-256:
`4cb243ebfdb96aa67bdea7ac88ae669d613c3025b1a1e8ceeb46eaa7add04026`.
Il [resoconto numerico](smstrikers-port/performance/2026-10-18-quiet-summary.json)
contiene risultati dei singoli run e hash degli INI riletti dal dispositivo.

| Profilo | FPS dei tre run | FPS aggregati | Media ms | p95 ms | p99 ms |
| --- | --- | ---: | ---: | ---: | ---: |
| Riferimento | 20,53 / 17,89 / 21,98 | 19,99 | 50,033 | 69,504 | 87,599 |
| Solo UV affini | 18,83 / 21,22 / 21,68 | 20,50 | 48,789 | 63,823 | 78,552 |
| Solo distanza fog al vertice | 21,89 / 20,89 / 20,10 | 20,94 | 47,766 | 58,279 | 66,609 |

Gli FPS aggregati sono campioni / tempo totale, non la media degli FPS
istantanei. Le repliche dell'esperimento sono i tre run; i 900 frame non
sono 900 repliche indipendenti. Il seed comune non rende identiche le
traiettorie quando il timestep e' reale.

I tempi medi delle due candidate sono inferiori del 2,49% e 4,53%, ma i
rispettivi intervalli FPS rientrano nella variabilita' del controllo.
Questi run **non dimostrano un guadagno attribuibile agli shader**.
Nessuno dei 2.700 frame ha rispettato il budget di 60 FPS. Dal controllo
servono circa **33,37 ms in meno per frame**, una riduzione del **66,7%**
del tempo medio, equivalente a circa tre volte il frame rate attuale.

## Cosa fa ACGC-Vita-Port

La [release v0.2.0](https://github.com/Brendonm17/ACGC-Vita-Port/releases/tag/v0.2.0)
documenta un passaggio da circa 44 a 60 FPS nelle scene esterne dense
considerate dall'autore. Tra gli interventi: copertura delle configurazioni
shader prima gestite dall'uber shader, riduzione delle draw del 28-58%
correggendo il culling, fusione di draw e cache dello stato nel fork VitaGL.
Le transizioni sono inoltre distribuite su piu' frame e la preparazione
delle texture compresse HD e' spostata fuori dal render thread.
Sono risultati dichiarati dal progetto ACGC, non misure di Strikers o una
garanzia di 60 FPS in qualsiasi scena.

Il [README Vita della v0.5.0](https://github.com/Brendonm17/ACGC-Vita-Port/blob/v0.5.0/vita/README.md)
descrive 52 configurazioni shader specializzate e programmi GXP gia'
compilati. Il [command buffer](https://github.com/Brendonm17/ACGC-Vita-Port/blob/v0.5.0/vita/src/vita_gx_cmdbuf.c)
confronta gruppi di stato e unisce draw adiacenti quando stato, geometria e
barriere lo consentono. Non basta condividere una texture: viewport,
illuminazione, indici e copie EFB devono restare compatibili.

Non possiamo dedurre il margine di Strikers dal solo fatto che entrambi
derivano da giochi GameCube. Servono il costo della scena effettiva e il
lavoro residuo della nostra implementazione. Neppure i risultati di MH3
o RE4 si trasferiscono numericamente a questo gioco.

## Cosa e' gia' presente in Strikers

Il percorso attuale e' **VitaGL standalone**. I vecchi report sul ponte
Aurora/GX descrivono un'architettura storica e non sono la base di questa
analisi. Sono gia' presenti preparazione degli asset sul PC, geometria
immutabile residente, GPU skinning, cache dello stato e 41 programmi
materiale di riferimento specializzati per maschera/alpha test.
I sei campionamenti nel fragment shader generale non implicano sei
campionamenti eseguiti da ogni materiale: le specializzazioni consentono
gia' al compilatore di eliminare quelli inutilizzati.

Resta lavoro per pacchetto nella
[preparazione della geometria](smstrikers-port/src/platform/native_match_geometry.cpp),
nella [conversione dei vertici dinamici](smstrikers-port/include/port/native_model_vertices.hpp),
nella [coda di comandi](smstrikers-port/src/platform/deferred_renderer.cpp)
e nel [renderer](smstrikers-port/src/platform/vitagl_renderer.cpp).
I percorsi dinamici possono costruire vettori temporanei, rimappare indici,
copiare payload nella coda e caricare dati. Questi passaggi sono candidati
concreti; la loro presenza nel codice non ne dimostra il peso dominante.
La coda preserva gia' l'ordine, ma non effettua una fusione generale delle draw.

## Prossimi interventi, in ordine

1. **Attribuire il tempo e censire le draw.** Collegare tempi di simulazione,
   preparazione dei pacchetti, attese della coda e invio renderer con numero
   di draw, vertici e byte dinamici per categoria. I timer delle chiamate
   draw sono tempo CPU; le attese GPU e i timer annidati richiedono una
   lettura separata. Misurare gli FPS in run quieti distinti dalla diagnostica.
2. **Eliminare ricostruzioni e copie ricorrenti.** Separare mesh/materiali
   immutabili da pose e dati variabili; riusare scratch e risultati preparati
   con invalidazione esplicita. La palette corrente delle ossa e i dati
   animati non possono diventare cache immutabili. Ridurre il formato
   dinamico solo per famiglie che non consumano tutti gli attributi.
3. **Ridurre le draw realmente eseguite.** Prima censire coppie adiacenti
   compatibili, poi intervenire su HUD, particelle o gruppi ripetuti che
   condividono tutto lo stato necessario. Preservare ordine, scissor,
   blend/depth, palette, texture e copie framebuffer. Valutare culling di
   oggetti fuori vista solo con bounds e passaggi ombre/riflessi corretti.
4. **Ridurre il costo GPU misurato.** Specializzare i materiali dominanti,
   provare precisione ridotta nei termini adatti e trasferire lavoro dal
   fragment al vertex shader entro un errore verificato. Se il limite e'
   la copertura dei pixel, misurare un esperimento isolato sulla risoluzione
   interna conservando HUD e rapporto d'aspetto; non promuoverlo senza
   confronto visivo e senza dimostrarne il beneficio.

La priorita' fra 2, 3 e 4 deve seguire i risultati di 1. Nessuna somma di
stime o riduzione teorica dei cicli shader costituisce una previsione di FPS.

## Shader: esperimenti disponibili e soglia di qualita'

Due opzioni indipendenti, lette all'avvio, restano disabilitate per default:

```ini
vitagl_affine_uv = 0
vitagl_vertex_fog = 0
```

- `vitagl_affine_uv=1`: elimina le divisioni proiettive solo quando tutti i
  canali usati hanno Q esattamente +1, considerando gli override correnti.
  Le UV proiettive e i casi non validi mantengono il riferimento. La variante
  prepara 81 programmi totali invece di 41; il costo di avvio e' distinto
  dal frame time in partita.
- `vitagl_vertex_fog=1`: sposta ai vertici soltanto la trasformazione affine
  della distanza fog. Clamp e curve rimangono nel fragment; intervalli
  degeneri, non finiti o numericamente sfavorevoli tornano al riferimento.
  Movie e copie framebuffer conservano i programmi originali.

Sono accettabili ulteriori approssimazioni a basso errore, inclusa fog o
illuminazione per vertice, ma clamp e funzioni non lineari non commutano
in generale con l'interpolazione. Prima soglia operativa per il confronto
RGB allineato: massimo 2/255 per canale e almeno il 99,9% entro 1/255,
con ispezione di volti, riflessi e bordi. Alpha test, silhouette, copertura
delle ombre, ordine e HUD richiedono verifiche dedicate.

La compilazione degli 81 programmi e' stata provata sulla Vita. Nel confronto
di snapshot l'area campo/giocatori/HUD era entro 2/255, ma la fascia degli
spettatori aveva differenze ampie con fase di animazione non registrata.
La qualifica visiva dell'intero frame rimane aperta; nessuna variante viene
abilitata automaticamente.

## Riferimenti permanenti

- [RE4, revisione studiata 90d97928](https://github.com/Rinnegatamante/re4/tree/90d979284eb8389af28ed06dfe29c89005c360f3):
  specializzazioni e ammissione stretta dei percorsi rapidi.
- [MH3, indice delle ottimizzazioni, revisione 2da6bfb8](https://github.com/Grarak/mh3-wii/blob/2da6bfb85a82d440c55a4c978a4a65e86355b0cb/docs/optimizations/README.md):
  preparazione nativa, riduzione del lavoro per draw e misure.
- [MH3, specializzazioni shader](https://github.com/Grarak/mh3-wii/blob/2da6bfb85a82d440c55a4c978a4a65e86355b0cb/docs/optimizations/rendering/shader-specializations.md):
  semplificazioni del programma, precisione e ripartizione fra stadi.
