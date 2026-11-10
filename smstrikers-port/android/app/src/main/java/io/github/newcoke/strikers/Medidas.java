package io.github.newcoke.strikers;

/**
 * As medidas de desempenho vindas do jogo.
 *
 * <p>Do outro lado fica src/platform/hud_bridge.cpp. O FPS daqui é o do JOGO e
 * não o da tela: o Android sabe dizer quantas vezes o painel atualizou, e esse
 * número marca 120 num aparelho de 120 Hz com o jogo travado numa imagem. O que
 * atravessa aqui é o que o limitador do porte mediu.
 *
 * <p>A ORDEM DO VETOR É CONTRATO com o C++. Trocar um índice aqui não dá erro de
 * compilação nem de execução: só troca os rótulos dos números, e o painel
 * continua parecendo certo mostrando outra coisa.
 */
final class Medidas {

    private Medidas() {}

    static final int FPS_CENTESIMOS = 0;
    static final int QUADRO_US      = 1;
    static final int OCUPADO_US     = 2;
    static final int OCUPADO_P95_US = 3;
    static final int PIOR_US        = 4;
    static final int QUADROS        = 5;
    static final int EM_PARTIDA     = 6;
    static final int QUANTIDADE     = 7;

    private static native boolean lerTudo(long[] destino);

    private static native int quantidade();

    private static boolean semPonte;

    /**
     * Lê as medidas do último quadro. Falso quando não há de onde ler — e nesse
     * caso o painel escreve "--", que é diferente de escrever zero.
     */
    static boolean ler(long[] destino) {
        if (semPonte) return false;
        try {
            return lerTudo(destino);
        } catch (UnsatisfiedLinkError e) {
            semPonte = true;
            return false;
        }
    }

    /**
     * Confere que os dois lados concordam sobre o tamanho do vetor.
     *
     * <p>Um contrato por índice que ninguém verifica é um contrato que se quebra
     * em silêncio. Isto não pega uma troca de ordem, mas pega o caso comum: um
     * campo acrescentado de um lado só.
     */
    static boolean contratoConfere() {
        if (semPonte) return false;
        try {
            return quantidade() == QUANTIDADE;
        } catch (UnsatisfiedLinkError e) {
            semPonte = true;
            return false;
        }
    }
}
