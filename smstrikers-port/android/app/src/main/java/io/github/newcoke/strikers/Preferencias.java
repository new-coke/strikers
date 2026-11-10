package io.github.newcoke.strikers;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * As opções, guardadas entre execuções.
 *
 * <p>Uma camada fina em cima do SharedPreferences, e não o SharedPreferences
 * direto pelo app inteiro, por um motivo prático: assim os nomes das chaves e os
 * valores padrão vivem num lugar só. Chave escrita à mão em dois lugares é um
 * bug silencioso — grava num sítio, lê noutro, e a opção "não funciona".
 *
 * <p>As que o JOGO lê não param aqui: {@link Configuracao} as escreve no
 * strikers.ini, porque o lado nativo não fala SharedPreferences. Estas ficam
 * como a fonte da verdade da interface, e o ini é gerado a partir delas.
 */
final class Preferencias {

    // --- o overlay de toque, que é só nosso -------------------------------
    static final String VIBRAR      = "vibrar";
    static final String DIRECIONAL  = "direcional";   // mostrar o direcional
    static final String CSTICK      = "cstick";       // mostrar o C-stick
    static final String OPACIDADE   = "opacidade";    // 25..100 (%)
    static final String TAMANHO     = "tamanho";      // 75..130 (%)
    static final String ZONA_MORTA  = "zona_morta";   // 0..35  (%)
    static final String HUD         = "hud";          // o painel de desempenho

    // --- as que viram chaves do strikers.ini ------------------------------
    // LINHAS de resolução interna, e 0 para seguir a tela. Chave nova de
    // propósito: a antiga guardava porcentagem, e ler um 150 velho como "150
    // linhas" daria uma imagem de tamanho de selo sem ninguém entender por quê.
    static final String RES_LINHAS  = "res_linhas";
    static final String MSAA        = "msaa";         // 1 ou 4
    static final String ANISO       = "aniso";        // 1, 2, 4, 8, 16
    static final String FPS_LIMIT   = "fps_limit";    // -1 = display, 0 = uncapped
    static final String VSYNC       = "vsync";
    static final String REGIAO      = "regiao";       // "" = do disco
    static final String IDIOMA      = "idioma";       // "" = do disco
    static final String DESBLOQUEAR = "desbloquear";  // unlock_all
    // O que fazer com a arte de menu que foi desenhada para 640 de largura
    // quando a tela é mais larga: 0 mantém a proporção, 1 estica.
    static final String ESTICAR     = "esticar";      // widen
    // A forma do quadro: 0 segue a tela, 1 força 16:9, 2 força 4:3. O 4:3 é o
    // enquadramento do GameCube, com tarjas nos lados — e é a única referência
    // que existe para comparar quando algo parece fora do lugar numa tela larga.
    static final String PROPORCAO   = "proporcao";    // aspect

    // --- o idioma da INTERFACE, que não é o do jogo -----------------------
    // Uma etiqueta BCP-47 ("pt-BR", "ja"), ou "" para seguir o aparelho. Não
    // confundir com IDIOMA acima, que escolhe entre os textos do disco.
    static final String IDIOMA_APP  = "idioma_app";

    // --- driver de vídeo ---------------------------------------------------
    // O nome da pasta em <files>/drivers, ou "" para o driver do sistema.
    static final String DRIVER      = "driver";
    static final String TEXTURE_PACK = "texture_pack";

    private final SharedPreferences p;

    Preferencias(Context ctx) {
        p = ctx.getSharedPreferences("strikers", Context.MODE_PRIVATE);
    }

    boolean ligado(String chave) {
        switch (chave) {
            case VIBRAR:      return p.getBoolean(chave, true);
            case HUD:         return p.getBoolean(chave, false);
            // DESLIGADO por padrão, e isto anda junto com fps_limit = 0: com o
            // vsync ligado a apresentação espera o painel, e aí o limite não
            // importa -- 60 Hz de painel dão 60 quadros por mais alto que o teto
            // esteja. Desbloquear de verdade é desligar os dois.
            case VSYNC:       return p.getBoolean(chave, false);
            case DIRECIONAL:  return p.getBoolean(chave, false);
            case CSTICK:      return p.getBoolean(chave, false);
            case DESBLOQUEAR: return p.getBoolean(chave, false);
            default:          return p.getBoolean(chave, false);
        }
    }

    int numero(String chave) {
        switch (chave) {
            case OPACIDADE:  return p.getInt(chave, 100);
            case TAMANHO:    return p.getInt(chave, 100);
            case ZONA_MORTA: return p.getInt(chave, 8);
            case RES_LINHAS: return p.getInt(chave, 0);
            case MSAA:       return p.getInt(chave, 1);
            case ANISO:      return p.getInt(chave, 16);
            // ESTICAR por padrão, que é como o porte sempre foi. Tentei trocar
            // para "manter" dizendo que era isso que tirava os riscos do fundo
            // de baixo das linhas do menu; medido no aparelho, não era. Ver a
            // nota em PortWidenStretch, src/platform/aspect.c.
            case ESTICAR:    return p.getInt(chave, 1);
            // A proporção: 0 segue a tela, 1 é 16:9, 2 é 4:3.
            case PROPORCAO:  return p.getInt(chave, 0);
            // Sem teto. A simulação do jogo continua andando a 60 por dentro;
            // o que sai daqui é quantas vezes a imagem é desenhada.
            case FPS_LIMIT:  return p.getInt(chave, -1);
            default:         return p.getInt(chave, 0);
        }
    }

    void ligar(String chave, boolean valor) {
        p.edit().putBoolean(chave, valor).apply();
    }

    void definir(String chave, int valor) {
        p.edit().putInt(chave, valor).apply();
    }

    String texto(String chave) {
        return p.getString(chave, "");
    }

    void definirTexto(String chave, String valor) {
        p.edit().putString(chave, valor).apply();
    }
}
