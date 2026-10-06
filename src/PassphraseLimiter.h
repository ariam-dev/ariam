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
#ifndef D_PASSPHRASE_LIMITER_H
#define D_PASSPHRASE_LIMITER_H

#include "common.h"

#include <array>
#include <chrono>
#include <map>
#include <string>

#include "TimerA2.h"

namespace aria2 {

enum PassphraseCheck {
  PASSPHRASE_OK,
  // No Basic credentials; nothing was checked or counted.
  PASSPHRASE_MISSING,
  // Rejected. A mismatch is counted; an unset passphrase or an HMAC
  // setup failure is not.
  PASSPHRASE_WRONG,
  // The client is locked out; nothing was checked or counted.
  PASSPHRASE_LOCKED
};

// Counts failed passphrase checks per client address.
//
// A failure at t is within the window at now if t >= now -
// FAILURE_WINDOW. A failure by a client that is not escalating locks it
// out for FIRST_LOCKOUT, and starts its escalation, when it and the
// LOCKOUT_THRESHOLD - 1 failures before it are all within the window.
// While a client escalates, every further failure locks it out for twice
// the previous duration, capped at MAX_LOCKOUT. Each lockout starts at
// the failure that causes it. Escalation ends when more than
// FAILURE_WINDOW has passed with no failure since the last lockout
// ended; the next failure then starts a fresh window. The caller does
// not count attempts made during a lockout. A success forgets the
// client.
//
// At most maxClients addresses are tracked. A new address at a full
// table drops the records that no longer affect their client (escalation
// ended, or no failure within the window); if the table is still full,
// the record with the oldest last failure or lockout end is dropped.
class PassphraseLimiter {
public:
  static constexpr int LOCKOUT_THRESHOLD = 5;
  static constexpr std::chrono::minutes FAILURE_WINDOW{15};
  static constexpr std::chrono::seconds FIRST_LOCKOUT{1};
  static constexpr std::chrono::minutes MAX_LOCKOUT{15};
  static constexpr size_t MAX_CLIENTS = 1024;

  explicit PassphraseLimiter(size_t maxClients = MAX_CLIENTS);

  bool locked(const std::string& addr, const Timer& now) const;

  void fail(const std::string& addr, const Timer& now);

  void succeed(const std::string& addr);

  // While addr escalates, LOCKOUT_THRESHOLD plus its failures after the
  // first lockout; otherwise its failures within the window.
  int countFailures(const std::string& addr, const Timer& now) const;

  Timer::Clock::duration lockoutRemaining(const std::string& addr,
                                          const Timer& now) const;

  size_t size() const { return clients_.size(); }

private:
  struct Client {
    // Failures while not escalating, oldest first.
    std::array<Timer::Clock::time_point, LOCKOUT_THRESHOLD> recent;
    size_t nrecent;
    // Failures since escalation started; zero if not escalating.
    int escalation;
    // Meaningful only while escalating.
    Timer::Clock::time_point lockedUntil;
  };

  // The last failure, or the last lockout end while escalating.
  static Timer::Clock::time_point lastEvent(const Client& client);

  // True if client no longer affects its address at now.
  static bool expired(const Client& client, const Timer& now);

  void evict(const Timer& now);

  size_t maxClients_;
  std::map<std::string, Client> clients_;
};

} // namespace aria2

#endif // D_PASSPHRASE_LIMITER_H
