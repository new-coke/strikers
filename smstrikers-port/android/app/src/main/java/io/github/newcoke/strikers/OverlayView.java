package io.github.newcoke.strikers;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;

import java.util.ArrayList;
import java.util.List;

/**
 * O overlay de toque, desenhado à mão.
 *
 * <p>POR QUE DESENHADO E NÃO EXTRAÍDO: os controles são círculos e pastilhas
 * translúcidos com um rótulo por cima — geometria, não arte. Desenhá-los aqui dá
 * o mesmo aspecto, fica vetorial (nítido em qualquer densidade de tela) e evita
 * empacotar os arquivos de outro aplicativo dentro deste APK.
 *
 * <p>O CONJUNTO É O DO XBOX 360 — LT/LB/L3 e RT/RB/R3 empilhados nos cantos de
 * cima, A B X Y em losango, dois manches, BACK e START. O jogo é de GameCube, e
 * a tradução acontece do outro lado da ponte, em touch.cpp: LT vira o L, RT vira
 * o R, LB e RB viram o Z. L3, R3 e BACK não têm equivalente no controle do
 * console e não fazem nada aqui — dito no menu, que é melhor do que um botão que
 * some ou um que engana.
 */
public class OverlayView extends View {

    /** O que um controle FAZ quando é tocado. */
    private enum Papel { BOTAO, GATILHO_ESQ, GATILHO_DIR, ANALOGICO, C_STICK, MENU }

    private enum Forma { CIRCULO, RECTANGULO, ANALOGICO }

    /** Um controle: forma, rótulo, papel e onde está. */
    private static final class Controlo {
        final String rotulo;
        final Forma forma;
        Papel papel = Papel.BOTAO;
        int mascara;            // bit do Toque, para os de papel BOTAO
        float cx, cy, raio;     // círculo
        final RectF caixa = new RectF();   // retângulo
        int dedo = -1;          // id do toque que o está segurando, ou -1
        float carga;            // 0..1, para a animação de pressão

        Controlo(String rotulo, Forma forma) {
            this.rotulo = rotulo;
            this.forma = forma;
        }

        boolean contem(float x, float y) {
            if (forma == Forma.CIRCULO || forma == Forma.ANALOGICO) {
                final float dx = x - cx, dy = y - cy;
                // Uma margem generosa: o dedo é gordo, e errar um botão numa
                // área de toque irrita muito mais do que encostar de raspão no
                // vizinho.
                final float alcance = raio * 1.25f;
                return dx * dx + dy * dy <= alcance * alcance;
            }
            return caixa.contains(x, y);
        }
    }

    private final List<Controlo> controlos = new ArrayList<>();
    private final Paint tinta = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint texto = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path caminho = new Path();

    private float mancheDx, mancheDy, cDx, cDy;
    private Controlo manche, mancheC;

    // Ajustes, lidos das preferências. Ver recarregar().
    private float opacidade = 1f;
    private float escala = 1f;
    private float zonaMorta = 0.08f;
    private boolean mostrarDirecional;
    private boolean mostrarCStick;

    // As barras do sistema, medidas e não adivinhadas. Sem isto o botão da
    // direita fica debaixo da barra de navegação, e o toque vai para o sistema
    // em vez de ir para o jogo.
    private int recuoEsq, recuoDir, recuoCima, recuoBaixo;

    /** Chamado quando o botão de ajustes é solto. */
    interface AoPedirMenu { void pediu(); }
    private AoPedirMenu aoPedirMenu;

    public OverlayView(Context ctx) {
        super(ctx);
        setFocusable(false);
        texto.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        texto.setTextAlign(Paint.Align.CENTER);
    }

    void aoPedirMenu(AoPedirMenu o) { aoPedirMenu = o; }

    /** Relê os ajustes e refaz o traçado. */
    void recarregar(Preferencias prefs) {
        opacidade = prefs.numero(Preferencias.OPACIDADE) / 100f;
        escala = prefs.numero(Preferencias.TAMANHO) / 100f;
        zonaMorta = prefs.numero(Preferencias.ZONA_MORTA) / 100f;
        mostrarDirecional = prefs.ligado(Preferencias.DIRECIONAL);
        mostrarCStick = prefs.ligado(Preferencias.CSTICK);
        if (getWidth() > 0) {
            montar(getWidth(), getHeight());
        }
        invalidate();
    }

