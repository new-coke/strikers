package io.github.newcoke.strikers;

import android.content.Context;
import android.util.AtomicFile;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * As opções da interface, escritas no strikers.ini que o jogo lê.
 *
 * <p>O lado nativo não fala SharedPreferences: ele lê um ini na pasta do jogo e
 * o transforma em variáveis de ambiente antes do main. Então o menu grava nas
 * preferências, que são a fonte da verdade da interface, e esta classe projeta
 * isso no arquivo.
 *
 * <p>REESCREVER PRESERVANDO O QUE NÃO É NOSSO, e não gerar o arquivo do zero: o
 * ini é do jogador — ele pode ter mexido à mão, posto um caminho de dados, um
 * remapeamento de botão, um comentário. Uma chave que o menu controla é
 * substituída no lugar onde já estava; uma que ainda não existe é acrescentada
 * no fim; tudo o mais passa intacto, comentários inclusive.
 */
final class Configuracao {

    private static final String TAG = "strikers";

    private Configuracao() {}

    /** Projeta as preferências no ini. Devolve false se não deu para gravar. */
    static boolean gravar(Context ctx, Preferencias prefs) {
        Map<String, String> chaves = new LinkedHashMap<>();

        // Vídeo.
        //
        // O res_scale do jogo NÃO é uma porcentagem: é um múltiplo das 448 linhas
        // do GameCube. A interface pergunta em linhas, que é o que alguém sabe
        // querer, e a conversão acontece aqui. Zero linhas quer dizer "seguir a
        // tela", e isso no ini se escreve NÃO escrevendo a chave.
        final int linhas = prefs.numero(Preferencias.RES_LINHAS);
        chaves.put("res_scale", linhas <= 0 ? null : String.format(
                java.util.Locale.ROOT, "%.3f", linhas / 448f));
        chaves.put("msaa", Integer.toString(prefs.numero(Preferencias.MSAA)));
        chaves.put("aniso", Integer.toString(prefs.numero(Preferencias.ANISO)));
        final int fps = prefs.numero(Preferencias.FPS_LIMIT);
        chaves.put("fps_limit", fps < 0 ? null : Integer.toString(fps));
        chaves.put("vsync", prefs.ligado(Preferencias.VSYNC) ? "1" : "0");
        // A arte de menu numa tela mais larga do que 4:3. Escrita por extenso e
        // não como 0/1 porque quem abre o ini à mão lê "fit" e entende; lê "0" e
        // tem de adivinhar de que lado está o desligado.
        chaves.put("widen",
                prefs.numero(Preferencias.ESTICAR) != 0 ? "stretch" : "fit");
        // A proporção. "auto" é o que o porte faz sem a chave, então seguir a
        // tela se escreve NÃO escrevendo nada — assim o ini não afirma uma
        // escolha onde não houve uma.
        final int forma = prefs.numero(Preferencias.PROPORCAO);
        chaves.put("aspect", forma == 1 ? "16:9" : forma == 2 ? "4:3" : null);

        // Jogo.
        chaves.put("unlock_all", prefs.ligado(Preferencias.DESBLOQUEAR) ? "1" : null);
        chaves.put("region", vazioParaNulo(prefs.texto(Preferencias.REGIAO)));
        chaves.put("language", vazioParaNulo(prefs.texto(Preferencias.IDIOMA)));

        // O painel de desempenho NAO e mais este: HudView desenha o dele em Java,
        // e ligar os dois poria dois medidores na tela dizendo a mesma coisa em
        // formatos diferentes. A chave fica comentada no arquivo, para quem
        // quiser o antigo com o menu de depuracao junto.
        chaves.put("overlay", null);

        // O backend continua sendo vulkan sempre: é o único que o Dawn tem aqui.
        chaves.put("backend", "vulkan");

        return reescrever(new File(StrikersHome.homeDir(ctx), "strikers.ini"), chaves);
    }

