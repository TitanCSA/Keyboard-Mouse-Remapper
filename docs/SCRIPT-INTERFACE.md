# Script-defined settings

A `ui` block defines the controls in **Use script**. Parameter names are read-only
integers available in the script and macros.

```c
ui {
    title("My settings");
    description("Adjust the behavior without editing code.");
    group("Mouse");
    toggle enabled("Enable processing", true);
    slider speed("Sensitivity", 10, 300, 100, "%");
    number delay("Pulse duration", 10, 500, 80, 5, "ms");
    select mode("Mode", 0) {
        0: "Normal";
        1: "Invert Y";
    };
    help("Changes apply to the running script.");
}
main {
    if (enabled) sensitivity(MOUSE_X, speed);
    if (mode == 1) invert(MOUSE_Y);
}
```

Slider arguments are label, minimum, maximum, default and optional unit.
A number can additionally specify its step before its unit. A select declares
its default value in parentheses and its value/label choices in a block.
Groups and help text arrange the controls without adding executable functions.

Changes publish to the worker between input frames. Globals, timing history and
running combos are preserved. Replacing the script starts a new instance.
**Reset settings** restores declared defaults.

Settings are saved separately in `.igc.settings.json`; a user-profile fallback
supports read-only script folders. Parameter values are validated against the
compiled schema before use. Presets and runtime values never modify script code.
Up to 64 parameters are supported.