    @Override
    public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            android.graphics.Insets b = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            recuoEsq = b.left; recuoDir = b.right; recuoCima = b.top; recuoBaixo = b.bottom;
        } else {
            recuoEsq = insets.getSystemWindowInsetLeft();
            recuoDir = insets.getSystemWindowInsetRight();
            recuoCima = insets.getSystemWindowInsetTop();
            recuoBaixo = insets.getSystemWindowInsetBottom();
        }
        if (getWidth() > 0) {
            montar(getWidth(), getHeight());
            invalidate();
        }
        return insets;
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        montar(w, h);
    }

    // -----------------------------------------------------------------------
    // Traçado
    //
    // Tudo em fração da tela e não em dp: o alvo é um celular em paisagem, e o
    // que precisa ficar constante é a posição relativa aos cantos — é lá que o
    // polegar alcança.
    // -----------------------------------------------------------------------
    private void montar(int w, int h) {
        controlos.clear();
        manche = mancheC = null;
        if (w == 0 || h == 0) return;

        final float u = Math.min(w, h);          // unidade: o lado menor
        final float r = u * 0.062f * escala;     // raio dos botões de face
        final float margem = u * 0.055f;

        // As bordas úteis, já descontadas as barras do sistema.
        final float esq = margem + recuoEsq;
        final float dir = w - margem - recuoDir;
        final float cima = margem + recuoCima;
        final float baixo = h - margem - recuoBaixo;

        // --- os gatilhos e os cliques dos manches, nos dois cantos de cima --
        //
        // Tres pastilhas de cada lado, que e o conjunto do Xbox 360. Nem todas
        // tem para onde ir num jogo de GameCube, e isso esta dito no menu em vez
        // de resolvido escondendo botao: LT vira o L, RT vira o R, LB e RB viram
        // o Z (ha um Z so, e nao ha porque o jogador descobrir qual ombro e o de
        // verdade), e L3, R3 e BACK nao tem equivalente no controle do console.
        final float lg = u * 0.155f * escala, ag = u * 0.085f * escala;

        float y = cima;
        final Papel[] papelEsq = {Papel.GATILHO_ESQ, Papel.BOTAO, Papel.BOTAO};
        final int[] mascaraEsq = {0, Toque.LB, Toque.L3};
        final String[] nomesEsq = {"LT", "LB", "L3"};
        for (int i = 0; i < 3; i++) {
            Controlo c = new Controlo(nomesEsq[i], Forma.RECTANGULO);
            c.papel = papelEsq[i];
            c.mascara = mascaraEsq[i];
            c.caixa.set(esq, y, esq + lg, y + ag);
            controlos.add(c);
            y += ag * 1.18f;
        }

        y = cima;
        final Papel[] papelDir = {Papel.GATILHO_DIR, Papel.BOTAO, Papel.BOTAO};
        final int[] mascaraDir = {0, Toque.Z, Toque.R3};
        final String[] nomesDir = {"RT", "RB", "R3"};
        for (int i = 0; i < 3; i++) {
            Controlo c = new Controlo(nomesDir[i], Forma.RECTANGULO);
            c.papel = papelDir[i];
            c.mascara = mascaraDir[i];
            c.caixa.set(dir - lg, y, dir, y + ag);
            controlos.add(c);
            y += ag * 1.18f;
        }

        // --- A B X Y em losango, no desenho do Xbox: Y em cima, X a esquerda,
        //     B a direita, A embaixo ------------------------------------------
        final float fx = dir - r * 2.4f;
        final float fy = baixo - r * 2.4f;
        final float d = r * 1.55f;
        controlos.add(face("Y", fx, fy - d, r, Toque.Y));
        controlos.add(face("X", fx - d, fy, r, Toque.X));
        controlos.add(face("B", fx + d, fy, r, Toque.B));
        controlos.add(face("A", fx, fy + d, r, Toque.A));

        // --- os dois manches -------------------------------------------------
        final float rm = u * 0.115f * escala;
        manche = new Controlo("", Forma.ANALOGICO);
        manche.papel = Papel.ANALOGICO;
        manche.cx = esq + rm * 1.15f;
        manche.cy = baixo - rm * 1.25f;
        manche.raio = rm;
        controlos.add(manche);

        // O da direita e o manche C do GameCube. Ligavel e nao obrigatorio
        // porque Strikers quase nao o usa e um segundo anel rouba tela.
        if (mostrarCStick) {
            mancheC = new Controlo("", Forma.ANALOGICO);
            mancheC.papel = Papel.C_STICK;
            mancheC.cx = w * 0.665f;
            mancheC.cy = baixo - rm * 1.55f;
            mancheC.raio = rm * 0.86f;
            controlos.add(mancheC);
        }

        // --- O DIRECIONAL, opcional ------------------------------------------
        if (mostrarDirecional) {
            final float rd = u * 0.045f * escala;
            final float dcx = esq + rm * 1.15f;
            final float dcy = h * 0.50f;
            final float passo = rd * 1.95f;
            controlos.add(face("▲", dcx, dcy - passo, rd, Toque.CIMA));
            controlos.add(face("▼", dcx, dcy + passo, rd, Toque.BAIXO));
            controlos.add(face("◀", dcx - passo, dcy, rd, Toque.ESQ));
            controlos.add(face("▶", dcx + passo, dcy, rd, Toque.DIR));
        }

        // --- a fila do centro, embaixo: BACK, ajustes, START ------------------
        final float lp = u * 0.115f * escala, ap = u * 0.052f * escala;
        final float rg = u * 0.036f * escala;
        final float folga = u * 0.028f;

        Controlo back = new Controlo("BACK", Forma.RECTANGULO);
        back.mascara = Toque.BACK;
        back.caixa.set(w * 0.5f - rg - folga - lp, baixo - ap,
                       w * 0.5f - rg - folga, baixo);
        controlos.add(back);

        Controlo comeco = new Controlo("START", Forma.RECTANGULO);
        comeco.mascara = Toque.START;
        comeco.caixa.set(w * 0.5f + rg + folga, baixo - ap,
                         w * 0.5f + rg + folga + lp, baixo);
        controlos.add(comeco);

        Controlo menu = new Controlo("", Forma.CIRCULO);
        menu.papel = Papel.MENU;
        menu.cx = w * 0.5f;
        menu.cy = baixo - ap / 2f;
        menu.raio = rg;
        controlos.add(menu);


    }

    private Controlo face(String rotulo, float cx, float cy, float raio, int mascara) {
        Controlo c = new Controlo(rotulo, Forma.CIRCULO);
        c.cx = cx; c.cy = cy; c.raio = raio; c.mascara = mascara;
        return c;
    }

    // -----------------------------------------------------------------------
    // Desenho
    // -----------------------------------------------------------------------
    @Override
    protected void onDraw(Canvas canvas) {
        for (Controlo c : controlos) {
            // A pressão se aproxima do alvo em vez de saltar: é a diferença
            // entre parecer um botão e parecer uma luz piscando.
            final float alvo = c.dedo >= 0 ? 1f : 0f;
            c.carga += (alvo - c.carga) * 0.35f;
            if (Math.abs(alvo - c.carga) > 0.003f) postInvalidateOnAnimation();
            else c.carga = alvo;

            switch (c.forma) {
                case RECTANGULO: desenharRectangulo(canvas, c); break;
                case CIRCULO:    desenharCirculo(canvas, c);    break;
                case ANALOGICO:  desenharAnalogico(canvas, c);  break;
            }
        }
    }

    /** Aplica a opacidade escolhida nos ajustes a um alfa de 0..255. */
    private int alfa(float valor) {
        return Math.max(0, Math.min(255, Math.round(valor * opacidade)));
    }

    private void paintControl(Canvas canvas, Path shape, float pressure) {
        tinta.setColor(Color.BLACK);
        tinta.setAlpha(alfa(105));
        canvas.drawPath(shape, tinta);
        tinta.setColor(Color.WHITE);
        tinta.setAlpha(alfa(38 + 52 * pressure));
        canvas.drawPath(shape, tinta);

        final float d = Math.max(1f, getResources().getDisplayMetrics().density);
        tinta.setStyle(Paint.Style.STROKE);
        tinta.setStrokeWidth(d * 2f);
        tinta.setColor(Color.BLACK);
        tinta.setAlpha(alfa(46));
        canvas.drawPath(shape, tinta);
        tinta.setStrokeWidth(d);
        tinta.setColor(Color.WHITE);
        tinta.setAlpha(alfa(110 + 90 * pressure));
        canvas.drawPath(shape, tinta);
        tinta.setStyle(Paint.Style.FILL);
    }

    private void desenharRectangulo(Canvas canvas, Controlo c) {
        final float raio = c.caixa.height() * 0.42f;
        caminho.reset();
        caminho.addRoundRect(c.caixa, raio, raio, Path.Direction.CW);
        paintControl(canvas, caminho, c.carga);
        // Os rótulos longos (START) pedem letra menor do que L ou Z, ou
        // transbordam a pastilha.
        final float tamanho = c.caixa.height() * (c.rotulo.length() > 2 ? 0.30f : 0.42f);
        escrever(canvas, c.rotulo, c.caixa.centerX(), c.caixa.centerY(), tamanho, c.carga);
    }

    private void desenharCirculo(Canvas canvas, Controlo c) {
        caminho.reset();
        caminho.addCircle(c.cx, c.cy, c.raio, Path.Direction.CW);
        paintControl(canvas, caminho, c.carga);
        if (c.papel == Papel.MENU) {
            desenharIconeAjustes(canvas, c);
        } else {
            escrever(canvas, c.rotulo, c.cx, c.cy, c.raio * 0.78f, c.carga);
        }
    }

    /**
     * O ícone de ajustes: três riscos com um botão em cada, em alturas
     * diferentes.
     *
     * <p>Não é uma engrenagem de propósito — uma engrenagem desenhada pequena
     * vira uma mancha com serrilha, e a esta escala (poucos milímetros) o ícone
     * de réguas continua legível porque é feito só de linhas retas.
     */
    private void desenharIconeAjustes(Canvas canvas, Controlo c) {
        final float lado = c.raio * 0.98f;
        final float d = Math.max(1f, getResources().getDisplayMetrics().density);
        final float[] posicoes = {-0.22f, 0.55f, -0.55f};   // botão em cada risco

        tinta.setStrokeCap(Paint.Cap.ROUND);
        for (int i = 0; i < 3; i++) {
            final float y = c.cy + (i - 1) * lado * 0.62f;
            tinta.setStyle(Paint.Style.STROKE);
            tinta.setStrokeWidth(d * 1.6f);
            tinta.setColor(Color.WHITE);
            tinta.setAlpha(alfa(228 + 27 * c.carga));
            canvas.drawLine(c.cx - lado, y, c.cx + lado, y, tinta);
            tinta.setStyle(Paint.Style.FILL);
            canvas.drawCircle(c.cx + posicoes[i] * lado, y, d * 2.6f, tinta);
        }
        tinta.setStrokeCap(Paint.Cap.BUTT);
    }

    private void desenharAnalogico(Canvas canvas, Controlo c) {
        final boolean principal = c == manche;
        final float dx = principal ? mancheDx : cDx;
        final float dy = principal ? mancheDy : cDy;

        // O ANEL EXTERIOR, EM DUAS PASSAGENS.
        //
        // Um risco branco com alfa baixo desaparece sobre fundo claro, e um
        // manche que não se vê é pior do que nenhum, porque o polegar perde a
        // referência do centro. Um risco escuro por baixo, um pouco mais grosso,
        // resolve sem escurecer nada: é a mesma ideia da sombra do texto.
        final float d1 = getResources().getDisplayMetrics().density;
        tinta.setStyle(Paint.Style.STROKE);
        tinta.setStrokeWidth(d1 * 2.2f);
        tinta.setColor(Color.BLACK);
        tinta.setAlpha(alfa(58));
        canvas.drawCircle(c.cx, c.cy, c.raio, tinta);
        tinta.setStrokeWidth(d1 * 1.2f);
        tinta.setColor(Color.WHITE);
        tinta.setAlpha(alfa(120));
        canvas.drawCircle(c.cx, c.cy, c.raio, tinta);
        tinta.setStyle(Paint.Style.FILL);

        // O puck segue o polegar, limitado ao anel.
        final float limite = c.raio * 0.52f;
        final float dist = (float) Math.hypot(dx, dy);
        final float k = dist > limite ? limite / dist : 1f;
        caminho.reset();
        caminho.addCircle(c.cx + dx * k, c.cy + dy * k, c.raio * 0.56f, Path.Direction.CW);
        paintControl(canvas, caminho, c.carga);
    }

    private void escrever(Canvas canvas, String s, float cx, float cy,
                          float tamanho, float carga) {
        if (s == null || s.isEmpty()) return;
        texto.setTextSize(tamanho);
        texto.setColor(Color.WHITE);
        texto.setAlpha(alfa(216 + 39 * carga));
        // Uma sombra curta por baixo do texto: sobre uma imagem clara o branco
        // puro desaparece.
        texto.setShadowLayer(tamanho * 0.18f, 0, tamanho * 0.04f, 0x66000000);
        final Paint.FontMetrics fm = texto.getFontMetrics();
        canvas.drawText(s, cx, cy - (fm.ascent + fm.descent) / 2f, texto);
        texto.clearShadowLayer();
    }

    // -----------------------------------------------------------------------
    // TOQUE
    //
    // Multitoque de verdade: cada dedo fica preso ao controle onde pousou até
    // levantar. Sem isso, arrastar o polegar do manche para cima de um botão
    // dispara o botão — que é exatamente o que não se quer num jogo.
    // -----------------------------------------------------------------------
    @Override
    public boolean onTouchEvent(MotionEvent e) {
        final int accao = e.getActionMasked();
        switch (accao) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                final int i = e.getActionIndex();
                pousar(e.getPointerId(i), e.getX(i), e.getY(i));
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                for (int i = 0; i < e.getPointerCount(); i++) {
                    arrastar(e.getPointerId(i), e.getX(i), e.getY(i));
                }
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_CANCEL: {
                final int id = accao == MotionEvent.ACTION_CANCEL
                        ? -2 : e.getPointerId(e.getActionIndex());
                levantar(id);
                break;
            }
        }
        enviarEstado();
        invalidate();
        return true;
    }

    private void pousar(int dedo, float x, float y) {
        for (Controlo c : controlos) {
            if (c.dedo < 0 && c.contem(x, y)) {
                c.dedo = dedo;
                if (c.forma == Forma.ANALOGICO) arrastar(dedo, x, y);
                return;
            }
        }
    }

    private void arrastar(int dedo, float x, float y) {
        for (Controlo c : controlos) {
            if (c.dedo == dedo && c.forma == Forma.ANALOGICO) {
                if (c == manche) { mancheDx = x - c.cx; mancheDy = y - c.cy; }
                else             { cDx = x - c.cx; cDy = y - c.cy; }
            }
        }
    }

    private void levantar(int dedo) {
        for (Controlo c : controlos) {
            if (c.dedo == dedo || dedo == -2) {
                final boolean eraMenu = c.papel == Papel.MENU && c.dedo >= 0;
                c.dedo = -1;
                if (c == manche) { mancheDx = mancheDy = 0; }
                if (c == mancheC) { cDx = cDy = 0; }
                // O menu abre AO SOLTAR, e não ao encostar: assim dá para
                // deslizar o dedo para fora e desistir, como em qualquer botão
                // de sistema.
                if (eraMenu && aoPedirMenu != null) aoPedirMenu.pediu();
            }
        }
    }

    /** Solta tudo. A Activity chama isto ao abrir o menu ou ao perder o foco. */
    void soltarTudo() {
        levantar(-2);
        enviarEstado();
        invalidate();
    }

    // -----------------------------------------------------------------------
    // O estado que atravessa para o jogo
    // -----------------------------------------------------------------------
    private void enviarEstado() {
        int botoes = 0;
        int gatilhoEsq = 0, gatilhoDir = 0;
        for (Controlo c : controlos) {
            if (c.dedo < 0) continue;
            switch (c.papel) {
                case BOTAO:       botoes |= c.mascara; break;
                case GATILHO_ESQ: gatilhoEsq = 255;    break;
                case GATILHO_DIR: gatilhoDir = 255;    break;
                default: break;   // ANALOGICO, C_STICK e MENU não são botões
            }
        }
        final int[] m = eixo(manche, mancheDx, mancheDy);
        final int[] c = eixo(mancheC, cDx, cDy);
        Toque.estado(botoes, gatilhoEsq, gatilhoDir, m[0], m[1], c[0], c[1]);
    }

    /**
     * Converte o deslocamento do polegar num par de eixos de -32767 a 32767.
     *
     * <p>TRÊS DETALHES QUE NÃO SÃO ÓBVIOS:
     *
     * <ul>
     *   <li>o Y da tela cresce para BAIXO e o do SDL também, então aqui NÃO se
     *       inverte o sinal. É o contrário do que a intuição pede, e é por isso
     *       que está escrito: o eixo Y do SDL_GAMEPAD é positivo para baixo;
     *   <li>a zona morta é aplicada ao MÓDULO e depois o resto é reesticado para
     *       0..1. Cortar cada eixo em separado deixaria o manche com quatro
     *       cantos vivos e o centro morto em cruz;
     *   <li>o curso cheio é 85% do raio do anel, e não o raio: chegar ao máximo
     *       exatamente na borda exigiria precisão que o polegar não tem.
     * </ul>
     */
    private int[] eixo(Controlo anel, float dx, float dy) {
        if (anel == null || anel.raio <= 0) return new int[]{0, 0};
        final float curso = anel.raio * 0.85f;
        float x = dx / curso, y = dy / curso;
        float modulo = (float) Math.hypot(x, y);
        if (modulo <= zonaMorta) return new int[]{0, 0};
        if (modulo > 1f) { x /= modulo; y /= modulo; modulo = 1f; }
        final float escalado = (modulo - zonaMorta) / (1f - zonaMorta);
        final float k = escalado / modulo;
        return new int[]{
            Math.round(Math.max(-1f, Math.min(1f, x * k)) * 32767f),
            Math.round(Math.max(-1f, Math.min(1f, y * k)) * 32767f)
        };
    }
}
