# Full-node comparison (solo, fresh private chain)

C++ run: this node | Java run: minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| genesis creation (s) | 1.992 | 3.185 |
| daemon ready (s) | 2.002 | 3.195 |
| peak RSS (kB) | 14288 | 271808 |
| idle RSS (kB) | 14288 | 271808 |
| threads | 23 | 59 |
| data dir (kB) | 504 | 184 |

Logs: cpp_node.log, java_node.log
