package io.github.newcoke.strikers;

import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.WindowInsets;

import java.io.RandomAccessFile;

/**
 * O painel de desempenho, no formato de um monitor de hardware: rótulo pequeno e
 * colorido em cima, número grande embaixo, unidade numa caixinha.
 *
 * <p>FICA NO TOPO, AO CENTRO, e não num canto: os cantos são todos dos botões —
 * LT/LB/L3 à esquerda, RT/RB/R3 à direita, manches e face embaixo. A faixa
 * central de cima é a única parte da tela que o overlay não usa.
 *
 * <p>O QUE É MEDIDO E O QUE NÃO É, dito aqui para ninguém ler um número inventado
 * como se fosse real:
 *
 * <ul>
 *   <li><b>FPS</b> e <b>quadro</b> — medidos, e são do JOGO: vêm do limitador do
 *       porte por JNI. O Android sabe dizer quantas vezes a tela atualizou, e
 *       esse número marcaria 120 com o jogo congelado numa imagem;
 *   <li><b>CPU</b> — o tempo que o jogo gastou trabalhando dentro do quadro.
 *       Não é ocupação do processador: /proc/stat deixou de ser legível por
 *       aplicativos no Android 8, e não há como saber ocupação honestamente;
 *   <li><b>RAM</b> — medida, do ActivityManager;
 *   <li><b>TEMP</b> — medida SE o aparelho expuser uma zona térmica legível.
 *       Muitos não expõem, e aí fica "--";
 *   <li><b>GPU</b> — a ocupação da GPU não tem API pública no Android. A Adreno
 *       publica em /sys/class/kgsl quando o arquivo é legível; quando não for,
 *       fica "--" em vez de um número bonito.
 * </ul>
 *
 * <p>Um painel que mente é pior do que não ter painel: quem olha está tentando
 * decidir se o jogo está esquentando ou engasgando, e um número decorativo
 * estraga exatamente essa decisão.
 */
public class HudView extends View {

    // As cores do monitor: rótulo colorido, número branco, unidade apagada.
    private static final int VERDE   = 0xFF3DDC84;
    private static final int CIANO   = 0xFF3FC7E8;
    private static final int AMARELO = 0xFFF2C037;
    private static final int LARANJA = 0xFFFF8A3D;
    private static final int APAGADO = 0xFF9AA3AD;

    private final Paint tinta = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint rotulo = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint numero = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint unidade = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF caixa = new RectF();

    private final String soc;
    private String fps = "--", quadro = "--", cpu = "--";
    private String ram = "--", temp = "--", gpu = "--";

    private final long[] medidas = new long[Medidas.QUANTIDADE];
    private final Handler relogio = new Handler(Looper.getMainLooper());
    private int recuoCima;

    public HudView(Context ctx) {
        super(ctx);
        rotulo.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        rotulo.setTextAlign(Paint.Align.CENTER);
        numero.setTypeface(Typeface.create("sans-serif", Typeface.BOLD));
        numero.setTextAlign(Paint.Align.CENTER);
        numero.setColor(Color.WHITE);
        unidade.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        unidade.setTextAlign(Paint.Align.CENTER);
        unidade.setColor(APAGADO);

        soc = Build.VERSION.SDK_INT >= 31 && Build.SOC_MODEL != null
                && !"unknown".equals(Build.SOC_MODEL) ? Build.SOC_MODEL : Build.HARDWARE;

        // QUATRO LEITURAS POR SEGUNDO para o que vem do jogo, uma por segundo
        // para o resto. Os contadores custam uma travessia de JNI e nada mais;
        // as outras medidas leem arquivos em /sys, e fazer isso a 4 Hz seria
        // pagar mais pela medição do que ela vale.
        relogio.post(new Runnable() {
            private int volta;
            @Override public void run() {
                lerJogo();
                if (volta++ % 4 == 0) lerSistema();
                invalidate();
                relogio.postDelayed(this, 250);
            }
        });
    }

