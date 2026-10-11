# Contributing

ariam is a personal fork of aria2, kept for one use (see the README).
Bug reports and small fixes within that scope are welcome; features
outside it, including BitTorrent, Metalink and WebSocket, belong in
[aria2](https://github.com/aria2/aria2).

- Open an [issue](https://github.com/ariam-dev/ariam/issues) before a
  larger change.
- A pull request targets `main`, one change per commit, with a test
  where the code has one (`make check`).
- Follow the surrounding code; C++ is formatted with the repository's
  `.clang-format`.

Contributions are accepted under the license of the files they change:
GPL-2.0-or-later with the OpenSSL linking exception (`COPYING`,
`LICENSE.OpenSSL`), or MIT for `www/`. There is no contributor license
agreement.

Report security issues privately; see [SECURITY.md](SECURITY.md).
