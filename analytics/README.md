# minima-analytics (optional sidecar)

Build (off by default):

```bash
cmake -S . -B build-analytics -DCMAKE_BUILD_TYPE=Release -DMINIMA_BUILD_ANALYTICS=ON
cmake --build build-analytics --target minima-analytics -j8
```

Run against a node data folder:

```bash
./build-analytics/analytics/minima-analytics -data ~/.minima -apiport 8080
curl -s http://127.0.0.1:8080/v1/capabilities | jq
curl -s http://127.0.0.1:8080/v1/catalog | jq '.metrics | length'
```

Overrides: `-archivepath`, `-mmrpath`, `-archive`, `-megammr`.
The sidecar opens the archive SQLite DB **read-only** and never touches keys or the wallet.
