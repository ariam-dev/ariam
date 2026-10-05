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
#include "SiteConcurrencyFile.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <sstream>

#include "a2io.h"
#include "Option.h"
#include "OptionParser.h"
#include "OptionHandler.h"
#include "prefs.h"
#include "File.h"
#include "BufferedFile.h"
#include "util.h"
#include "fmt.h"
#include "DlAbortEx.h"
#include "Exception.h"
#include "LogFactory.h"
#include "Logger.h"

namespace aria2 {

namespace {
// A function, not a namespace-scope array, because the PrefPtr globals
// are initialized dynamically in prefs.cc.
std::array<PrefPtr, 2> sitePrefs()
{
  return {{PREF_SITE_CONCURRENT, PREF_SITE_CONCURRENT_DEFAULT}};
}

bool isSitePref(PrefPtr pref)
{
  return pref == PREF_SITE_CONCURRENT || pref == PREF_SITE_CONCURRENT_DEFAULT;
}
} // namespace

bool loadSiteConcurrencyFile(Option& option, const std::string& path)
{
  // Not File::exists(), which is false on any stat failure: only
  // ENOENT means that there is no file.
  a2_struct_stat st;
  if (a2stat(utf8ToWChar(path).c_str(), &st) != 0) {
    int errNum = errno;
    if (errNum == ENOENT) {
      return false;
    }
    throw DL_ABORT_EX(fmt("Failed to load site limits from %s: %s",
                          path.c_str(), util::safeStrerror(errNum).c_str()));
  }
  if (!S_ISREG(st.st_mode)) {
    throw DL_ABORT_EX(
        fmt("Failed to load site limits from %s: not a regular file.",
            path.c_str()));
  }
  std::stringstream ss;
  {
    BufferedFile fp(path.c_str(), BufferedFile::READ);
    if (!fp) {
      int errNum = errno;
      throw DL_ABORT_EX(fmt("Failed to load site limits from %s: %s",
                            path.c_str(), util::safeStrerror(errNum).c_str()));
    }
    fp.transfer(ss);
    if (!fp) {
      int errNum = errno;
      throw DL_ABORT_EX(fmt("Failed to load site limits from %s: %s",
                            path.c_str(), util::safeStrerror(errNum).c_str()));
    }
  }
  // A scratch option, so that an invalid file leaves option unchanged.
  Option values;
  const auto& parser = OptionParser::getInstance();
  std::string line;
  for (int lineno = 1; getline(ss, line); ++lineno) {
    // Same line syntax as OptionParser::parse(Option&, std::istream&).
    if (line.empty() || line[0] == '#') {
      continue;
    }
    auto nv = util::divide(std::begin(line), std::end(line), '=');
    if (nv.first.first == nv.first.second) {
      continue;
    }
    std::string name(nv.first.first, nv.first.second);
    PrefPtr pref = option::k2p(name);
    if (!isSitePref(pref)) {
      throw DL_ABORT_EX(fmt("Failed to load site limits from %s, line %d: "
                            "option '%s' is not allowed in this file.",
                            path.c_str(), lineno, name.c_str()));
    }
    try {
      parser->find(pref)->parse(values,
                                std::string(nv.second.first, nv.second.second));
    }
    catch (Exception& e) {
      throw DL_ABORT_EX2(fmt("Failed to load site limits from %s, line %d.",
                             path.c_str(), lineno),
                         e);
    }
  }
  for (auto pref : sitePrefs()) {
    if (values.defined(pref)) {
      option.put(pref, values.get(pref));
    }
  }
  return true;
}

namespace {
void throwSaveError(const std::string& path, const std::string& tempPath,
                    const char* step, int errNum)
{
  File(tempPath).remove();
  throw DL_ABORT_EX(fmt("Failed to save site limits to %s: %s failed: %s",
                        path.c_str(), step,
                        util::safeStrerror(errNum).c_str()));
}
} // namespace

void saveSiteConcurrencyFile(const std::string& path, const Option& option)
{
  std::string content;
  for (auto pref : sitePrefs()) {
    content += pref->k;
    content += "=";
    content += option.get(pref);
    content += "\n";
  }
  std::string tempPath = path;
  tempPath += "__temp";
#ifdef __MINGW32__
  FILE* fp = a2fopen(utf8ToWChar(tempPath).c_str(),
                     utf8ToWChar(IOFile::WRITE).c_str());
#else  // !__MINGW32__
  FILE* fp = a2fopen(tempPath.c_str(), IOFile::WRITE);
#endif // !__MINGW32__
  if (!fp) {
    // tempPath may be something else, such as a directory, so it is not
    // removed.
    int errNum = errno;
    throw DL_ABORT_EX(fmt("Failed to save site limits to %s: open %s failed: "
                          "%s",
                          path.c_str(), tempPath.c_str(),
                          util::safeStrerror(errNum).c_str()));
  }
  // BufferedFile::close() ignores fflush and fsync errors, so each step
  // is done and checked here.
  if (fwrite(content.data(), 1, content.size(), fp) != content.size()) {
    int errNum = errno;
    fclose(fp);
    throwSaveError(path, tempPath, "write", errNum);
  }
  if (fflush(fp) != 0) {
    int errNum = errno;
    fclose(fp);
    throwSaveError(path, tempPath, "flush", errNum);
  }
#ifdef __MINGW32__
  int syncRv = _commit(fileno(fp));
#else  // !__MINGW32__
  int syncRv = fsync(fileno(fp));
#endif // !__MINGW32__
  if (syncRv != 0) {
    int errNum = errno;
    fclose(fp);
    throwSaveError(path, tempPath, "fsync", errNum);
  }
  if (fclose(fp) != 0) {
    int errNum = errno;
    throwSaveError(path, tempPath, "close", errNum);
  }
  if (!File(tempPath).renameTo(path)) {
    int errNum = errno;
    throwSaveError(path, tempPath, "rename", errNum);
  }
  A2_LOG_INFO(fmt("Saved site limits to %s.", path.c_str()));
}

} // namespace aria2
