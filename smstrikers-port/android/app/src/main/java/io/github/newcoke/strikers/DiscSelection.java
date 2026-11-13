package io.github.newcoke.strikers;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;

import java.io.IOException;

final class DiscSelection {
    private static final String URI = "uri";
    private static final String NAME = "name";

    private DiscSelection() {}

    private static SharedPreferences preferences(Context context) {
        return context.getSharedPreferences("selected-disc", Context.MODE_PRIVATE);
    }

    static Uri uri(Context context) {
        String value = preferences(context).getString(URI, null);
        return value == null ? null : Uri.parse(value);
    }

    static String name(Context context) {
        return preferences(context).getString(NAME, null);
    }

    static String displayName(Context context, Uri uri) {
        try (Cursor cursor = context.getContentResolver().query(uri,
                new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                String name = cursor.getString(0);
                if (name != null && !name.isEmpty()) return name;
            }
        }
        return context.getString(R.string.disc_image);
    }

    static void remember(Context context, Uri uri, String name, int readGrant) {
        Uri previous = uri(context);
        context.getContentResolver().takePersistableUriPermission(uri, readGrant);
        preferences(context).edit().putString(URI, uri.toString()).putString(NAME, name).apply();
        if (!uri.equals(previous)) release(context, previous);
    }

    static void forget(Context context) {
        Uri previous = uri(context);
        preferences(context).edit().remove(URI).remove(NAME).apply();
        release(context, previous);
    }

    private static void release(Context context, Uri uri) {
        if (uri == null) return;
        try {
            context.getContentResolver().releasePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException ignored) {
            // The provider may already have revoked the grant.
        }
    }

    static void checkImage(Context context, Uri uri) throws IOException {
        try (ParcelFileDescriptor file = open(context, uri)) {
            byte[] header = new byte[32];
            int count = Os.pread(file.getFileDescriptor(), header, 0, header.length, 0);
            // Raw G4Q images have the GameCube magic at 0x1c. The native reader checks
            // the full disc, including the contents of compressed CISO and GCZ images.
            if (count >= header.length) {
                boolean raw = header[0] == 'G' && header[1] == '4' && header[2] == 'Q'
                        && (header[3] == 'E' || header[3] == 'P' || header[3] == 'J')
                        && magic(header, 28) == 0xc2339f3d;
                int container = magic(header, 0);
                if (raw || container == 0x4349534f || container == 0x01c00bb1) return;
            }
            throw new IOException(context.getString(R.string.disc_format_unsupported));
        } catch (ErrnoException error) {
            throw new IOException(context.getString(R.string.disc_open_failed), error);
        }
    }

    private static int magic(byte[] header, int offset) {
        return (header[offset] & 255) << 24 | (header[offset + 1] & 255) << 16
                | (header[offset + 2] & 255) << 8 | (header[offset + 3] & 255);
    }

    static ParcelFileDescriptor open(Context context, Uri uri) throws IOException {
        ParcelFileDescriptor file = context.getContentResolver().openFileDescriptor(uri, "r");
        if (file == null) throw new IOException(context.getString(R.string.disc_open_failed));
        try {
            // Some document providers return a pipe. The DVD reader needs random access.
            Os.lseek(file.getFileDescriptor(), 0, OsConstants.SEEK_SET);
        } catch (ErrnoException error) {
            file.close();
            throw new IOException(context.getString(R.string.disc_requires_seek), error);
        }
        return file;
    }
}
