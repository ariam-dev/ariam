#include "PassphraseLimiter.h"

#include <chrono>

#include <cppunit/extensions/HelperMacros.h>

#include "TimerA2.h"
#include "a2functional.h"

namespace aria2 {

class PassphraseLimiterTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(PassphraseLimiterTest);
  CPPUNIT_TEST(testThreshold);
  CPPUNIT_TEST(testLockoutEnds);
  CPPUNIT_TEST(testDoublingAndCap);
  CPPUNIT_TEST(testWindow);
  CPPUNIT_TEST(testSpreadOut);
  CPPUNIT_TEST(testTrickleStartsOver);
  CPPUNIT_TEST(testEscalationEnds);
  CPPUNIT_TEST(testSucceedResets);
  CPPUNIT_TEST(testPerAddress);
  CPPUNIT_TEST(testEvictExpiredFirst);
  CPPUNIT_TEST(testEvictOldest);
  CPPUNIT_TEST_SUITE_END();

public:
  void testThreshold();
  void testLockoutEnds();
  void testDoublingAndCap();
  void testWindow();
  void testSpreadOut();
  void testTrickleStartsOver();
  void testEscalationEnds();
  void testSucceedResets();
  void testPerAddress();
  void testEvictExpiredFirst();
  void testEvictOldest();
};

CPPUNIT_TEST_SUITE_REGISTRATION(PassphraseLimiterTest);

namespace {
const std::string A = "192.0.2.1";
const std::string B = "192.0.2.2";

Timer at(std::chrono::seconds s) { return Timer(24_h + s); }

void failTimes(PassphraseLimiter& limiter, const std::string& addr, int n,
               const Timer& now)
{
  for (int i = 0; i < n; ++i) {
    limiter.fail(addr, now);
  }
}

// Seconds addr stays locked out from now.
int64_t remaining(const PassphraseLimiter& limiter, const std::string& addr,
                  const Timer& now)
{
  return std::chrono::duration_cast<std::chrono::seconds>(
             limiter.lockoutRemaining(addr, now))
      .count();
}
} // namespace

void PassphraseLimiterTest::testThreshold()
{
  PassphraseLimiter limiter;
  auto t = at(0_s);
  CPPUNIT_ASSERT(!limiter.locked(A, t));
  failTimes(limiter, A, 4, t);
  CPPUNIT_ASSERT_EQUAL(4, limiter.countFailures(A, t));
  CPPUNIT_ASSERT(!limiter.locked(A, t));
  limiter.fail(A, t);
  CPPUNIT_ASSERT_EQUAL(5, limiter.countFailures(A, t));
  CPPUNIT_ASSERT(limiter.locked(A, t));
  CPPUNIT_ASSERT_EQUAL((int64_t)1, remaining(limiter, A, t));
}

void PassphraseLimiterTest::testLockoutEnds()
{
  PassphraseLimiter limiter;
  failTimes(limiter, A, 5, at(0_s));
  auto t = at(0_s);
  t.advance(999_ms);
  CPPUNIT_ASSERT(limiter.locked(A, t));
  CPPUNIT_ASSERT(!limiter.locked(A, at(1_s)));
  // The failures still count after the lockout ends.
  CPPUNIT_ASSERT_EQUAL(5, limiter.countFailures(A, at(1_s)));
}

void PassphraseLimiterTest::testDoublingAndCap()
{
  PassphraseLimiter limiter;
  auto now = 0_s;
  failTimes(limiter, A, 5, at(now));
  CPPUNIT_ASSERT_EQUAL((int64_t)1, remaining(limiter, A, at(now)));
  // Each further failure, made when the lockout ends, doubles it until
  // the 15 min (900 s) cap.
  const int64_t expected[] = {2,   4,   8,   16,  32,  64,
                              128, 256, 512, 900, 900, 900};
  for (auto e : expected) {
    now += std::chrono::seconds(remaining(limiter, A, at(now)));
    CPPUNIT_ASSERT(!limiter.locked(A, at(now)));
    limiter.fail(A, at(now));
    CPPUNIT_ASSERT_EQUAL(e, remaining(limiter, A, at(now)));
  }
  CPPUNIT_ASSERT_EQUAL(17, limiter.countFailures(A, at(now)));
}

