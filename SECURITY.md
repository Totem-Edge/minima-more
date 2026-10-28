# Security Policy

## Supported versions

Security fixes are provided for the latest `0.5.x` release line. Older releases
and unmaintained forks are not supported.

## Reporting a vulnerability

Do **not** open a public issue for a suspected vulnerability.

Use GitHub private vulnerability reporting:

<https://github.com/Totem-Edge/minima-more/security/advisories/new>

Please include:

- affected version/commit and node mode (`full`, `-archive`, `-megammr`,
  `minima-analytics`);
- a minimal reproduction (command, script, or transaction) and expected vs
  actual behaviour;
- impact assessment, especially anything affecting consensus, script
  validation, contract addresses, wallet keys, archive integrity, or remote
  crash/DoS;
- whether the issue is already public.

We aim to acknowledge reports within 3 business days, provide a triage decision
within 10 business days, and coordinate disclosure after a fix is available.
These are targets, not contractual SLAs.

## Scope

In scope: consensus/TxPoW validation, KISSVM script execution and formatting,
WOTS/Winternitz, wallet/keys, P2P, archive/MegaMMR, SQLite/DuckDB persistence,
and the optional analytics API (including SSRF, injection, and media fetching).

Out of scope: vulnerabilities in third-party dependencies that are already
public and fixed upstream (report them upstream), social engineering, and
issues requiring a compromised host or wallet seed.

## Consensus issues

A behaviour that can produce a different block hash, script result, contract
address, or accepted/rejected transaction is treated as **security-critical**
and handled with the same private reporting process.
