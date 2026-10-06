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
#include "StatusPage.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "HttpServer.h"
#include "HttpHeader.h"
#include "DownloadEngine.h"
#include "SocketCore.h"
#include "Option.h"
#include "prefs.h"
#include "File.h"
#include "BufferedFile.h"
#include "DlAbortEx.h"
#include "LogFactory.h"
#include "Logger.h"
#include "base64.h"
#include "util.h"
#include "fmt.h"
#include "a2functional.h"

namespace aria2 {

namespace status_page {

const char CHALLENGE[] =
    "WWW-Authenticate: Basic realm=\"ariam\", charset=\"UTF-8\"\r\n";

namespace {
struct PageType {
  const char* extension;
  const char* contentType;
};

constexpr PageType PAGE_TYPES[] = {
    {"html", "text/html; charset=utf-8"},
    {"js", "text/javascript"},
    {"css", "text/css"},
    {"svg", "image/svg+xml"},
    {"png", "image/png"},
    {"ico", "image/x-icon"},
};

const PageType* findPageType(const std::string& fileName)
{
  auto dot = fileName.rfind('.');
  if (dot == std::string::npos) {
    return nullptr;
  }
  auto ext = fileName.c_str() + dot + 1;
  for (const auto& t : PAGE_TYPES) {
    if (strcmp(t.extension, ext) == 0) {
      return &t;
    }
  }
  return nullptr;
}

bool isLowerAlnum(char c)
{
  return ('a' <= c && c <= 'z') || ('0' <= c && c <= '9');
}
} // namespace

bool isPageFileName(const std::string& name)
{
  if (name.empty() || !isLowerAlnum(name[0])) {
    return false;
  }
  for (auto c : name) {
    if (!isLowerAlnum(c) && c != '.' && c != '_' && c != '-') {
      return false;
    }
  }
  if (name.find("..") != std::string::npos) {
    return false;
  }
  return findPageType(name) != nullptr;
}

bool matchPagePath(const std::string& path, std::string& fileName)
{
  if (path == "/") {
    fileName = "index.html";
    return true;
  }
  if (path.empty() || path[0] != '/') {
    return false;
  }
  auto name = path.substr(1);
  if (!isPageFileName(name)) {
    return false;
  }
  fileName = std::move(name);
  return true;
}

const char* getContentType(const std::string& fileName)
{
  auto t = findPageType(fileName);
  return t ? t->contentType : "application/octet-stream";
}

bool parseBasicPassword(const std::string& authorization,
                        std::string& password)
{
  auto p =
      util::divide(std::begin(authorization), std::end(authorization), ' ');
  if (!util::strieq(p.first.first, p.first.second, "Basic")) {
    return false;
  }
  auto userpass = base64::decode(p.second.first, p.second.second);
  auto colon = std::find(std::begin(userpass), std::end(userpass), ':');
  if (colon != std::end(userpass)) {
    ++colon;
  }
  password.assign(colon, std::end(userpass));
  return true;
}

PassphraseCheck checkBasicAuthorization(const HttpServer& server,
                                        DownloadEngine* e)
{
  std::string passphrase;
  if (!parseBasicPassword(
          server.getRequestHeader()->find(HttpHeader::AUTHORIZATION),
          passphrase)) {
    return PASSPHRASE_MISSING;
  }
  return e->checkStatusPagePassphrase(server.getSocket()->getPeerInfo().addr,
                                      passphrase);
}

namespace {
bool isJsonMediaType(const std::string& contentType)
{
  auto mediaType = util::stripIter(
      std::begin(contentType),
      std::find(std::begin(contentType), std::end(contentType), ';'));
  return util::strieq(mediaType.first, mediaType.second, "application/json");
}
} // namespace

bool acceptsPassphraseRpc(const HttpServer& server)
{
  if (!server.getStatusPage() || server.getRequestType() != RPC_TYPE_JSON ||
      server.getMethod() != "POST") {
    return false;
  }
  const auto& header = server.getRequestHeader();
  for (auto hdKey : {HttpHeader::CONTENT_TYPE, HttpHeader::X_ARIAM_PAGE,
                     HttpHeader::HTTP_HOST, HttpHeader::ORIGIN,
                     HttpHeader::SEC_FETCH_SITE}) {
    if (header->findAll(hdKey).size() > 1) {
      return false;
    }
  }
  if (!isJsonMediaType(header->find(HttpHeader::CONTENT_TYPE)) ||
      header->find(HttpHeader::X_ARIAM_PAGE) != "1") {
    return false;
  }
  const auto& host = header->find(HttpHeader::HTTP_HOST);
  if (host.empty()) {
    return false;
  }
  if (header->defined(HttpHeader::ORIGIN) &&
      header->find(HttpHeader::ORIGIN) != "https://" + host) {
    return false;
  }
  if (header->defined(HttpHeader::SEC_FETCH_SITE) &&
      header->find(HttpHeader::SEC_FETCH_SITE) != "same-origin") {
    return false;
  }
  return true;
}

namespace {
bool isIdentifierStart(char c)
{
  return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '_' ||
         c == '$';
}
} // namespace

bool acceptsJsonpCallback(const std::string& callback)
{
  if (callback.empty()) {
    return true;
  }
  if (callback.size() > MAX_JSONP_CALLBACK_LENGTH ||
      !isIdentifierStart(callback[0])) {
    return false;
  }
  return std::all_of(std::begin(callback), std::end(callback), [](char c) {
    return isIdentifierStart(c) || ('0' <= c && c <= '9') || c == '.';
  });
}

namespace {
bool readPageFile(const std::string& path, std::string& out)
{
  if (!File(path).isFile()) {
    return false;
  }
  BufferedFile fp(path.c_str(), BufferedFile::READ);
  if (!fp) {
    return false;
  }
  std::array<char, 4_k> buf;
  for (;;) {
    auto n = fp.read(buf.data(), buf.size());
    out.append(buf.data(), n);
    if (n < buf.size()) {
      break;
    }
  }
  return static_cast<bool>(fp);
}
} // namespace

void feedResponse(HttpServer& server, DownloadEngine* e)
{
  switch (checkBasicAuthorization(server, e)) {
  case PASSPHRASE_OK:
    break;
  case PASSPHRASE_LOCKED:
    server.feedStatusPageResponse(429);
    return;
  case PASSPHRASE_MISSING:
  case PASSPHRASE_WRONG:
    server.feedStatusPageResponse(401, CHALLENGE);
    return;
  }
  const auto& fileName = server.getStatusPageFile();
  auto path = e->getOption()->get(PREF_STATUS_PAGE_DIR) + "/" + fileName;
  std::string body;
  if (!readPageFile(path, body)) {
    A2_LOG_INFO(fmt("Status page file %s cannot be read.", path.c_str()));
    server.feedStatusPageResponse(404);
    return;
  }
  server.feedStatusPageResponse(200, "", std::move(body),
                                getContentType(fileName));
}

void validateOptions(const Option& option)
{
  if (option.blank(PREF_STATUS_PAGE_PASSPHRASE)) {
    return;
  }
  if (!option.getAsBool(PREF_RPC_SECURE)) {
    throw DL_ABORT_EX("--status-page-passphrase requires --rpc-secure=true:"
                      " the passphrase would cross the network in plaintext.");
  }
  if (option.getAsBool(PREF_RPC_ALLOW_ORIGIN_ALL)) {
    throw DL_ABORT_EX("--status-page-passphrase cannot be used with"
                      " --rpc-allow-origin-all=true.");
  }
  if (option.blank(PREF_RPC_SECRET)) {
    throw DL_ABORT_EX("--status-page-passphrase requires --rpc-secret.");
  }
  if (!option.blank(PREF_RPC_USER)) {
    throw DL_ABORT_EX("--status-page-passphrase cannot be used with"
                      " --rpc-user.");
  }
}

} // namespace status_page

} // namespace aria2
