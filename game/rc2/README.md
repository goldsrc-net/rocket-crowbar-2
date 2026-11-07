# rc2/ - mod content directory

The runtime game directory for mod-rc2, run next to the stock `valve/` directory
with `-game rc2`.

```
rc2/
  liblist.gam      # mod manifest (this repo)
  dlls/hl.so       # server game library built from this repo
  cl_dlls/client.so  # client library built from this repo (RC2 has its own)
  ...              # maps, models, sprites, sounds, events, rc2.ini; everything
                   #   not here falls back to valve/
```

The original's content is in the beta 1.1 installer; see `../../reference/README.md`.
