# ESP-IDF firmware target

This directory is reserved for the production ESP-IDF build once the SSH component is selected.

Expected build commands:

```text
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

Do not copy `sdkconfig`, build output, device keys, or credentials into the repository until the project explicitly defines which parts are portable.
