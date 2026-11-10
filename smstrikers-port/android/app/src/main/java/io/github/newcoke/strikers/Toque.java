package io.github.newcoke.strikers;

/**
 * A ponte entre os botões desenhados na tela e o controle que o jogo enxerga.
 *
 * <p>Do outro lado fica src/platform/touch.cpp, que guarda este estado e o
 * entrega a um joystick virtual do SDL declarado como gamepad. O PAD do Aurora
 * vê um controle comum chegando e o mapeia por onde um controle de verdade
 * passaria — remapeamento, zona morta, ícones de botão e a atribuição de porta
 * continuam funcionando sem saber o que é um dedo.
 *
 * <p>Esta classe não guarda estado: só empacota o que o overlay calculou.
 *
 * <p>POR QUE OS NOMES NÃO PODEM MUDAR SOZINHOS: a máquina virtual acha as
 * funções nativas pelo nome completo
 * (Java_io_github_newcoke_strikers_Toque_enviar). Renomear a classe ou mudá-la
 * de pacote sem mexer no C++ compila dos dois lados e só quebra no primeiro
 * toque, com UnsatisfiedLinkError.
 */
final class Toque {

    private Toque() {}

    // As máscaras são nossas, não de nenhum hardware: o overlay fala GameCube e
    // a tradução para o SDL acontece uma vez só, no touch.cpp.
    static final int A     = 1 << 0;
    static final int B     = 1 << 1;
    static final int X     = 1 << 2;
    static final int Y     = 1 << 3;
    static final int Z     = 1 << 4;
    static final int START = 1 << 5;
    static final int CIMA  = 1 << 6;
    static final int BAIXO = 1 << 7;
    static final int ESQ   = 1 << 8;
    static final int DIR   = 1 << 9;
    // Os que o controle do Xbox tem e o do GameCube nao: desenhados porque o
    // conjunto e o do Xbox, e mapeados para o que existe onde existe algo.
    static final int LB    = 1 << 10;
    static final int L3    = 1 << 11;
    static final int R3    = 1 << 12;
    static final int BACK  = 1 << 13;

    private static native void enviar(int botoes, int gatilhoEsq, int gatilhoDir,
                                      int lx, int ly, int cx, int cy);

    private static native boolean pronto();

    /**
     * Falso enquanto não há para onde mandar o estado.
     *
     * <p>O resultado negativo NÃO fica em cache, e essa é a diferença que
     * importa: o pad virtual só existe depois que o laço do jogo roda pela
     * primeira vez, então perguntar cedo demais responde "não" e guardar esse
     * "não" deixaria os controles mortos para sempre. Só o erro de ligação, que
     * é permanente de verdade, é lembrado.
     */
    private static boolean semPonte;

    static boolean ligada() {
        if (semPonte) return false;
        try {
            return pronto();
        } catch (UnsatisfiedLinkError e) {
            semPonte = true;
            return false;
        }
    }

    static void estado(int botoes, int gatilhoEsq, int gatilhoDir,
                       int lx, int ly, int cx, int cy) {
        if (semPonte) return;
        try {
            enviar(botoes, gatilhoEsq, gatilhoDir, lx, ly, cx, cy);
        } catch (UnsatisfiedLinkError e) {
            semPonte = true;
        }
    }
}
