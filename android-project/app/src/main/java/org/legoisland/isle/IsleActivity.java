package org.legoisland.isle;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.util.Log;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

import org.libsdl.app.SDLActivity;

public class IsleActivity extends SDLActivity {
    private static final String TAG = "IsleActivity";

    protected String[] getLibraries() {
        return new String[] { "SDL3", "lego1", "isle" };
    }

    /**
     * Called from native code (see ISLE/android/filepicker.cpp); kept by proguard-rules.pro.
     * Copies the game files from the document tree selected by the user into the app's
     * external files directory, so they are owned and readable by this app. Returns the
     * root directory the files were imported into, or null on failure.
     */
    public String importGameFiles(String treeUri) {
        try {
            showToast("Copying game files. This may take a while...");

            Uri tree = Uri.parse(treeUri);
            ContentResolver resolver = getContentResolver();
            String rootId = DocumentsContract.getTreeDocumentId(tree);
            boolean bareGameRoot = !containsGameDirectory(resolver, tree, rootId);
            File filesDir = getExternalFilesDir(null);

            String destRoot = tryImport(resolver, tree, rootId, bareGameRoot, filesDir);
            if (destRoot == null) {
                // Unreadable leftovers (copied by a privileged intermediary with foreign
                // ownership) can block the default location and cannot be deleted or
                // renamed by this app; import into a fresh directory instead.
                File fallback = new File(filesDir, "imported-" + System.currentTimeMillis());
                destRoot = tryImport(resolver, tree, rootId, bareGameRoot, fallback);
            }

            showToast(destRoot != null ? "Game files copied" : "Copying game files failed");
            return destRoot;
        } catch (Exception e) {
            Log.e(TAG, "Failed to import game files from " + treeUri, e);
            showToast("Copying game files failed");
            return null;
        }
    }

    private String tryImport(ContentResolver resolver, Uri tree, String rootId, boolean bareGameRoot, File root) {
        File dest = bareGameRoot ? new File(root, "LEGO") : root;
        if (copyDocumentTree(resolver, tree, rootId, dest)) {
            return root.getAbsolutePath();
        }
        return null;
    }

    private boolean containsGameDirectory(ContentResolver resolver, Uri tree, String rootId) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, rootId);
        String[] columns = {
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };

        Cursor cursor = resolver.query(children, columns, null, null, null);
        if (cursor == null) {
            return false;
        }

        try {
            while (cursor.moveToNext()) {
                String name = cursor.getString(0);
                String mime = cursor.getString(1);
                if (name != null && DocumentsContract.Document.MIME_TYPE_DIR.equals(mime) &&
                        name.toLowerCase(Locale.ROOT).startsWith("lego")) {
                    return true;
                }
            }
        } finally {
            cursor.close();
        }

        return false;
    }

    private boolean copyDocumentTree(ContentResolver resolver, Uri tree, String docId, File dest) {
        if (dest.exists() && (!dest.isDirectory() || dest.list() == null || !dest.canWrite())) {
            // An unreadable leftover (e.g. copied by a privileged intermediary with foreign
            // ownership) cannot be deleted by this app, but it can be renamed aside.
            File moved = new File(dest.getParent(), dest.getName() + ".unreadable." + System.currentTimeMillis());
            if (!dest.renameTo(moved)) {
                Log.e(TAG, "Failed to move unreadable " + dest + " aside");
                return false;
            }
            Log.w(TAG, "Moved unreadable " + dest + " aside to " + moved);
        }

        if (!dest.isDirectory() && !dest.mkdirs()) {
            Log.e(TAG, "Failed to create directory " + dest);
            return false;
        }

        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        String[] columns = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };

        Cursor cursor = resolver.query(children, columns, null, null, null);
        if (cursor == null) {
            Log.e(TAG, "Failed to list children of " + docId);
            return false;
        }

        boolean copied = true;
        try {
            while (cursor.moveToNext()) {
                String childId = cursor.getString(0);
                String name = cursor.getString(1);
                String mime = cursor.getString(2);

                if (name == null || name.startsWith(".")) {
                    continue;
                }

                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                    copied &= copyDocumentTree(resolver, tree, childId, new File(dest, name));
                }
                else {
                    Uri document = DocumentsContract.buildDocumentUriUsingTree(tree, childId);
                    copied &= copyDocumentFile(resolver, document, new File(dest, name));
                }
            }
        } finally {
            cursor.close();
        }

        return copied;
    }

    private boolean copyDocumentFile(ContentResolver resolver, Uri document, File dest) {
        try (InputStream in = resolver.openInputStream(document);
                OutputStream out = new FileOutputStream(dest)) {
            if (in == null) {
                Log.e(TAG, "Failed to open " + document);
                return false;
            }

            byte[] buffer = new byte[1 << 16];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }

            Log.i(TAG, "Copied " + document + " to " + dest);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Failed to copy " + document + " to " + dest, e);
            return false;
        }
    }

    private void showToast(final String message) {
        runOnUiThread(() -> Toast.makeText(this, message, Toast.LENGTH_LONG).show());
    }
}
