/* The browser SDL shim must understand the shared controls' saved key names. */
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const struct { const char *name; SDL_Scancode code; } bindings[] = {
        {"W", SDL_SCANCODE_W}, {"Space", SDL_SCANCODE_SPACE},
        {"Left Ctrl", SDL_SCANCODE_LCTRL}, {"Escape", SDL_SCANCODE_ESCAPE},
        {"Tab", SDL_SCANCODE_TAB}, {"1", SDL_SCANCODE_1},
        {"F12", SDL_SCANCODE_F12}, {"Return", SDL_SCANCODE_RETURN},
        {"Keypad Enter", SDL_SCANCODE_KP_ENTER}, {"Left", SDL_SCANCODE_LEFT}
    };
    for (unsigned i = 0; i < sizeof(bindings)/sizeof(bindings[0]); i++) {
        assert(SDL_GetScancodeFromName(bindings[i].name) == bindings[i].code);
        assert(!strcmp(SDL_GetScancodeName(bindings[i].code), bindings[i].name));
    }
    assert(SDL_GetScancodeFromName("left ctrl") == SDL_SCANCODE_LCTRL);
    assert(SDL_GetScancodeFromName(NULL) == SDL_SCANCODE_UNKNOWN);
    assert(SDL_GetScancodeFromName("") == SDL_SCANCODE_UNKNOWN);
    assert(SDL_GetScancodeFromName("not a key") == SDL_SCANCODE_UNKNOWN);
    assert(!*SDL_GetScancodeName((SDL_Scancode)-1));
    assert(!*SDL_GetScancodeName(SDL_SCANCODE_COUNT));
    for (int i = 0; i < SDL_SCANCODE_COUNT; i++) {
        const char *name = SDL_GetScancodeName((SDL_Scancode)i);
        if (*name) assert(!strcmp(name, SDL_GetScancodeName(SDL_GetScancodeFromName(name))));
    }
    puts("Browser control bindings and SDL key-name round trips passed.");
}
