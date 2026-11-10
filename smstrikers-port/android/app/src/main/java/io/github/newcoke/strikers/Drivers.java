package io.github.newcoke.strikers;

import android.content.Context;
import android.net.Uri;
import android.util.Log;

import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Os drivers de vídeo que o jogador instalou.
 *
 * <p>Um pacote de driver é o formato que a comunidade do libadrenotools usa: um
 * .zip com um {@code meta.json} e o {@code .so} do driver ao lado. O meta diz o
 * nome a mostrar e, o que importa de verdade, o SONAME do {@code .so} — que não
 * é adivinhável, varia de pacote para pacote.
 *
 * <p>ONDE O .so FICA, E POR QUE NÃO NA PASTA DO JOGO. O {@code dlopen} do Android
 * recusa carregar uma biblioteca de um lugar onde qualquer aplicativo poderia
 * tê-la trocado, e {@code /storage/emulated/0/Strikers} é exatamente esse tipo
 * de lugar. Então os pacotes podem ser deixados lá, mas a instalação copia o
 * conteúdo para a pasta privada do aplicativo, que é de onde ele carrega.
 */
final class Drivers {

    private static final String TAG = "strikers";

    /** Um driver instalado. */
    static final class Driver {
        final String id;          // o nome da pasta
        final String nome;
        final String descricao;
        final String soname;      // o que o adrenotools tem de carregar
        final File pasta;

        Driver(String id, String nome, String descricao, String soname, File pasta) {
            this.id = id;
            this.nome = nome;
            this.descricao = descricao;
            this.soname = soname;
            this.pasta = pasta;
        }
    }

    /** Um driver que dá para baixar. */
    static final class Fonte {
        final String nome;
        final String descricao;
        final String url;

        Fonte(String nome, String descricao, String url) {
            this.nome = nome;
            this.descricao = descricao;
            this.url = url;
        }
    }

    /**
     * A lista de onde baixar.
     *
     * <p>Curta e com a origem à vista de propósito: baixar e carregar código que
     * roda dentro do processo do jogo não é coisa para esconder atrás de um
     * botão anônimo. Se um endereço parar de responder — e vão parar, são
     * lançamentos de terceiros — o caminho continua sendo baixar à mão e usar
     * Importar.
     */
    static List<Fonte> fontes(Context ctx) {
        List<Fonte> f = new ArrayList<>();
        // OS NOMES NÃO SÃO TRADUZIDOS: "Turnip 26.0 — Adreno 8xx" é como o
        // arquivo se chama lá na origem, e quem for conferir o endereço à mão
        // procura essa palavra. As descrições são frases e essas sim traduzem.
        f.add(new Fonte("Turnip 26.0 — Adreno 8xx",
                ctx.getString(R.string.drv_a8xx_desc),
                "https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v26.0.0-rc08/"
              + "turnip_a8xx.zip"));
        f.add(new Fonte("Turnip 26.0 — geral",
                ctx.getString(R.string.drv_general_desc),
                "https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v26.0.0-rc08/"
              + "Turnip_v26.0.0_R8.zip"));
        f.add(new Fonte("Qualcomm 840",
                ctx.getString(R.string.drv_qualcomm_desc),
                "https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v840/"
              + "Qualcomm_840_adpkg.zip"));
        return f;
    }

    private Drivers() {}

    /** A pasta privada onde os drivers instalados vivem. */
    static File instalados(Context ctx) {
        File d = new File(ctx.getFilesDir(), "drivers");
        //noinspection ResultOfMethodCallIgnored
        d.mkdirs();
        return d;
    }

    /** Onde o jogador pode largar os .zip sem precisar do seletor. */
    static File caixaDeEntrada(Context ctx) {
        File d = new File(StrikersHome.homeDir(ctx), "drivers");
        //noinspection ResultOfMethodCallIgnored
        d.mkdirs();
        return d;
    }

    static List<Driver> lista(Context ctx) {
        List<Driver> saida = new ArrayList<>();
        File[] pastas = instalados(ctx).listFiles();
        if (pastas == null) return saida;
        for (File pasta : pastas) {
            if (!pasta.isDirectory()) continue;
            Driver d = ler(pasta);
            if (d != null) saida.add(d);
        }
        Collections.sort(saida, (a, b) -> a.nome.compareToIgnoreCase(b.nome));
        return saida;
    }

    static Driver porId(Context ctx, String id) {
        if (id == null || id.isEmpty()) return null;
        for (Driver d : lista(ctx)) {
            if (d.id.equals(id)) return d;
        }
        return null;
    }

