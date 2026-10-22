# Strikers Vita: candidata locale per i tester

Consegnare privatamente `strikers_vita.vpk`, `strikers.ini`, questo documento
e `release-candidate.json` dalla directory `smstrikers-port/build-vita-gxm-latest`.
Il VPK resta locale: non allegarlo a release, commit o altri upload su GitHub.
ISO e PSARC non fanno parte della consegna; ogni tester prepara il proprio
archivio seguendo [ASSET_SETUP.md](https://github.com/robin994/strikersVita/blob/main/ASSET_SETUP.md) nella repository.

Installare il VPK completo con VitaShell e copiare l'INI fornito in
`ux0:data/strikersVita/strikers.ini`, conservando prima le impostazioni personali.
Il PSARC della nuova pipeline deve essere in `ux0:data/strikersVita/sms.psarc`.
Prima di aggiornare salvare sul PC anche `cardA/` e `cardB/` dalla stessa directory.

Il profilo usa il flusso normale del gioco, glifi PlayStation attivi, 444 MHz
CPU / 222 MHz GPU, completamento dei frame sincrono e nessun render skipping.
Diagnostiche, overlay FPS, profiler, capture, watchdog e probe sono disabilitati.
Per tornare ai glifi GameCube impostare `vita_playstation_glyphs = 0` e riavviare.

Provare avvio e caricamento del salvataggio, menu e comandi, una partita,
ingresso in campo, gol/replay, premiazioni e filmati, inclusi salto e ritorno
al menu. Segnalare scena e passaggi per riprodurre il problema, qualità di
video/audio e sincronizzazione, modello Vita e firmware. Allegare il manifest;
in caso di crash conservare anche il coredump. Non attivare profiling durante
una prova di fluidità del profilo normale.

Questa è una candidata funzionale. **30 FPS stabili e 60 FPS non sono certificati.**
Le prove precedenti sulla base C9 hanno mostrato un ingresso sotto i 30 FPS
e un filmato completato con durata corretta ma con fotogrammi mancanti.
Questi risultati non misurano la candidata generata dal nuovo commit.

Per ricrearla da sorgenti puliti, con VitaSDK predisposto:

```sh
smstrikers-port/build-vita-vitagl.sh --release-candidate
```

Il comando riutilizza l'unica directory di build Vita, azzera la cache CMake
degli esperimenti e produce il manifest con commit, configurazione e SHA-256.
