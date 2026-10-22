# IGC keyboard and mouse language

Scripts compile once to bounded bytecode. One worker owns the running instance:
keyboard and mouse devices share a combined physical frame, variables, parameters
and macro state. Held inputs from multiple physical devices are combined when a
script is active. Without a script, original strokes pass through unchanged.

## Controls and units

| Controls | Meaning |
| --- | --- |
| `KEY_A..KEY_Z`, `KEY_0..KEY_9` | Physical US scan-code positions |
| `KEY_LCTRL`, `KEY_RCTRL`, `KEY_LSHIFT`, `KEY_RSHIFT`, `KEY_LALT`, `KEY_RALT` | Separate left/right modifiers |
| `KEY_ENTER`, `KEY_SPACE`, `KEY_ESC`, `KEY_F1..KEY_F24` | Named physical keys |
| `SCAN_00..SCAN_FF`, `E0_00..E0_FF`, `E1_00..E1_FF` | Unextended, E0 and E1 scan positions |
| `MOUSE_LEFT`, `MOUSE_RIGHT`, `MOUSE_MIDDLE`, `MOUSE_X1`, `MOUSE_X2` | Five mouse buttons |
| `MOUSE_X`, `MOUSE_Y` | Signed relative movement counts in this event |
| `MOUSE_WHEEL`, `MOUSE_HWHEEL` | Signed wheel units, commonly 120 per notch |
| `ABS_X`, `ABS_Y` | Absolute reports, in 0..65535 coordinates |

Buttons use 0 (released) or 100 (pressed). Relative movement is an impulse: it
returns to zero for unrelated events and timers. A macro's relative movement step
is emitted once; it is not replayed as a held gamepad axis. Absolute coordinates
remain distinct from relative movement. Unknown scan extensions, metadata and
unmodified fields are preserved.

Names are case-insensitive when resolved as control constants; language keywords
and variables remain case-sensitive. Key labels are not text composition or IME
commands. A script writing a key creates physical key events using that scan code.

## Code

Supported: signed 32-bit integers, defines, constants, fixed arrays, globals,
locals, functions, `init`, `main`, `if/else`, `for`, `while`,
`break`, `continue`, `return`, arithmetic, bitwise operators and short-circuit
conditions. Strings are allowed in `ui` declarations, not executable code.

```c
combo pulse {
    set_val(KEY_C, 100);
    wait(30);
    set_val(KEY_C, 0);
}
main {
    if (event_press(MOUSE_X2) && get_ival(KEY_LCTRL)) combo_run(pulse);
}
```

`wait` suspends a combo without blocking physical input. Macro deadlines use an
event-driven timer. Windows scheduling can delay a deadline; input capture has no
fixed frame-rate limit or script polling loop.

## Functions

| Functions | Purpose |
| --- | --- |
| `get_val(c)`, `get_ival(c)`, `get_lval(c)` | Current output, physical input, previous physical input |
| `set_val(c,v)`, `add_val(c,v)` | Set or add output using bounded native ranges |
| `remap(a,b)` | A produces B while physical B remains usable |
| `swap(a,b)`, `invert(c)` | Exchange controls or negate movement |
| `sensitivity(c,percent)` | Scale movement; fractional raw counts are retained |
| `deadzone(c,counts[,second])` | Zero movement whose magnitude is within a raw-count threshold |
| `block(c,ms)` | Suppress a control for a bounded duration |
| `event_press(c)`, `event_release(c)` | Physical button transitions |
| `double_tap(c,windowMs)`, `get_ptime(c)`, `get_brtime(c)` | Trigger timing and hold/release duration |
| `get_rtime()`, `get_runtime()`, `get_phase(periodMs)` | Frame interval, elapsed time and periodic phase |
| `combo_run(name)`, `combo_stop(name)`, `combo_restart(name)`, `combo_running(name)`, `combo_stop_all()` | Timed macro lifecycle |
| `tick(ms)`, `vm_tctrl(ms)` | Request timed evaluations when a script needs them |
| `is_available(c)` | Whether an applicable output device exists |
| `get_family()`, `get_device()` | Combined script identity: family 0, logical device 1 |
| `abs`, `min`, `max`, `clamp`, `rand`, `random` | Integer helpers |
| `sin`, `cos`, `wave_sin`, `wave_cos`, `atan2`, `lerp` | Motion and interpolation |
| `pow`, `isqrt`, `hypot`, `get_bit`, `set_bit` | Integer math |
| `get_pvar`, `set_pvar` | Instance-local persistent variables, not disk storage |

Cross-family actions need an endpoint of the destination type. Remap is inactive
when either endpoint is unavailable. Timed relative motion can use a mouse;
keyboard actions use a keyboard. The inspector identifies the actual destination.

## Limits and failure handling

A frame has a shared 10,000-instruction budget. There are at most 32 combos,
64 interface parameters, 4,096 global integers, 256 locals per function,
32 nested calls and 1,024 expression stack entries. Files are limited to 1 MiB;
syntax, bytecode, arrays and visual metadata are bounded as well.

Invalid arguments, division by zero, out-of-range indices and exhausted budgets
fault the script. The worker removes the faulty script and restores physical
input. Transport failures close capture so the driver's recovery can run.
Security revocation is not automatically reacquired.

## Visual editing

Visual view supports remaps, inversion, thresholds, sensitivity, blocking,
nested ALL/ANY/NOT triggers, holds, double taps, sequences and mouse motion.
Its marked generated section round-trips with the script. Custom code remains
visible and preserved; arbitrary code is not claimed to be fully represented by
visual cards. Parameter declarations are described in [script settings](SCRIPT-INTERFACE.md).
