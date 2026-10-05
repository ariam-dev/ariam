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
#ifndef D_SITE_CONCURRENCY_H
#define D_SITE_CONCURRENCY_H

#include "common.h"

#include <string>
#include <vector>

namespace aria2 {

class Option;
class DownloadEngine;

// Site of a download that has no referer.
extern const std::string SITE_NONE;

// One entry of the site-concurrent option.
struct SiteLimit {
  // Lowercase domain name or IP literal.
  std::string site;
  // At least 1.
  int limit;
};

// Site of an unlisted host: an IP literal is its own site; a domain
// name's site is its registrable domain per the Public Suffix List
// (libpsl builtin data), or the whole host if it has none (e.g.
// localhost).
std::string siteOfHost(const std::string& normalizedHost);

struct SiteMatch {
  // The matched site-concurrent entry, siteOfHost() of an unlisted
  // host, or SITE_NONE.
  std::string site;
  bool listed;
  // 0 means no per-site limit.
  int limit;
};

// A download's site comes from the referer in its own option. The site
// list comes from the engine's global option, never from a download's
// option copy, so that changing site-concurrent reclassifies queued
// downloads.
class SiteConcurrency {
public:
  // Reads the site options of the global option of e. Throws DlAbortEx
  // if PREF_SITE_CONCURRENT is invalid.
  explicit SiteConcurrency(const DownloadEngine& e);

  SiteConcurrency(std::vector<SiteLimit> sites, int defaultLimit);

  SiteMatch classify(const Option& downloadOption) const;

  // Empty host gives SITE_NONE.
  SiteMatch matchHost(const std::string& host) const;

  // In site-concurrent order.
  const std::vector<SiteLimit>& getSites() const { return sites_; }

  // Limit of every unlisted site and of SITE_NONE. 0 means no
  // per-site limit.
  int getDefaultLimit() const { return defaultLimit_; }

  // Parses "HOST:N[,HOST:N...]"; empty value gives no entries. Throws
  // DlAbortEx naming the first error.
  static std::vector<SiteLimit> parseSites(const std::string& value);

  // Formats sites as a site-concurrent value parseSites accepts.
  static std::string formatSites(const std::vector<SiteLimit>& sites);

  // Normalized referer host, or empty string if there is none.
  static std::string getRefererHost(const Option& downloadOption);

  static std::string normalizeHost(std::string host);

private:
  std::vector<SiteLimit> sites_;
  // ipLiteral_[i] is true if sites_[i] is an IP literal.
  std::vector<bool> ipLiteral_;
  int defaultLimit_;
};

} // namespace aria2

#endif // D_SITE_CONCURRENCY_H
