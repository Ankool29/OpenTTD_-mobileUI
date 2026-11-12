package org.ankool29.openttdmobile;

import org.libsdl.app.SDLActivity;

/**
 * SDL entry activity for OpenTTD Mobile.
 *
 * SDLActivity loads libSDL2.so first, then libmain.so which contains the
 * OpenTTD entry point (SDL_main).
 */
public class OpenTTDActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
