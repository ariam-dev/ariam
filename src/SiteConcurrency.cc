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
#include "SiteConcurrency.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include <libpsl.h>

#include "Option.h"
#include "DownloadEngine.h"
#include "prefs.h"
#include "util.h"
#include "uri.h"
#include "fmt.h"
#include "DlAbortEx.h"

namespace aria2 {

const std::string SITE_NONE = "(none)";

namespace {

bool isLdhChar(char c)
{
  return ('a' <= c && c <= 'z') || ('0' <= c && c <= '9') || c == '-';
}

// Returns true if host is a lowercase LDH domain name.
bool isDomainName(const std::string& host)
{
  if (host.empty() || host.size() > 253) {
    return false;
  }
  size_t first = 0;
  for (;;) {
    auto dot = host.find('.', first);
    auto last = dot == std::string::npos ? host.size() : dot;
    if (first == last || last - first > 63 || host[first] == '-' ||
        host[last - 1] == '-' ||
        !std::all_of(host.begin() + first, host.begin() + last, isLdhChar)) {
      return false;
    }
    if (dot == std::string::npos) {
      return true;
    }
    first = dot + 1;
  }
}

bool isIpv6Char(char c)
{
  return ('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || c == ':' ||
         c == '.';
}

bool isIpv6Literal(const std::string& host)
{
  return host.find(':') != std::string::npos &&
         std::all_of(host.begin(), host.end(), isIpv6Char) &&
         util::isNumericHost(host);
}

void validateSite(const std::string& site)
{
  if (std::any_of(site.begin(), site.end(),
                  [](char c) { return 'A' <= c && c <= 'Z'; })) {
    throw DL_ABORT_EX(fmt("Site '%s' must be lowercase.", site.c_str()));
  }
  if (!isDomainName(site) && !isIpv6Literal(site)) {
    throw DL_ABORT_EX(fmt("Site '%s' is not a domain name or IP address.",
                          site.c_str()));
  }
}

std::string getUriHost(const std::string& uri)
{
  uri::UriStruct us;
  if (!uri::parse(us, uri)) {
    return std::string();
  }
  return SiteConcurrency::normalizeHost(us.host);
}

// Returns true if host is site or a subdomain of site.
bool domainMatch(const std::string& host, const std::string& site)
{
  if (host.size() == site.size()) {
    return host == site;
  }
  return host.size() > site.size() &&
         host[host.size() - site.size() - 1] == '.' &&
         util::endsWith(host, site);
}

} // namespace

std::string siteOfHost(const std::string& normalizedHost)
{
  if (normalizedHost.empty() || util::isNumericHost(normalizedHost)) {
    return normalizedHost;
  }
  // psl_builtin() is a static context, and the result points into the
  // argument.
  const char* domain =
      psl_registrable_domain(psl_builtin(), normalizedHost.c_str());
  if (!domain) {
    return normalizedHost;
  }
  return domain;
}

SiteConcurrency::SiteConcurrency(const DownloadEngine& e)
    : SiteConcurrency(parseSites(e.getOption()->get(PREF_SITE_CONCURRENT)),
                      e.getOption()->getAsInt(PREF_SITE_CONCURRENT_DEFAULT))
{
}

SiteConcurrency::SiteConcurrency(std::vector<SiteLimit> sites,
                                 int defaultLimit)
    : sites_(std::move(sites)), defaultLimit_(std::max(defaultLimit, 0))
{
  for (const auto& s : sites_) {
    ipLiteral_.push_back(util::isNumericHost(s.site));
  }
}

SiteMatch SiteConcurrency::classify(const Option& downloadOption) const
{
  return matchHost(getRefererHost(downloadOption));
}

SiteMatch SiteConcurrency::matchHost(const std::string& host) const
{
  auto h = normalizeHost(host);
  if (h.empty()) {
    return SiteMatch{SITE_NONE, false, defaultLimit_};
  }
  bool ip = util::isNumericHost(h);
  const SiteLimit* best = nullptr;
  for (size_t i = 0; i < sites_.size(); ++i) {
    const auto& s = sites_[i];
    bool match =
        (ip || ipLiteral_[i]) ? h == s.site : domainMatch(h, s.site);
    if (match && (!best || s.site.size() > best->site.size())) {
      best = &s;
    }
  }
  if (best) {
    return SiteMatch{best->site, true, best->limit};
  }
  return SiteMatch{siteOfHost(h), false, defaultLimit_};
}

std::vector<SiteLimit> SiteConcurrency::parseSites(const std::string& value)
{
  std::vector<SiteLimit> sites;
  auto v = util::strip(value);
  if (v.empty()) {
    return sites;
  }
  std::vector<std::string> items;
  util::split(std::begin(v), std::end(v), std::back_inserter(items), ',',
              /* doStrip = */ true, /* allowEmpty = */ true);
  for (const auto& item : items) {
    auto colon = item.rfind(':');
    if (colon == std::string::npos) {
      throw DL_ABORT_EX(
          fmt("Bad site entry '%s'. Expected HOST:N.", item.c_str()));
    }
    auto site = item.substr(0, colon);
    auto n = item.substr(colon + 1);
    validateSite(site);
    uint32_t limit;
    if (!util::isNumber(std::begin(n), std::end(n)) ||
        !util::parseUIntNoThrow(limit, n) || limit < 1) {
      throw DL_ABORT_EX(fmt("Limit of site '%s' must be an integer greater "
                            "than or equal to 1.",
                            site.c_str()));
    }
    if (std::any_of(sites.begin(), sites.end(),
                    [&site](const SiteLimit& s) { return s.site == site; })) {
      throw DL_ABORT_EX(fmt("Site '%s' is listed twice.", site.c_str()));
    }
    sites.push_back(SiteLimit{site, static_cast<int>(limit)});
  }
  return sites;
}

std::string SiteConcurrency::formatSites(const std::vector<SiteLimit>& sites)
{
  std::string res;
  for (const auto& s : sites) {
    if (!res.empty()) {
      res += ",";
    }
    res += s.site;
    res += ":";
    res += util::itos(s.limit);
  }
  return res;
}

std::string SiteConcurrency::getRefererHost(const Option& downloadOption)
{
  const auto& headers = downloadOption.get(PREF_HEADER);
  std::vector<std::string> lines;
  util::split(std::begin(headers), std::end(headers),
              std::back_inserter(lines), '\n', /* doStrip = */ true);
  for (const auto& line : lines) {
    auto p = util::divide(std::begin(line), std::end(line), ':');
    if (!util::strieq(p.first.first, p.first.second, "referer") ||
        p.second.first == p.second.second) {
      continue;
    }
    return getUriHost(std::string(p.second.first, p.second.second));
  }
  auto referer = util::strip(downloadOption.get(PREF_REFERER));
  if (referer.empty() || referer == "*") {
    return std::string();
  }
  return getUriHost(referer);
}

std::string SiteConcurrency::normalizeHost(std::string host)
{
  util::lowercase(host);
  if (!host.empty() && host.back() == '.') {
    host.pop_back();
  }
  return host;
}

} // namespace aria2
