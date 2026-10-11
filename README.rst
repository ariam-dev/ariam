ariam
=====

ariam is a personal fork of `aria2 <https://github.com/aria2/aria2>`_
1.37.0, a modified version of aria2 built for one use: a headless
download daemon on a home server that a browser hands downloads to,
with per-site download limits and a passphrase-gated status page on
the LAN. The browser side is the Firefox extension `ariam connect
<https://github.com/ariam-dev/ariam-connect>`_.

ariam downloads over HTTP(S) and FTP. BitTorrent, Metalink and
WebSocket RPC are off by default; enabling them at build time is
unsupported. For a general-purpose downloader, use aria2.

The program is still ``aria2c``, and its options, configuration and
JSON-RPC interface are aria2's (see the `aria2 manual
<https://aria2.github.io/manual/en/html/>`_), with the additions and
changes below. This program comes with no warranty.

Changes from aria2
------------------

Fixes from elsewhere:

* Build with gettext 0.23 (Debian, `#1092222
  <https://bugs.debian.org/1092222>`_).
* gnutls: require the server-auth ExtendedKeyUsage when verifying a
  peer certificate (upstream `44aa33d
  <https://github.com/aria2/aria2/commit/44aa33d40901b0f3751231c8029b6743cf31f789>`_,
  after 1.37.0).
* Stack buffer underflow in ``IOFile::getLine`` (`aria2#2376
  <https://github.com/aria2/aria2/pull/2376>`_).
* Busy loop while an unauthorized RPC response is delayed (`aria2#2209
  <https://github.com/aria2/aria2/pull/2209>`_, amended).

ariam's own changes:

* `Per-site concurrency`_ limits, and a per-download ``site-hint``
  option.
* A `status page`_ served on the RPC port.
* ``Cookie:`` and ``Authorization:`` lines in a download's ``header``
  option are sent only to the origin of the URI the download was given,
  not to the origins it redirects to.
* Pooled HTTP(S) connections are reused only for the scheme and host
  they were opened for (`aria2#2073
  <https://github.com/aria2/aria2/issues/2073>`_).
* Control files are ``<name>.ariam``. A ``<name>.aria2`` file is an
  ordinary file to ariam. A download is never named like a control
  file: one whose name ends in ``.ariam`` or ``.ariam__temp`` (in any
  case) is saved as ``<name>_``.
* ``--save-session`` keeps unfinished downloads only: a download that
  stopped with an error is not re-queued on the next start.
  ``--save-not-found`` has no effect.
* The session records each download's resolved file name, and no
  download is renamed onto or resumes into a file another download
  owns.
* ``Content-Disposition`` with a trailing semicolon is accepted
  (`aria2#1118 <https://github.com/aria2/aria2/issues/1118>`_).
* URIs longer than 65535 bytes are rejected instead of terminating the
  process.
* Out-of-bounds reads in base64 decoding of bytes ``0x80`` and above,
  and an overflow in RPC pagination, are fixed.
* Logs show request URIs as their origin, without path or query.

Per-site concurrency
--------------------

A download's site comes from, first non-empty: its ``site-hint``
option, a ``Referer:`` line in its ``header`` option, the ``referer``
option. The download's own URIs never decide its site. A download with
no source, or a source with no host, belongs to one shared site,
``(none)``.

The host is lowercased and a trailing dot stripped; IP literals are
their own site. A listed site ``example.com`` matches ``example.com``
and every subdomain, and the longest matching entry wins. An unlisted
host's site is its registrable domain per the Public Suffix List
(``members.example.co.uk`` is ``example.co.uk``).

``site-concurrent``
  Listed sites, ``host:N[,host:N...]``, N of 1 or more.
``site-concurrent-default``
  N for each unlisted site, each counted separately; ``0`` means no
  per-site limit.
``max-concurrent-downloads``
  The ceiling across all sites.
``site-concurrent-file``
  Where live changes to the two site options are saved. On start, the
  file overrides the configuration file and the command line.
``site-hint``
  Per download (``aria2.addUri`` options): a URI of the page the
  download was started from. Only its host is read; it is never sent
  to a server and cannot be set globally.

Waiting downloads start oldest first. A download whose site is at its
limit is skipped and keeps its queue position. A paused download
releases its slot. ``aria2.changeGlobalOption`` with either site option
applies at once and reclassifies queued downloads.

RPC additions, token-authorized like any method:

* ``site`` and ``name`` keys on every ``aria2.tellActive``,
  ``tellWaiting``, ``tellStopped`` and ``tellStatus`` entry.
* ``ariam.getSiteSummary`` returns ``{default, ceiling, sites: [{site,
  listed, limit, active, waiting, paused}]}``.

Status page
-----------

With ``status-page-passphrase`` set, the RPC port serves the page in
``www/`` (installed to ``$(datadir)/ariam/www``, see
``status-page-dir``) at ``/``. Sign in with HTTP Basic Auth: any user
name, the passphrase as the password. ariam refuses to start with a
passphrase unless ``rpc-secure=true`` and ``rpc-secret`` are set,
``rpc-allow-origin-all`` is off and ``rpc-user`` is unset.

The passphrase allows same-origin ``POST /jsonrpc`` calls that carry
``X-Ariam-Page: 1`` to a fixed set of methods: reading the queue,
``getGlobalStat`` and ``ariam.getSiteSummary``; pause, unpause, remove
and purge; and changing only the two site options. It cannot add
downloads, read per-download options, see URIs or error messages, or
change any other option, and the RPC secret never reaches the browser.
Failed passphrase checks are rate limited per client address. The page
sends a strict Content Security Policy and no inline script.

On an instance with the page enabled, a JSONP request whose
``jsoncallback`` is not a plain identifier is refused.

Building
--------

From a git checkout you need autoconf, automake, autopoint, libtool,
pkg-config and a C++11 compiler, plus development packages for GnuTLS
and Nettle (or OpenSSL), c-ares, zlib and libpsl; CppUnit runs the
tests. Then::

    $ autoreconf -i
    $ ./configure --with-ca-bundle=/etc/ssl/certs/ca-certificates.crt
    $ make
    $ make check
    $ make install

The CA bundle path varies by system; see aria2's `build notes
<https://github.com/aria2/aria2#how-to-build>`_ for the details and
other platforms.

Contributing and security
-------------------------

Bugs and requests go to `issues
<https://github.com/ariam-dev/ariam/issues>`_; see `CONTRIBUTING.md
<CONTRIBUTING.md>`_ and, for vulnerabilities, `SECURITY.md
<SECURITY.md>`_. Changes are listed in `NEWS <NEWS>`_.

License
-------

ariam is distributed under the GNU General Public License, version 2
or later, with the OpenSSL linking exception, as aria2 is: see
`COPYING <COPYING>`_ and `LICENSE.OpenSSL <LICENSE.OpenSSL>`_. The
status page in ``www/`` is MIT licensed (`www/LICENSE
<www/LICENSE>`_). aria2 is copyright Tatsuhiro Tsujikawa and its
contributors; source files ariam modified say so in their headers.