    private static Driver ler(File pasta) {
        File meta = new File(pasta, "meta.json");
        if (!meta.isFile()) return null;
        try {
            JSONObject j = new JSONObject(texto(meta));
            String soname = j.optString("libraryName", "");
            if (soname.isEmpty() || !new File(pasta, soname).isFile()) {
                Log.w(TAG, "driver em " + pasta + " sem o .so que o meta.json anuncia");
                return null;
            }
            return new Driver(pasta.getName(),
                    j.optString("name", pasta.getName()),
                    j.optString("description", ""),
                    soname, pasta);
        } catch (Exception e) {
            Log.w(TAG, "meta.json ilegível em " + pasta, e);
            return null;
        }
    }

    private static String texto(File f) throws IOException {
        try (InputStream in = new java.io.FileInputStream(f)) {
            java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
            byte[] b = new byte[8192];
            int n;
            while ((n = in.read(b)) > 0) out.write(b, 0, n);
            return out.toString("UTF-8");
        }
    }

    /**
     * Instala um pacote a partir de um fluxo de .zip. Devolve o driver ou null.
     *
     * <p>Extrai para uma pasta temporária e só renomeia no fim: um .zip truncado
     * no meio da cópia deixaria um driver pela metade, e um driver pela metade é
     * um jogo que não abre com uma mensagem que não ajuda.
     */
    static Driver instalar(Context ctx, InputStream zip, String nomeSugerido) {
        File destino = new File(instalados(ctx), slug(nomeSugerido));
        File temporaria = new File(instalados(ctx), ".tmp-" + System.currentTimeMillis());
        //noinspection ResultOfMethodCallIgnored
        temporaria.mkdirs();
        try (ZipInputStream z = new ZipInputStream(zip)) {
            ZipEntry e;
            while ((e = z.getNextEntry()) != null) {
                if (e.isDirectory()) continue;
                // Só o nome do arquivo: um .zip pode trazer "../" no caminho, e
                // extrair isso escreveria fora da pasta de destino.
                String nome = new File(e.getName()).getName();
                if (nome.isEmpty()) continue;
                try (OutputStream out = new FileOutputStream(new File(temporaria, nome))) {
                    byte[] b = new byte[64 * 1024];
                    int n;
                    while ((n = z.read(b)) > 0) out.write(b, 0, n);
                }
            }
        } catch (IOException e) {
            Log.w(TAG, "não deu para extrair o driver", e);
            apagar(temporaria);
            return null;
        }

        Driver lido = ler(temporaria);
        if (lido == null) {
            apagar(temporaria);
            return null;
        }
        apagar(destino);
        if (!temporaria.renameTo(destino)) {
            apagar(temporaria);
            return null;
        }
        return ler(destino);
    }

    /** Baixa e instala. Chamar fora da thread principal. */
    static Driver baixar(Context ctx, Fonte fonte) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(fonte.url).openConnection();
            c.setInstanceFollowRedirects(true);
            c.setConnectTimeout(20000);
            c.setReadTimeout(60000);
            if (c.getResponseCode() / 100 != 2) {
                Log.w(TAG, "download de " + fonte.url + " respondeu " + c.getResponseCode());
                return null;
            }
            try (InputStream in = c.getInputStream()) {
                return instalar(ctx, in, fonte.nome);
            }
        } catch (Exception e) {
            Log.w(TAG, "não deu para baixar " + fonte.url, e);
            return null;
        } finally {
            if (c != null) c.disconnect();
        }
    }

    static Driver importar(Context ctx, Uri uri) {
        try (InputStream in = ctx.getContentResolver().openInputStream(uri)) {
            if (in == null) return null;
            String nome = uri.getLastPathSegment();
            return instalar(ctx, in, nome != null ? nome : "driver");
        } catch (Exception e) {
            Log.w(TAG, "não deu para importar " + uri, e);
            return null;
        }
    }

    static void remover(Context ctx, Driver d) {
        apagar(d.pasta);
    }

    private static void apagar(File f) {
        if (f == null || !f.exists()) return;
        File[] filhos = f.listFiles();
        if (filhos != null) {
            for (File c : filhos) apagar(c);
        }
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }

    private static String slug(String nome) {
        String s = nome == null ? "driver" : nome;
        s = s.toLowerCase(Locale.ROOT).replaceAll("\\.zip$", "");
        s = s.replaceAll("[^a-z0-9._-]+", "-").replaceAll("^-+|-+$", "");
        return s.isEmpty() ? "driver" : s;
    }
}
