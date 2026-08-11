# Pinned third-party components

The SSH production build uses official wolfSSL components pinned as Git
submodules:

- wolfSSL v5.9.2 stable: `ac01707f552c611fbd135cc723b2682b3e7f80f2`
- wolfSSH v1.5.0 stable: `8643d7be841184f766374e3b0ed68ced6391543c`

Initialize them after cloning:

```powershell
git submodule update --init --depth 1
```

The Arduino bring-up build keeps wolfSSH disabled until the ESP-IDF
production target supplies the component configuration and server adapter.
