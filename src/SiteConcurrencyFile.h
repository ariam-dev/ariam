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
#ifndef D_SITE_CONCURRENCY_FILE_H
#define D_SITE_CONCURRENCY_FILE_H

#include "common.h"

#include <string>

namespace aria2 {

class Option;

// The site-concurrent-file holds PREF_SITE_CONCURRENT and
// PREF_SITE_CONCURRENT_DEFAULT in configuration file syntax, one
// "NAME=VALUE" line each.

// Stores the values of path in option through their option handlers.
// Returns false if path does not exist (ENOENT). Throws DlAbortEx
// naming path and the reason on any other failure, including a line
// for another option or an invalid value. option is unchanged on
// false or throw.
bool loadSiteConcurrencyFile(Option& option, const std::string& path);

// Writes the values of option to a temporary file next to path,
// flushing, syncing, and closing it with each step checked, and renames
// it to path. On failure throws DlAbortEx naming path, the step, and
// the reason; path is unchanged and a temporary file it created is
// removed.
void saveSiteConcurrencyFile(const std::string& path, const Option& option);

} // namespace aria2

#endif // D_SITE_CONCURRENCY_FILE_H
