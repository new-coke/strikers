package io.github.newcoke.strikers;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.net.Uri;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

import java.io.IOException;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

import dev.encounter.aurora.AuroraSurface;

/** O jogo. Tudo que decide se ele pode começar está em {@link SetupActivity}. */
public class StrikersActivity extends SDLActivity {

    private ParcelFileDescriptor disc;
    private OverlayView overlay;
    private HudView hud;
    private Preferencias prefs;
    private final Handler relogio = new Handler(Looper.getMainLooper());

    @Override
    public void loadLibraries() {
        Uri uri = getIntent().getData();
        try {
            if (uri == null) {
                Os.unsetenv("STRIKERS_ANDROID_DISC_FD");
            } else {
                disc = DiscSelection.open(this, uri);
                Os.setenv("STRIKERS_ANDROID_DISC_FD", Integer.toString(disc.getFd()), true);
            }
        } catch (IOException | SecurityException | ErrnoException error) {
            Log.w("strikers", "Could not open the selected disc", error);
            throw new IllegalStateException(getString(R.string.disc_open_failed), error);
        }
        super.loadLibraries();
    }

    @Override
    protected String[] getLibraries() {
        // Só uma: o SDL é linkado dentro do libmain.so em vez de carregado ao
        // lado. Ver a nota em tools/configure-android.sh.
        return new String[] { "main" };
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        // A do Aurora, não a do SDL. Ela avisa o renderizador quando a surface
        // some, e no Android a surface some toda vez que o jogador troca de
        // aplicativo — uma swapchain do WebGPU ainda apontada para um
        // ANativeWindow destruído não é um quadro preto, é um travamento.
        return new AuroraSurface(context);
    }

    @Override
    protected void attachBaseContext(Context base) {
        // O idioma escolhido no launcher, antes de qualquer getString. Ver
        // Idiomas.envolver.
        super.attachBaseContext(Idiomas.envolver(base));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Antes do super, que é quem carrega o libmain.so: o jogo lê a
        // configuração do ambiente num construtor de biblioteca, então o que for
        // definido depois disto é tarde demais.
        StrikersHome.prepare(this);
        super.onCreate(savedInstanceState);
        if (mBrokenLibraries || isFinishing()) return;

        // Uma partida são minutos segurando o manche numa direção, o que nenhum
        // aparelho conta como atividade.
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        telaCheia();

        prefs = new Preferencias(this);
        overlay = new OverlayView(this);
        overlay.recarregar(prefs);
        overlay.aoPedirMenu(() -> {
            overlay.soltarTudo();
            startActivity(new Intent(this, SettingsActivity.class));
        });
        addContentView(overlay, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        // O painel de desempenho, no topo ao centro: e a unica faixa da tela que
        // o overlay de toque nao usa. Os cantos sao todos dos botoes.
        hud = new HudView(this);
        FrameLayout.LayoutParams lpHud = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lpHud.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
        lpHud.topMargin = Math.round(8 * getResources().getDisplayMetrics().density);
        addContentView(hud, lpHud);

        seguirControleFisico();
    }

    /**
     * Esconde as barras do sistema e deixa a janela ir até as bordas.
     *
     * <p>Não é só estética: sem isto a barra de navegação fica por cima do canto
     * inferior direito, que é exatamente onde o polegar direito vive, e o toque
     * vai para o sistema em vez de ir para o jogo.
     */
    private void telaCheia() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController c = getWindow().getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.systemBars());
                c.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                  | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                  | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                  | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                  | View.SYSTEM_UI_FLAG_FULLSCREEN
                  | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
    }

    /**
     * Some com o overlay enquanto houver um controle de verdade ligado.
     *
     * <p>Quem decide é o lado nativo, e não o Java: é ele que enumera os
     * gamepads do SDL e sabe qual deles é o nosso. Toque.ligada() responde
     * exatamente isso — existe um pad virtual? — e perguntar uma vez por segundo
     * é barato, porque do outro lado é uma leitura atômica. Um controle que
     * chega ou sai no meio de uma partida não é coisa que se perceba mais
     * rápido do que isso.
     */
    private void seguirControleFisico() {
        relogio.postDelayed(new Runnable() {
            @Override public void run() {
                if (overlay != null) {
                    final boolean mostrar = Toque.ligada();
                    final int quer = mostrar ? View.VISIBLE : View.GONE;
                    if (overlay.getVisibility() != quer) {
                        if (!mostrar) overlay.soltarTudo();
                        overlay.setVisibility(quer);
                    }
                }
                relogio.postDelayed(this, 1000);
            }
        }, 1000);
    }

    @Override
    protected void onResume() {
        super.onResume();
        telaCheia();
        if (overlay != null && prefs != null) overlay.recarregar(prefs);
        if (hud != null && prefs != null) {
            hud.setVisibility(prefs.ligado(Preferencias.HUD) ? View.VISIBLE : View.GONE);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean temFoco) {
        super.onWindowFocusChanged(temFoco);
        if (temFoco) telaCheia();
        else if (overlay != null) overlay.soltarTudo();
    }

    @Override
    protected void onDestroy() {
        relogio.removeCallbacksAndMessages(null);
        super.onDestroy();
        if (disc != null) {
            try {
                disc.close();
            } catch (IOException error) {
                Log.w("strikers", "Could not close the disc", error);
            }
        }
    }
}