    @Override
    public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            recuoCima = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout()).top;
        } else {
            recuoCima = insets.getSystemWindowInsetTop();
        }
        requestLayout();
        return insets;
    }

    @Override
    protected void onDetachedFromWindow() {
        relogio.removeCallbacksAndMessages(null);
        super.onDetachedFromWindow();
    }

    private void lerJogo() {
        if (!Medidas.ler(medidas)) {
            fps = quadro = cpu = "--";
            return;
        }
        final long us = medidas[Medidas.QUADRO_US];
        // O FPS DERIVADO DO TEMPO DE QUADRO quando ele existe: a janela móvel do
        // limitador é estável, mas o tempo do último quadro é o que mostra um
        // engasgo no instante em que ele acontece.
        // SEM CASA DECIMAL a partir de 100. Com o limite desligado este número
        // chega a quatro dígitos, e "1520,1" não cabe na coluna -- encosta no
        // risco do vizinho. Abaixo de 100 a decimal é justamente onde se vê a
        // diferença entre "trava" e "está quase", então ali ela fica.
        final float f = medidas[Medidas.FPS_CENTESIMOS] / 100f;
        fps = f >= 100f ? String.format("%.0f", f) : String.format("%.1f", f);
        quadro = us > 0 ? String.format("%.1f", us / 1000f) : "--";
        cpu = String.format("%.2f", medidas[Medidas.OCUPADO_US] / 1000f);
    }

    private void lerSistema() {
        final ActivityManager am =
                (ActivityManager) getContext().getSystemService(Context.ACTIVITY_SERVICE);
        if (am != null) {
            final ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
            am.getMemoryInfo(mi);
            if (mi.totalMem > 0) {
                ram = Long.toString(Math.round(100f * (mi.totalMem - mi.availMem) / mi.totalMem));
            }
        }
        temp = lerTemperatura();
        gpu = lerGpu();
    }

    /** Percorre as zonas térmicas atrás de uma que seja da CPU ou do SoC. */
    private String lerTemperatura() {
        for (int i = 0; i < 16; i++) {
            final String tipo = lerArquivo("/sys/class/thermal/thermal_zone" + i + "/type");
            if (tipo == null) continue;
            final String t = tipo.toLowerCase();
            if (!(t.contains("cpu") || t.contains("soc") || t.contains("tsens")
                  || t.contains("ap"))) continue;
            final String bruto = lerArquivo("/sys/class/thermal/thermal_zone" + i + "/temp");
            if (bruto == null) continue;
            try {
                float v = Float.parseFloat(bruto.trim());
                // Uns exportam milésimos de grau, outros graus.
                if (v > 1000f) v /= 1000f;
                if (v > 10f && v < 150f) return String.format("%.0f", v);
            } catch (NumberFormatException ignorado) { }
        }
        return "--";
    }

    /** Ocupação da Adreno, quando o nó do kgsl for legível. */
    private String lerGpu() {
        final String bruto = lerArquivo("/sys/class/kgsl/kgsl-3d0/gpubusy");
        if (bruto == null) return "--";
        final String[] partes = bruto.trim().split("\\s+");
        if (partes.length < 2) return "--";
        try {
            final long ocupado = Long.parseLong(partes[0]);
            final long total = Long.parseLong(partes[1]);
            if (total <= 0) return "--";
            return Long.toString(Math.round(100f * ocupado / total));
        } catch (NumberFormatException e) {
            return "--";
        }
    }

    private static String lerArquivo(String caminho) {
        try (RandomAccessFile f = new RandomAccessFile(caminho, "r")) {
            return f.readLine();
        } catch (Exception e) {
            return null;   // não legível neste aparelho: fica "--"
        }
    }

    // -----------------------------------------------------------------------
    // Desenho
    // -----------------------------------------------------------------------

    /** As seis colunas do painel, na ordem em que aparecem. */
    private static final String[] ROTULOS  = {"FPS", "QUADRO", "CPU", "GPU", "RAM", "TEMP"};
    private static final String[] UNIDADES = {"", "MS", "MS", "%", "%", "°C"};
    private static final int[] CORES = {VERDE, CIANO, CIANO, AMARELO, AMARELO, LARANJA};

    private String valor(int i) {
        switch (i) {
            case 0: return fps;
            case 1: return quadro;
            case 2: return cpu;
            case 3: return gpu;
            case 4: return ram;
            default: return temp;
        }
    }

    @Override
    protected void onMeasure(int larguraSpec, int alturaSpec) {
        final float d = getResources().getDisplayMetrics().density;
        // Largura fixa por coluna, e não medida pelo conteúdo: assim o painel
        // não muda de tamanho quando "9.9" vira "60.4", que num número que se
        // atualiza quatro vezes por segundo seria um tremor constante.
        setMeasuredDimension(Math.round(ROTULOS.length * 76 * d + 24 * d),
                             Math.round(88 * d));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        final float d = getResources().getDisplayMetrics().density;
        final float w = getWidth(), h = getHeight();

        caixa.set(0, 0, w, h);
        final float r = 17 * d;
        tinta.setColor(0xD9000000);
        canvas.drawRoundRect(caixa, r, r, tinta);
        tinta.setStyle(Paint.Style.STROKE);
        tinta.setStrokeWidth(Math.max(1f, d));
        tinta.setColor(0x33FFFFFF);
        canvas.drawRoundRect(caixa, r, r, tinta);
        tinta.setStyle(Paint.Style.FILL);

        rotulo.setTextSize(12 * d);
        numero.setTextSize(29 * d);
        unidade.setTextSize(10 * d);

        final float larguraColuna = (w - 24 * d) / ROTULOS.length;
        for (int i = 0; i < ROTULOS.length; i++) {
            final float cx = 12 * d + larguraColuna * (i + 0.5f);

            if (i > 0) {
                tinta.setColor(0x26FFFFFF);
                canvas.drawRect(12 * d + larguraColuna * i - 0.5f * d, 16 * d,
                                12 * d + larguraColuna * i + 0.5f * d, h - 16 * d, tinta);
            }

            rotulo.setColor(CORES[i]);
            canvas.drawText(ROTULOS[i], cx, 24 * d, rotulo);
            canvas.drawText(valor(i), cx, 57 * d, numero);
            if (!UNIDADES[i].isEmpty()) {
                canvas.drawText(UNIDADES[i], cx, 75 * d, unidade);
            }
        }

        // O nome do SoC, pequeno, embaixo à direita: identifica a medição sem
        // roubar uma coluna.
        unidade.setTextAlign(Paint.Align.RIGHT);
        canvas.drawText(soc, w - 12 * d, h - 4 * d, unidade);
        unidade.setTextAlign(Paint.Align.CENTER);
    }

    /** O recuo do topo, para a Activity posicionar abaixo do entalhe. */
    int recuoDoTopo() { return recuoCima; }
}