    static boolean gravarTexturas(Context context, String pack) {
        Map<String, String> keys = new LinkedHashMap<>();
        boolean off = TexturePacks.OFF.equals(pack);
        keys.put("textures", off ? "0" : null);
        keys.put("texture_pack", off || pack.isEmpty() ? null : "\"" + pack + "\"");
        return reescrever(new File(StrikersHome.homeDir(context), "strikers.ini"), keys);
    }

    private static String vazioParaNulo(String s) {
        return s == null || s.isEmpty() ? null : s;
    }

    /**
     * Substitui cada chave onde ela já estiver, acrescenta as que faltam e
     * comenta as que viraram nulas.
     *
     * <p>Comentar em vez de apagar tem uma razão: uma chave ausente e uma chave
     * comentada dizem a mesma coisa ao jogo, mas só a comentada diz ao jogador
     * que aquela opção existe e está no padrão.
     */
    private static synchronized boolean reescrever(File ini, Map<String, String> chaves) {
        AtomicFile file = new AtomicFile(ini);
        List<String> linhas = new ArrayList<>();
        if (ini.exists()) {
            try (BufferedReader r = new BufferedReader(
                    new InputStreamReader(file.openRead(), StandardCharsets.UTF_8))) {
                String linha;
                while ((linha = r.readLine()) != null) linhas.add(linha);
            } catch (IOException e) {
                Log.w(TAG, "não deu para ler " + ini, e);
                return false;
            }
        }

        Map<String, Boolean> postas = new LinkedHashMap<>();
        for (String k : chaves.keySet()) postas.put(k, false);

        for (int i = 0; i < linhas.size(); i++) {
            final String crua = linhas.get(i);
            final String nome = nomeDaChave(crua);
            if (nome == null || !chaves.containsKey(nome)) continue;
            if (Boolean.TRUE.equals(postas.get(nome))) {
                // Uma segunda ocorrência da mesma chave: o jogo leria a última,
                // então esta tem de sair do caminho.
                linhas.set(i, "#" + crua);
                continue;
            }
            final String valor = chaves.get(nome);
            linhas.set(i, valor == null ? "#" + nome + " =" : nome + " = " + valor);
            postas.put(nome, true);
        }

        List<String> novas = new ArrayList<>();
        for (Map.Entry<String, String> e : chaves.entrySet()) {
            if (Boolean.TRUE.equals(postas.get(e.getKey()))) continue;
            novas.add(e.getValue() == null
                    ? "#" + e.getKey() + " ="
                    : e.getKey() + " = " + e.getValue());
        }
        if (!novas.isEmpty()) {
            linhas.add("");
            linhas.add("# Escrito pelo menu de configurações do aplicativo.");
            linhas.addAll(novas);
        }

        FileOutputStream stream = null;
        try {
            stream = file.startWrite();
            OutputStreamWriter writer = new OutputStreamWriter(stream, StandardCharsets.UTF_8);
            for (String linha : linhas) {
                writer.write(linha);
                writer.write("\n");
            }
            writer.flush();
            file.finishWrite(stream);
            return true;
        } catch (IOException e) {
            if (stream != null) file.failWrite(stream);
            Log.w(TAG, "não deu para gravar " + ini, e);
            return false;
        }
    }

    /**
     * O nome da chave numa linha, comentada ou não, ou null se a linha não for
     * uma atribuição.
     *
     * <p>As comentadas contam: é onde o padrão está documentado, e é ali que a
     * escolha do jogador deve aparecer em vez de no fim do arquivo, longe da
     * explicação.
     */
    private static String nomeDaChave(String linha) {
        String s = linha.trim();
        while (s.startsWith("#") || s.startsWith(";")) s = s.substring(1).trim();
        final int igual = s.indexOf('=');
        if (igual <= 0) return null;
        final String nome = s.substring(0, igual).trim();
        if (nome.isEmpty()) return null;
        for (int i = 0; i < nome.length(); i++) {
            final char c = nome.charAt(i);
            if (!Character.isLetterOrDigit(c) && c != '_') return null;
        }
        return nome.toLowerCase(java.util.Locale.ROOT);
    }
}
