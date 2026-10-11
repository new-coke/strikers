# GX decoder: installazione e misura su PS Vita, 2026-10-11

La build con snapshot GX ridotti è stata installata e misurata sulla Vita.
Nel confronto effettuato la media passa da **18,09 a 18,76 FPS (+3,70%)**,
mentre le latenze del 95° e 99° percentile peggiorano. Questo singolo confronto
non dimostra un guadagno stabile e non soddisfa l'obiettivo di 60 FPS.

## Risultati hardware

| Misura | Controllo | Decoder nuovo |
| --- | ---: | ---: |
| Fotogrammi di partita effettiva | 300 | 300 |
| Intervallo contatore live-play | 600–899 | 600–899 |
| FPS da tempo totale trascorso | 18,0878 | 18,7571 |
| Tempo medio per fotogramma | 55,286 ms | 53,313 ms |
| Mediana | 53,948 ms | 50,691 ms |
| 95° percentile | 68,798 ms | 70,947 ms |
| 99° percentile | 88,742 ms | 93,255 ms |
| Mediana fase tasks | 53,622 ms | 50,348 ms |
| Mediana fase present | 0,331 ms | 0,328 ms |
| Fotogrammi entro 16,667 ms | 0% | 0% |
| Fotogrammi entro 33,333 ms | 0% | 0% |

La riduzione media è di 1,973 ms, pari al 3,57%. Il 95° percentile aumenta
del 3,12% e il 99° del 5,09%. Sono due catture singole: il tempo reale influenza
l'evoluzione della partita, quindi lo stesso intervallo di fotogrammi non
garantisce scene identiche. Non classificare la patch come miglioramento
prestazionale stabile senza ripetizioni alternate e un controllo delle code.

Entrambe le catture usano GXM a 960×544, tempo reale (`fixed_dt=0`), nessun
frameskip, diagnostica, overlay e profiler disattivati, seed `0x53545249`,
cache geometrica statica da 8 MiB e le stesse impostazioni native del controllo.
La partita automatica è selezionata solo tramite `strikers.ini`.
Le impostazioni di lancio sono identiche byte per byte.

Le frequenze **misurate** sono CPU 444 MHz, GPU 222 MHz, bus 222 MHz e crossbar
166 MHz per entrambe le build. Il valore CPU richiesto nell'INI è 500 MHz;
non descrivere queste catture come misurate a 500 MHz.

Il tempo di `present` è basso, ma `tasks_us` comprende anche il lavoro di
rendering e le possibili attese. Questi contatori non separano CPU attiva,
decoder GX, invio GXM e tempo GPU. Non bastano per attribuire tutto il costo
alla CPU o per escludere attese GPU. Per arrivare a 60 FPS partendo dalla
media della candidata serve ancora una riduzione di circa **68,74%** del
tempo per fotogramma.

## Identità della build e validazione

| Artefatto | SHA-256 |
| --- | --- |
| SELF controllo installato | `44052765ae221aef45a890a8ec49c254dc2cc756d53eadb887ac5d81942c7a9d` |
| SELF candidata installata | `1200ffa8d3a29f988c4636dfb1724d9fcbe09e89e1346ae294ef5bcc2ff571a1` |
| VPK candidata | `8bdc0a71667ba3ed1c902d2fd423c7913cb850bfb62543a0809cd7c891336ab8` |
| INI di lancio comune | `f62c92c416f6226ed7ad2f79eb58cec1b5065e9f8136349b7ab0db7765bece1d` |
| INI durante entrambe le catture | `2729543914379188dfea4cdc66a07e492759ae6e8ccb1bff642c087fb3c8fd8e` |
| INI originale ripristinato | `e903fbc96bcff24029076f0ba6e6ce7e28ac088bc437cfe9fb646237abd99493` |

Il runtime aggiunge una sola riga vuota prima del blocco delle impostazioni
gestite; questa differenza è stata verificata esattamente. La candidata
installata è stata riletta dopo l'aggiornamento e dopo la cattura. Il VPK
contiene lo stesso SELF, ma l'installazione completata usa la sostituzione
diretta del solo `eboot.bin`. I parametri di build semantici coincidono con
quelli del controllo, incluso `STRIKERS_VITA_GXM_LEAN_RENDER=OFF`.

