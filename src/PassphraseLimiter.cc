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
#include "PassphraseLimiter.h"

#include <algorithm>

namespace aria2 {

constexpr int PassphraseLimiter::LOCKOUT_THRESHOLD;
constexpr std::chrono::minutes PassphraseLimiter::FAILURE_WINDOW;
constexpr std::chrono::seconds PassphraseLimiter::FIRST_LOCKOUT;
constexpr std::chrono::minutes PassphraseLimiter::MAX_LOCKOUT;
constexpr size_t PassphraseLimiter::MAX_CLIENTS;

PassphraseLimiter::PassphraseLimiter(size_t maxClients)
    : maxClients_(std::max(maxClients, static_cast<size_t>(1)))
{
}

Timer::Clock::time_point PassphraseLimiter::lastEvent(const Client& client)
{
  if (client.escalation > 0) {
    return client.lockedUntil;
  }
  return client.recent[client.nrecent - 1];
}

bool PassphraseLimiter::expired(const Client& client, const Timer& now)
{
  if (client.escalation == 0 && client.nrecent == 0) {
    return true;
  }
  return now.getTime() > lastEvent(client) + FAILURE_WINDOW;
}

bool PassphraseLimiter::locked(const std::string& addr, const Timer& now) const
{
  return lockoutRemaining(addr, now) > Timer::Clock::duration::zero();
}

Timer::Clock::duration
PassphraseLimiter::lockoutRemaining(const std::string& addr,
                                    const Timer& now) const
{
  auto i = clients_.find(addr);
  if (i == clients_.end() || now.getTime() >= (*i).second.lockedUntil) {
    return Timer::Clock::duration::zero();
  }
  return (*i).second.lockedUntil - now.getTime();
}

int PassphraseLimiter::countFailures(const std::string& addr,
                                     const Timer& now) const
{
  auto i = clients_.find(addr);
  if (i == clients_.end() || expired((*i).second, now)) {
    return 0;
  }
  const auto& client = (*i).second;
  if (client.escalation > 0) {
    return client.escalation;
  }
  return static_cast<int>(
      std::count_if(client.recent.begin(),
                    client.recent.begin() + client.nrecent,
                    [&now](const Timer::Clock::time_point& t) {
                      return now.getTime() <= t + FAILURE_WINDOW;
                    }));
}

void PassphraseLimiter::evict(const Timer& now)
{
  for (auto i = clients_.begin(); i != clients_.end();) {
    if (expired((*i).second, now)) {
      i = clients_.erase(i);
    }
    else {
      ++i;
    }
  }
  if (clients_.size() < maxClients_) {
    return;
  }
  auto oldest = std::min_element(
      clients_.begin(), clients_.end(),
      [](const std::pair<const std::string, Client>& a,
         const std::pair<const std::string, Client>& b) {
        return lastEvent(a.second) < lastEvent(b.second);
      });
  clients_.erase(oldest);
}

void PassphraseLimiter::fail(const std::string& addr, const Timer& now)
{
  auto i = clients_.find(addr);
  if (i == clients_.end()) {
    if (clients_.size() >= maxClients_) {
      evict(now);
    }
    i = clients_.insert(std::make_pair(addr, Client{})).first;
  }
  auto& client = (*i).second;
  if (expired(client, now)) {
    client = Client{};
  }
  if (client.escalation > 0) {
    ++client.escalation;
  }
  else {
    if (client.nrecent == client.recent.size()) {
      std::copy(client.recent.begin() + 1, client.recent.end(),
                client.recent.begin());
      --client.nrecent;
    }
    client.recent[client.nrecent++] = now.getTime();
    if (client.nrecent < client.recent.size() ||
        now.getTime() > client.recent[0] + FAILURE_WINDOW) {
      return;
    }
    client.escalation = LOCKOUT_THRESHOLD;
  }
  // 1 s << 10 exceeds MAX_LOCKOUT; capping the shift keeps it in range.
  int doublings = std::min(client.escalation - LOCKOUT_THRESHOLD, 10);
  auto lockout = std::min<Timer::Clock::duration>(
      FIRST_LOCKOUT * (1 << doublings), MAX_LOCKOUT);
  client.lockedUntil = now.getTime() + lockout;
}

void PassphraseLimiter::succeed(const std::string& addr)
{
  clients_.erase(addr);
}

} // namespace aria2
