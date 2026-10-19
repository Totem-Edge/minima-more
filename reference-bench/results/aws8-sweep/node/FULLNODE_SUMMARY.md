# Full-node comparison (solo, fresh private chain)

C++ run: this node | Java run: minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| genesis creation (s) | 3.203 | 5.159 |
| daemon ready (s) | 3.211 | 5.167 |
| peak RSS (kB) | 19084 | 223868 |
| idle RSS (kB) | 18636 | 223868 |
| threads | 23 | 53 |
| data dir (kB) | 404 | 200 |

Logs: cpp_node.log, java_node.log
