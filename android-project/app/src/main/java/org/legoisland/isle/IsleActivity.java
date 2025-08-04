package org.legoisland.isle;

import org.libsdl.app.SDLActivity;

public class IsleActivity extends SDLActivity {
    protected String[] getLibraries() {
        return new String[] { "SDL3", "lego1", "isle" };
    }
}
