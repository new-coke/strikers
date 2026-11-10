package io.github.newcoke.strikers;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.Arrays;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

final class TexturePacks {
    static final String OFF = ":off:";
    private static final long MAX_UNPACKED_BYTES = 4L * 1024 * 1024 * 1024;
    private static final int MAX_ENTRIES = 50000;

    private TexturePacks() {}

    static String[] list(File root) {
        String[] packs = root.list((parent, name) -> !name.startsWith(".") && !name.equals(OFF)
                && name.indexOf('\n') == -1 && name.indexOf('\r') == -1
                && new File(parent, name).isDirectory());
        if (packs == null) return new String[0];
        Arrays.sort(packs, String.CASE_INSENSITIVE_ORDER);
        return packs;
    }

    static synchronized String install(File root, InputStream input, String archiveName)
            throws IOException {
        if (input == null) throw new IOException("Could not open archive");
        String base = archiveName.replaceFirst("(?i)\\.zip$", "")
                .replaceAll("[^\\p{L}\\p{N} ._-]", "_").trim().replaceFirst("^\\.+", "");
        if (base.isEmpty()) base = "Texture pack";
        if (base.codePointCount(0, base.length()) > 60) {
            base = base.substring(0, base.offsetByCodePoints(0, 60)).trim();
        }
        Path folder = root.toPath();
        Files.createDirectories(folder);
        Path staging = Files.createTempDirectory(folder.getParent(), ".texture-pack-");
        try {
            extract(input, staging);
            Path target = folder.resolve(base);
            for (int suffix = 2; Files.exists(target); suffix++) {
                target = folder.resolve(base + " (" + suffix + ")");
            }
            Files.move(staging, target);
            return target.getFileName().toString();
        } finally {
            deleteTree(staging);
        }
    }

    private static void extract(InputStream input, Path folder) throws IOException {
        byte[] buffer = new byte[64 * 1024];
        int count = 0;
        long bytes = 0;
        boolean containsTextures = false;
        try (ZipInputStream zip = new ZipInputStream(input)) {
            for (ZipEntry entry; (entry = zip.getNextEntry()) != null; ) {
                if (++count > MAX_ENTRIES) throw new IOException("Too many ZIP entries");
                String name = entry.getName().replace('\\', '/');
                Path relative = Paths.get(name);
                Path target = folder.resolve(relative).normalize();
                if (relative.isAbsolute() || !target.startsWith(folder) || target.equals(folder)) {
                    throw new IOException("Invalid ZIP path");
                }
                String file = target.getFileName().toString().toLowerCase(Locale.ROOT);
                boolean texture = !entry.isDirectory() && file.startsWith("tex1_")
                        && (file.endsWith(".png") || file.endsWith(".dds"));
                if (texture) Files.createDirectories(target.getParent());
                try (OutputStream out = texture ? Files.newOutputStream(target,
                        StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE) : null) {
                    for (int read; (read = zip.read(buffer)) != -1; ) {
                        bytes += read;
                        if (bytes > MAX_UNPACKED_BYTES) throw new IOException("ZIP exceeds 4 GiB");
                        if (out != null) out.write(buffer, 0, read);
                    }
                }
                containsTextures |= texture;
            }
        }
        if (!containsTextures) throw new IOException("No Dolphin-format PNG or DDS textures");
    }

    static void remove(File root, String name) throws IOException {
        Path folder = root.toPath();
        Path target = folder.resolve(name).normalize();
        if (!folder.equals(target.getParent())) throw new IOException("Invalid pack name");
        deleteTree(target);
    }

    private static void deleteTree(Path root) throws IOException {
        if (!Files.exists(root, LinkOption.NOFOLLOW_LINKS)) return;
        Files.walkFileTree(root, new SimpleFileVisitor<Path>() {
            @Override
            public FileVisitResult visitFile(Path file, BasicFileAttributes attrs) throws IOException {
                Files.delete(file);
                return FileVisitResult.CONTINUE;
            }

            @Override
            public FileVisitResult postVisitDirectory(Path dir, IOException error) throws IOException {
                if (error != null) throw error;
                Files.delete(dir);
                return FileVisitResult.CONTINUE;
            }
        });
    }
}
