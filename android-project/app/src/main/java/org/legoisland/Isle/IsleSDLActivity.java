package org.legoisland.Isle;

import org.libsdl.app.SDLActivity;

public class IsleSDLActivity extends SDLActivity {
    protected String[] getLibraries() {
        return new String[] {
            "SDL3",
            "lego1",
            "isle"
        };
    }
}