La patch riduce le copie temporanee dello stato nel decoder; non introduce
un bypass completo di modelli/materiali GX. Nel sorgente gli snapshot TEV
color/alpha/order passano rispettivamente da 132/132/264 a 36/44/24 byte;
gli indici delle matrici riutilizzano i selettori già salvati. I test host
precedenti hanno verificato 37/37 casi e 130.202 controlli frontend senza
errori, anche con ASan/UBSan. Il digest di equivalenza resta
`ab552d3a579e1db2`.

L'audit del binario trova draw/present GXM ed esclude punti di ingresso
GL/vgl/vita2d. Le catture sono complete e non risultano nuovi coredump.
Questo non certifica grafica, input, audio o stabilità prolungata della
candidata: non sono state ottenute nuove verifiche visive per questo SELF.

## Installazione, pulizia e configurazione

La chiusura per Title ID continua a fallire su Vita Companion 1.06. Dopo
l'autorizzazione esplicita dell'utente, `destroy` ha confermato la chiusura
globale delle applicazioni e la scrittura dell'eseguibile è riuscita.
Non abilitare la chiusura globale implicitamente nei futuri strumenti.

Lo strumento `vita_workflow_device.py` ora conserva i backup sul computer,
aggiorna il file esistente, verifica la rilettura e tenta il ripristino
verificato se il trasferimento o la rilettura falliscono. Quattro test
verificano aggiornamento, trasferimento parziale, rilettura errata e tutela
di un backup locale precedente. La sostituzione diretta non è atomica:
se la connessione cade anche durante il ripristino, resta necessario
recuperare il file dalla copia sul computer.

Per questo confronto è stato riutilizzato un solo percorso temporaneo,
`ux0:data/strikersVita/perf-test.csv`. Il controllo è stato scaricato e
rimosso prima del lancio della candidata. La pulizia dei backup precedenti
avviene **dopo entrambe le misure**: il suo effetto sugli FPS non è misurato.
Sono stati eliminati 53 vecchi file di backup/staging (186.641.749 byte,
circa 178 MiB), tutti archiviati e verificati sul computer prima della
cancellazione. Sono rimasti solo `eboot.bin` e `sce_sys` nella cartella
dell'app e gli 8 elementi originali nella cartella dati. Anche il CSV
temporaneo della candidata è stato rimosso. La candidata resta installata,
l'INI originale è ripristinato byte per byte e il gioco è chiuso.
Lo stato finale è registrato nel
[manifest di pulizia](evidence/GX_DECODE_SNAPSHOTS_2026-10-11/hardware/final-device-cleanup.json).

## Prove riproducibili

- [Confronto, identità, variazioni percentuali e limiti](evidence/GX_DECODE_SNAPSHOTS_2026-10-11/hardware/comparison.json).
- [CSV controllo](evidence/GX_DECODE_SNAPSHOTS_2026-10-11/hardware/control.csv) e [CSV candidata](evidence/GX_DECODE_SNAPSHOTS_2026-10-11/hardware/candidate.csv).
- [Impostazioni controllo](evidence/GX_DECODE_SNAPSHOTS_2026-10-11/hardware/control-run.json) e [impostazioni candidata](evidence/GX_DECODE_SNAPSHOTS_2026-10-11/hardware/candidate-run.json).
- [Identità della compilazione](evidence/GX_DECODE_SNAPSHOTS_2026-10-11/hardware/build-identity.json), [installazione verificata](evidence/GX_DECODE_SNAPSHOTS_2026-10-11/hardware/install-identity.json) e [ripristino configurazione](evidence/GX_DECODE_SNAPSHOTS_2026-10-11/hardware/restore-config-identity.json).
- [Validazione sorgente e host precedente](evidence/GX_DECODE_SNAPSHOTS_2026-10-11/validation.json): descrive lo stato prima della compilazione/installazione; questo report aggiunge la prova hardware successiva.

I binari completi, le copie dei vecchi backup e i registri di lavoro restano
in `ab-artifacts/decoder-install-20261011-8965defa/` sul computer.

Il risultato giustifica solo una riduzione locale del lavoro nel decoder.
Il prossimo intervento per l'obiettivo di 60 FPS deve identificare e
ridurre una parte sostanziale dei circa 53 ms per fotogramma,
misurando separatamente preparazione delle geometrie, trasformazioni,
decoder, stato/materiali, invii GXM e attese. Un percorso nativo completo
deve mantenere l'ordine dei draw e le dipendenze EFB, verificare hit/fallback
e numero di draw, e non aggiungere draw tramite la cache.