void PassphraseLimiterTest::testWindow()
{
  {
    // 5 failures spanning exactly 15 min lock out for 1 s.
    PassphraseLimiter limiter;
    for (auto t : {0_min, 1_min, 2_min, 3_min}) {
      limiter.fail(A, at(t));
    }
    CPPUNIT_ASSERT(!limiter.locked(A, at(15_min)));
    limiter.fail(A, at(15_min));
    CPPUNIT_ASSERT_EQUAL(5, limiter.countFailures(A, at(15_min)));
    CPPUNIT_ASSERT_EQUAL((int64_t)1, remaining(limiter, A, at(15_min)));
  }
  {
    // 5 failures spanning more than 15 min do not.
    PassphraseLimiter limiter;
    const std::chrono::seconds times[] = {0_s, 60_s, 120_s, 180_s,
                                          15_min + 1_s};
    for (auto t : times) {
      limiter.fail(A, at(t));
      CPPUNIT_ASSERT(!limiter.locked(A, at(t)));
    }
    CPPUNIT_ASSERT_EQUAL(4, limiter.countFailures(A, at(15_min + 1_s)));
    // The window slides: one more failure at 15 min + 60 s puts the 5
    // most recent within 60 s through 15 min + 60 s.
    auto t = 15_min + 60_s;
    limiter.fail(A, at(t));
    CPPUNIT_ASSERT_EQUAL((int64_t)1, remaining(limiter, A, at(t)));
  }
}

void PassphraseLimiterTest::testSpreadOut()
{
  // Each failure is less than 15 min after the previous one, but no
  // 15 min span holds 5 of them.
  PassphraseLimiter limiter;
  for (auto t : {0_min, 14_min, 28_min, 42_min, 56_min}) {
    limiter.fail(A, at(t));
    CPPUNIT_ASSERT(!limiter.locked(A, at(t)));
  }
  CPPUNIT_ASSERT_EQUAL(2, limiter.countFailures(A, at(56_min)));
}

void PassphraseLimiterTest::testTrickleStartsOver()
{
  PassphraseLimiter limiter;
  failTimes(limiter, A, 4, at(0_s));
  CPPUNIT_ASSERT_EQUAL(4, limiter.countFailures(A, at(15_min)));
  // More than 15 min later the window holds none of them.
  auto t = 15_min + 1_s;
  CPPUNIT_ASSERT_EQUAL(0, limiter.countFailures(A, at(t)));
  limiter.fail(A, at(t));
  CPPUNIT_ASSERT_EQUAL(1, limiter.countFailures(A, at(t)));
  CPPUNIT_ASSERT(!limiter.locked(A, at(t)));
  failTimes(limiter, A, 3, at(t));
  CPPUNIT_ASSERT(!limiter.locked(A, at(t)));
  limiter.fail(A, at(t));
  CPPUNIT_ASSERT_EQUAL((int64_t)1, remaining(limiter, A, at(t)));
}

