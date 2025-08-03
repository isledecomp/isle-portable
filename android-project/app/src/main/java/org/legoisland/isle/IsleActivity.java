package org.legoisland.Isle;

import org.libsdl.app.SDLActivity;

public class HelloWorldActivity extends SDLActivity {
    protected String[] getLibraries() {
        return new String[] { "SDL3", "lego1", "isle" };
    }
}
