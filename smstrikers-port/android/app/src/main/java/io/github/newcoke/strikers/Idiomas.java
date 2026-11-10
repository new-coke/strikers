package io.github.newcoke.strikers;

import android.content.Context;
import android.content.res.Configuration;

import java.util.Locale;

/**
 * O idioma do aplicativo.
 *
 * <p>Por padrão o Android já escolhe sozinho: ele olha os idiomas do aparelho e
 * pega a pasta {@code values-*} que combina. Esta classe existe para o outro
 * caso — quem quer o aplicativo num idioma que não é o do aparelho. Não é raro:
 * um celular configurado em inglês por hábito, um aparelho emprestado, alguém
 * que prefere ler termos técnicos no original.
 *
 * <p>NÃO CONFUNDIR COM O IDIOMA DO JOGO. O disco tem os textos dele e o porte
 * escolhe entre os que o disco traz, pela chave {@code language} do strikers.ini
 * ({@link Preferencias#IDIOMA}). Este aqui é só a interface do aplicativo — o
 * launcher, as configurações, a tela de drivers. São duas coisas diferentes e
 * mudar uma não mexe na outra.
 */
final class Idiomas {

    private Idiomas() {}

    /**
     * As etiquetas BCP-47, e {@code ""} para seguir o aparelho.
     *
     * <p>A ORDEM E O CONTEÚDO ACOMPANHAM as pastas {@code values-*} que existem
     * de verdade. Oferecer um idioma sem tradução não dá erro — o Android cai no
     * padrão, que é o inglês — mas mostra "Español" no menu e inglês na tela, o
     * que é pior do que não ter oferecido.
     */
    static final String[] CODIGOS = {
        "", "en", "pt-BR", "es", "fr", "de", "it", "ja", "zh-CN", "ru"
    };

    /**
     * O nome de cada idioma NO PRÓPRIO IDIOMA.
     *
     * <p>Quem está procurando "日本語" numa lista em português não reconhece
     * "Japonês" tão rápido quanto reconhece a palavra que escreve todo dia — e
     * quem abriu esta lista provavelmente não lê bem o idioma em que ela está.
     */
    private static final String[] NOMES = {
        null,                 // preenchido com a tradução de "Seguir o sistema"
        "English",
        "Português (Brasil)",
        "Español",
        "Français",
        "Deutsch",
        "Italiano",
        "日本語",
        "简体中文",
        "Русский",
    };

    /** Os nomes para mostrar numa lista, com o primeiro traduzido. */
    static String[] nomes(Context ctx) {
        String[] n = NOMES.clone();
        n[0] = ctx.getString(R.string.language_system);
        return n;
    }

    /** O nome do idioma escolhido, para mostrar à direita da linha. */
    static String nomeDe(Context ctx, String codigo) {
        for (int i = 0; i < CODIGOS.length; i++) {
            if (CODIGOS[i].equals(codigo)) return nomes(ctx)[i];
        }
        // Um código que veio de uma versão anterior com mais idiomas do que esta:
        // mostrado como está, em vez de dizer "Seguir o sistema" e mentir.
        return codigo.isEmpty() ? ctx.getString(R.string.language_system) : codigo;
    }

    /**
     * O contexto que as telas devem usar: o de base quando o idioma segue o
     * aparelho, ou um com a configuração trocada quando não segue.
     *
     * <p>Chamado de {@code attachBaseContext}, que é antes de qualquer
     * {@code getString} da Activity. Mais tarde do que isso e metade da tela
     * já foi montada com os textos antigos.
     */
    static Context envolver(Context base) {
        final String codigo = new Preferencias(base).texto(Preferencias.IDIOMA_APP);
        if (codigo.isEmpty()) return base;

        final Locale idioma = Locale.forLanguageTag(codigo);
        final Configuration c = new Configuration(base.getResources().getConfiguration());
        c.setLocale(idioma);
        c.setLayoutDirection(idioma);
        return base.createConfigurationContext(c);
    }
}