void PassphraseLimiterTest::testEscalationEnds()
{
  PassphraseLimiter limiter;
  auto now = 0_s;
  failTimes(limiter, A, 5, at(now));
  // Drive the lockout to its 15 min cap.
  for (int i = 0; i < 10; ++i) {
    now += std::chrono::seconds(remaining(limiter, A, at(now)));
    limiter.fail(A, at(now));
  }
  CPPUNIT_ASSERT_EQUAL(15, limiter.countFailures(A, at(now)));
  CPPUNIT_ASSERT_EQUAL((int64_t)900, remaining(limiter, A, at(now)));
  // 15 min after the lockout ends (30 min after the last failure) the
  // client still escalates, so one more failure locks for 15 min again.
  auto later = now + 15_min + 15_min;
  CPPUNIT_ASSERT(!limiter.locked(A, at(later)));
  CPPUNIT_ASSERT_EQUAL(15, limiter.countFailures(A, at(later)));
  limiter.fail(A, at(later));
  CPPUNIT_ASSERT_EQUAL(16, limiter.countFailures(A, at(later)));
  CPPUNIT_ASSERT_EQUAL((int64_t)900, remaining(limiter, A, at(later)));
  // More than 15 min after the lockout ends, a fresh window starts:
  // 4 failures do not lock, the 5th locks for 1 s.
  auto quiet = later + 15_min + 15_min + 1_s;
  CPPUNIT_ASSERT_EQUAL(0, limiter.countFailures(A, at(quiet)));
  failTimes(limiter, A, 4, at(quiet));
  CPPUNIT_ASSERT_EQUAL(4, limiter.countFailures(A, at(quiet)));
  CPPUNIT_ASSERT(!limiter.locked(A, at(quiet)));
  limiter.fail(A, at(quiet));
  CPPUNIT_ASSERT_EQUAL((int64_t)1, remaining(limiter, A, at(quiet)));
}

void PassphraseLimiterTest::testSucceedResets()
{
  PassphraseLimiter limiter;
  failTimes(limiter, A, 7, at(0_s));
  CPPUNIT_ASSERT(limiter.locked(A, at(0_s)));
  limiter.succeed(A);
  CPPUNIT_ASSERT(!limiter.locked(A, at(0_s)));
  CPPUNIT_ASSERT_EQUAL(0, limiter.countFailures(A, at(0_s)));
  CPPUNIT_ASSERT_EQUAL((size_t)0, limiter.size());
  // The next lockout starts from the threshold and 1 s again.
  failTimes(limiter, A, 4, at(0_s));
  CPPUNIT_ASSERT(!limiter.locked(A, at(0_s)));
  limiter.fail(A, at(0_s));
  CPPUNIT_ASSERT_EQUAL((int64_t)1, remaining(limiter, A, at(0_s)));
}

void PassphraseLimiterTest::testPerAddress()
{
  PassphraseLimiter limiter;
  failTimes(limiter, A, 5, at(0_s));
  CPPUNIT_ASSERT(limiter.locked(A, at(0_s)));
  CPPUNIT_ASSERT(!limiter.locked(B, at(0_s)));
  CPPUNIT_ASSERT_EQUAL(0, limiter.countFailures(B, at(0_s)));
  limiter.succeed(B);
  CPPUNIT_ASSERT(limiter.locked(A, at(0_s)));
}

void PassphraseLimiterTest::testEvictExpiredFirst()
{
  PassphraseLimiter limiter(3);
  limiter.fail("a", at(0_s));
  limiter.fail("b", at(1_s));
  failTimes(limiter, "c", 5, at(20_min));
  CPPUNIT_ASSERT_EQUAL((size_t)3, limiter.size());
  // a and b no longer count at 20 min; both are dropped.
  limiter.fail("d", at(20_min));
  CPPUNIT_ASSERT_EQUAL((size_t)2, limiter.size());
  CPPUNIT_ASSERT(limiter.locked("c", at(20_min)));
  CPPUNIT_ASSERT_EQUAL(1, limiter.countFailures("d", at(20_min)));
}

void PassphraseLimiterTest::testEvictOldest()
{
  PassphraseLimiter limiter(3);
  failTimes(limiter, "a", 5, at(0_s)); // locked until 1 s
  limiter.fail("b", at(2_s));
  limiter.fail("c", at(3_s));
  limiter.fail("d", at(4_s));
  CPPUNIT_ASSERT_EQUAL((size_t)3, limiter.size());
  // a has the oldest last failure or lockout end.
  CPPUNIT_ASSERT_EQUAL(0, limiter.countFailures("a", at(4_s)));
  CPPUNIT_ASSERT_EQUAL(1, limiter.countFailures("b", at(4_s)));
  CPPUNIT_ASSERT_EQUAL(1, limiter.countFailures("c", at(4_s)));
  CPPUNIT_ASSERT_EQUAL(1, limiter.countFailures("d", at(4_s)));
}

} // namespace aria2
