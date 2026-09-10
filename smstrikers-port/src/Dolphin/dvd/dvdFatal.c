#include "__dvd.h"

static void (*FatalFunc)();

const char* Japanese = "\n\n\n\x83\x47\x83\x89\x81\x5B\x82\xAA\x94\xAD\x90\xB6\x82\xB5\x82\xDC\x82\xB5\x82\xBD\x81\x42\n\n"
                       "\x96\x7B\x91\xCC\x82\xCC\x83\x70\x83\x8F\x81\x5B\x83\x7B\x83\x5E\x83\x93\x82\xF0\x89\x9F\x82\xB5\x82\xC4\x93\x64\x8C\xB9\x82\xF0\x4F\x46\x46\x82\xC9\x82\xB5\x81\x41\n"
                       "\x96\x7B\x91\xCC\x82\xCC\x8E\xE6\x88\xB5\x90\xE0\x96\xBE\x8F\x91\x82\xCC\x8E\x77\x8E\xA6\x82\xC9\x8F\x5D\x82\xC1\x82\xC4\x82\xAD\x82\xBE\x82\xB3\x82\xA2\x81\x42";

const char* English = "\n\n\nAn error has occurred.\n"
                      "Turn the power off and refer to the\n"
                      "Nintendo GameCube Instruction Booklet\n"
                      "for further instructions.";

const char* const Europe[6] = {
    { "\n\n\nAn error has occurred.\n"
      "Turn the power off and refer to the\n"
      "Nintendo GameCube Instruction Booklet\n"
      "for further instructions." },
    { "\n\n\nEin Fehler ist aufgetreten.\n"
      "Bitte schalten Sie den Nintendo GameCube\n"
      "aus und lesen Sie die Bedienungsanleitung,\n"
      "um weitere Informationen zu erhalten." },
    { "\n\n\nUne erreur est survenue.\n"
      "Eteignez la console et r\xE9"
      "f"
      "\xE9rez-vous au\n"
      "manuel d'instructions Nintendo GameCube\n"
      "pour de plus amples informations." },
    { "\n\n\nSe ha producido un error.\n"
      "Apaga la consola y consulta el manual\n"
      "de instrucciones de Nintendo GameCube\n"
      "para obtener m\xE1"
      "s informaci\xF3"
      "n." },
    { "\n\n\nSi \xE8"
      " verificato un errore.\n"
      "Spegni (OFF) e controlla il manuale\n"
      "d'istruzioni del Nintendo GameCube\n"
      "per ulteriori indicazioni." },
    { "\n\n\nEr is een fout opgetreden.\n"
      "Zet de Nintendo GameCube uit en\n"
      "raadpleeg de handleiding van de\n"
      "Nintendo GameCube voor nadere\n"
      "instructies." },
};

static void ShowMessage(void)
{
    const char* message;
    GXColor bg = { 0x00, 0x00, 0x00, 0x00 };
    GXColor fg = { 0xFF, 0xFF, 0xFF, 0x00 };

    if (VIGetTvFormat() == VI_NTSC)
    {
        if (OSGetFontEncode() == OS_FONT_ENCODE_SJIS)
        {
            message = Japanese;
        }
        else
        {
            message = English;
        }
    }
    else
    {
        message = Europe[OSGetLanguage()];
    }

    OSFatal(fg, bg, message);
}

int DVDSetAutoFatalMessaging(BOOL enable)
{
    BOOL enabled;
    int prev;

    enabled = OSDisableInterrupts();

    prev = FatalFunc ? 1 : 0;
    FatalFunc = enable ? ShowMessage : NULL;

    OSRestoreInterrupts(enabled);
    return prev;
}

/**
 * Offset/Address/Size: 0x0 | 0x80248C98 | size: 0x30
 */
void __DVDPrintFatalMessage(void)
{
    if (FatalFunc)
    {
        FatalFunc();
    }
}
