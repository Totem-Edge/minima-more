# Full-node comparison (solo, fresh private chain)

C++ run: this node | Java run: minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| genesis creation (s) | 5.911 | 4.575 |
| daemon ready (s) | 5.918 | 4.583 |
| peak RSS (kB) | 19964 | 301540 |
| idle RSS (kB) | 18676 | 301540 |
| threads | 23 | 52 |
| data dir (kB) | 468 | 212 |

Logs: cpp_node.log, java_node.log
