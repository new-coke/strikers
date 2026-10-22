# Profilazione dei pacchetti nella finestra di gioco

Questa raccolta serve ad attribuire il costo CPU del renderer. E' separata
dalle misure degli FPS quieti e si abilita soltanto tramite INI.

```ini
diagnostics = 0
fps_overlay = 0
vita_test_match = 1
vita_frameskip = 0
fixed_dt = 0
seed = 0x53545249
vitagl_affine_uv = 0
vitagl_vertex_fog = 0
vitagl_timing = 0
vitagl_packet_profile = 1
frame_capture = ux0:data/strikersVita/census-profile-1.csv
frame_capture_frames = 300
frame_capture_match_only = 1
frame_capture_play_only = 1
frame_capture_skip = 600
```

Riavviare l'app dopo la modifica. Conservare prima l'INI normale e ripristinarlo
al termine. Il nome di cattura deve essere nuovo per ogni run; registrare
hash dell'INI effettivo e di `ux0:app/SMSVITA01/eboot.bin`.

`vitagl_packet_profile=1` abilita anche i timer del thread proprietario di
VitaGL, indipendentemente da `vitagl_timing`. Non serve attivare la diagnostica
generale, che aggiungerebbe altri dump e timer. Con l'opzione spenta, gli scope
dei pacchetti non interrogano il clock e non allocano memoria di raccolta.

Il benchmark decide quali frame selezionare. La raccolta dei pacchetti
conserva soltanto gli stessi campioni, con identita' del frame live assegnata
dal benchmark: warm-up, intro e replay non entrano negli aggregati. Lo spazio
per i campioni e' riservato prima della raccolta, con limite di 8.192 frame.
I nuovi report vengono scritti al termine della finestra, dopo la misura del
frame finale; i report cumulativi periodici del runtime sono sospesi durante
questa modalita'. Un errore di output e' riportato su stderr e i campioni
restano in memoria, senza un ciclo automatico di riscrittura durante il gioco.

Per l'esempio sopra, recuperare tutti e tre i file dalla Vita:

```text
census-profile-1.csv
census-profile-1.csv.packets.csv
census-profile-1.csv.views.csv
```

Anche `vita_workflow_device.py fetch` recupera questi nomi specifici del run.
Analisi sul PC:

```sh
python3 smstrikers-port/tools/analyze_packet_profile.py \
  /percorso/census-profile-1.csv --expected 300 \
  --out /percorso/census-profile-1-summary.json
```

L'analizzatore rifiuta campioni incompleti, frame non allineati e aggregati
per vista che non coincidono con quelli per frame. Il test host usa l'output
del vero esportatore C++, insieme a casi di identita' e conteggi non validi.

## Lettura dei risultati

Le fasi partizionano il tempo di parete di `on_packet`:

| Fase | Lavoro incluso |
| --- | --- |
| `invalidation` | Consumo delle notifiche di scrittura all'ingresso del pacchetto |
| `arrays` | Risoluzione e validazione di stream, indici e identita' della geometria |
| `skin` | Validazione e preparazione della palette corrente |
| `material` | Matrici, raster, texture, luci e stato del materiale |
| `geometry` | Lookup residente, conversione dei vertici e creazione del modello su miss |
| `submit` | Costruzione/invio del comando posseduto, incluse eventuali attese |
| `cleanup` | Contatori e distruzione dei temporanei del callback |

I conteggi distinguono hit residenti, modelli residenti appena costruiti,
draw dinamiche, rifiuti, riferimenti in ingresso, vertici/indici convertiti
e byte dinamici. La coda registra anche byte realmente copiati e tempo delle
copie. Il percorso raro che prende in prestito un payload sovradimensionato
in modo sincrono non viene contato come una copia posseduta.

**Non sommare le attese ai tempi dei pacchetti.** `packet_wait_ns` e' gia'
incluso nelle fasi che bloccano. Le copie sono gia' incluse nell'invio.
`frontend_wait_us` include anche le attese fuori dai callback, per esempio
la barriera a fine frame. I timer `owner_draw_us` e `owner_present_us`
misurano chiamate CPU sul thread VitaGL, che puo' lavorare in parallelo
al produttore; non sono timestamp GPU.

`tasks_outside_packet_scopes_ms_per_frame` e' la differenza fra il tempo
dei task e gli scope dei pacchetti, non una misura esclusiva della simulazione.
Comprende altro lavoro del motore, callback non censiti e costi di inizio
frame, inclusa l'invalidazione eseguita prima dei pacchetti. Gli scope
includono il costo dei clock diagnostici al loro interno.

Per valutare un'ottimizzazione ripetere run quieti con
`vitagl_packet_profile=0`, `vitagl_timing=0`, diagnostica e overlay spenti,
stessi clock effettivi, scenario e finestra. Il frame rate di un run
strumentato serve a quantificare la perturbazione e non dimostra un guadagno.
