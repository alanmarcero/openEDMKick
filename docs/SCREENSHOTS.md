# Native UI screenshot

![Native plugin editor](../assets/native-ui.png)

`assets/native-ui.png` shows the v1.0.0 native JUCE editor at 1100 × 880, captured directly from the release build. It displays **Festival 1/64 Drop Rush (8 Bar)** in **Auto Mode C**, with **LOCK IN (WAV)** active, **Beat 4 Auto** reverse sync enabled, the **Kicking Mule** stage mid-kick, and the playhead sweeping across the 1/64-note Piano Roll.

Reproduce on macOS from the repository root:

```sh
cmake --build build --target openedmkick_plugin_tests --parallel
OEK_CAPTURE="$PWD/assets/native-ui.png" build/openedmkick_plugin_tests '[ui]'
```
