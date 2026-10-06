/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 dta0one
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#ifndef D_STATUS_PAGE_H
#define D_STATUS_PAGE_H

#include "common.h"

#include <string>

#include "PassphraseLimiter.h"

namespace aria2 {

class HttpServer;
class DownloadEngine;
class Option;

// The passphrase-gated status page served on the RPC port when
// PREF_STATUS_PAGE_PASSPHRASE is set.
namespace status_page {

// Sent with every 401 of a status page route.
extern const char CHALLENGE[];

// True if name matches ^[a-z0-9][a-z0-9._-]*\.(html|js|css|svg|png|ico)$
// and holds no "..".
bool isPageFileName(const std::string& name);

// True if path, the raw request path without query and fragment, is a
// status page route; stores the file it serves in fileName: "/" serves
// "index.html", "/<name>" serves name if isPageFileName(name). A
// percent-encoded byte never matches.
bool matchPagePath(const std::string& path, std::string& fileName);

const char* getContentType(const std::string& fileName);

// Stores in password the password of the Basic credentials in
// authorization, an Authorization field value: everything after the
// first ':' of the decoded credentials, or empty if there is none. Any
// user name is accepted. Returns false, leaving password unchanged, if
// the scheme is not Basic.
bool parseBasicPassword(const std::string& authorization,
                        std::string& password);

// Checks the Basic credentials of the last request of server, keyed by
// the peer address of server.
PassphraseCheck checkBasicAuthorization(const HttpServer& server,
                                        DownloadEngine* e);

// True if the status page passphrase may authorize the last request of
// server, decided before the passphrase is checked: the status page
// routes exist; it is a POST /jsonrpc JSON-RPC request (never JSONP,
// XML-RPC, or WebSocket); the media type of Content-Type is
// application/json, case-insensitive, parameters ignored; X-Ariam-Page
// is 1; Host is not empty; Origin, if present, is exactly "https://"
// followed by Host; Sec-Fetch-Site, if present, is same-origin. Each of
// these header fields appears at most once.
bool acceptsPassphraseRpc(const HttpServer& server);

constexpr size_t MAX_JSONP_CALLBACK_LENGTH = 128;

// True if callback, the jsoncallback of a JSONP request, may be served
// while the status page routes exist: empty, or at most
// MAX_JSONP_CALLBACK_LENGTH bytes matching ^[A-Za-z_$][A-Za-z0-9_$.]*$.
// Any other callback would make the response a same-origin script that
// the page's script-src 'self' runs.
bool acceptsJsonpCallback(const std::string& callback);

// Answers a STATUS_PAGE_TYPE request: 401 with CHALLENGE if the
// passphrase is missing or wrong, 429 if the client is locked out, 404
// if the file cannot be read from PREF_STATUS_PAGE_DIR, otherwise 200
// with the file.
void feedResponse(HttpServer& server, DownloadEngine* e);

// Throws DlAbortEx naming the conflicting option if
// PREF_STATUS_PAGE_PASSPHRASE is set and PREF_RPC_SECURE is false,
// PREF_RPC_ALLOW_ORIGIN_ALL is true, PREF_RPC_SECRET is not set, or
// PREF_RPC_USER is set.
void validateOptions(const Option& option);

} // namespace status_page

} // namespace aria2

#endif // D_STATUS_PAGE_H
