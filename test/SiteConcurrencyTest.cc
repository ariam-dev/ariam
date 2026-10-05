#include "SiteConcurrency.h"

#include <memory>
#include <sstream>

#include <cppunit/extensions/HelperMacros.h>

#include "Option.h"
#include "OptionParser.h"
#include "OptionHandler.h"
#include "prefs.h"
#include "Exception.h"
#include "DownloadEngine.h"
#include "SelectEventPoll.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "RpcMethodImpl.h"
#include "RpcRequest.h"
#include "RpcResponse.h"
#include "ValueBase.h"
#include "a2functional.h"

namespace aria2 {

class SiteConcurrencyTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SiteConcurrencyTest);
  CPPUNIT_TEST(testRefererHeaderBeatsRefererOption);
  CPPUNIT_TEST(testRefererHeaderNameCaseInsensitive);
  CPPUNIT_TEST(testEmptyRefererHeaderSkipped);
  CPPUNIT_TEST(testRefererOptionStarOrEmptySkipped);
  CPPUNIT_TEST(testNoReferer);
  CPPUNIT_TEST(testUnparsableReferer);
  CPPUNIT_TEST(testFirstRefererHeaderWins);
  CPPUNIT_TEST(testSubdomainMatch);
  CPPUNIT_TEST(testLongestMatchWins);
  CPPUNIT_TEST(testRegistrableDomain);
  CPPUNIT_TEST(testSiteOfHost);
  CPPUNIT_TEST(testIpLiteral);
  CPPUNIT_TEST(testNormalizeHost);
  CPPUNIT_TEST(testUsesGlobalOptionNotDownloadOption);
  CPPUNIT_TEST(testUnsetOptions);
  CPPUNIT_TEST(testParseSites);
  CPPUNIT_TEST(testParseSites_reject);
  CPPUNIT_TEST(testOptionHandlerFlags);
  CPPUNIT_TEST(testSiteConcurrentDefault);
  CPPUNIT_TEST(testConfigFile);
  CPPUNIT_TEST(testChangeGlobalOption);
  CPPUNIT_TEST(testChangeGlobalOption_withBadSiteConcurrent);
  CPPUNIT_TEST_SUITE_END();

  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;

public:
  void setUp()
  {
    option_ = std::make_shared<Option>();
    e_ = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
    e_->setOption(option_.get());
    e_->setRequestGroupMan(make_unique<RequestGroupMan>(
        std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get()));
  }

  void testRefererHeaderBeatsRefererOption();
  void testRefererHeaderNameCaseInsensitive();
  void testEmptyRefererHeaderSkipped();
  void testRefererOptionStarOrEmptySkipped();
  void testNoReferer();
  void testUnparsableReferer();
  void testFirstRefererHeaderWins();
  void testSubdomainMatch();
  void testLongestMatchWins();
  void testRegistrableDomain();
  void testSiteOfHost();
  void testIpLiteral();
  void testNormalizeHost();
  void testUsesGlobalOptionNotDownloadOption();
  void testUnsetOptions();
  void testParseSites();
  void testParseSites_reject();
  void testOptionHandlerFlags();
  void testSiteConcurrentDefault();
  void testConfigFile();
  void testChangeGlobalOption();
  void testChangeGlobalOption_withBadSiteConcurrent();
};

CPPUNIT_TEST_SUITE_REGISTRATION(SiteConcurrencyTest);

namespace {
SiteConcurrency createSites()
{
  return SiteConcurrency({{"a.test", 1}, {"b.test", 3}, {"x.b.test", 5}}, 2);
}

Option createDownloadOption(const std::string& header,
                            const std::string& referer)
{
  Option option;
  if (!header.empty()) {
    option.put(PREF_HEADER, header);
  }
  if (!referer.empty()) {
    option.put(PREF_REFERER, referer);
  }
  return option;
}

rpc::RpcRequest createReq(std::string methodName)
{
  return {std::move(methodName), List::g()};
}
} // namespace

void SiteConcurrencyTest::testRefererHeaderBeatsRefererOption()
{
  auto sites = createSites();
  auto option = createDownloadOption(
      "Accept: */*\nReferer: https://a.test/page\n", "https://b.test/");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"),
                       SiteConcurrency::getRefererHost(option));
  auto m = sites.classify(option);
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), m.site);
  CPPUNIT_ASSERT(m.listed);
  CPPUNIT_ASSERT_EQUAL(1, m.limit);

  // referer option is used when the header has no Referer line.
  option = createDownloadOption("Accept: */*\n", "https://b.test/");
  m = sites.classify(option);
  CPPUNIT_ASSERT_EQUAL(std::string("b.test"), m.site);
  CPPUNIT_ASSERT_EQUAL(3, m.limit);
}

void SiteConcurrencyTest::testRefererHeaderNameCaseInsensitive()
{
  auto sites = createSites();
  auto option = createDownloadOption("referer: https://a.test/\n", "");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), sites.classify(option).site);
  option = createDownloadOption("REFERER:https://a.test/\n", "");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), sites.classify(option).site);
  // A header whose name only starts with Referer is not a Referer line.
  option = createDownloadOption("Referer-Policy: https://a.test/\n", "");
  CPPUNIT_ASSERT_EQUAL(SITE_NONE, sites.classify(option).site);
}

void SiteConcurrencyTest::testEmptyRefererHeaderSkipped()
{
  auto sites = createSites();
  auto option =
      createDownloadOption("Referer: \nAccept: */*\n", "https://b.test/");
  CPPUNIT_ASSERT_EQUAL(std::string("b.test"), sites.classify(option).site);
  // A later non-empty Referer line is used.
  option = createDownloadOption("Referer:\nReferer: https://a.test/\n",
                                "https://b.test/");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), sites.classify(option).site);
}

void SiteConcurrencyTest::testRefererOptionStarOrEmptySkipped()
{
  auto sites = createSites();
  auto option = createDownloadOption("", "*");
  CPPUNIT_ASSERT_EQUAL(std::string(),
                       SiteConcurrency::getRefererHost(option));
  CPPUNIT_ASSERT_EQUAL(SITE_NONE, sites.classify(option).site);

  option = createDownloadOption("Referer: https://a.test/\n", "*");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), sites.classify(option).site);

  option.put(PREF_HEADER, "");
  option.put(PREF_REFERER, "");
  CPPUNIT_ASSERT_EQUAL(SITE_NONE, sites.classify(option).site);
}

void SiteConcurrencyTest::testNoReferer()
{
  auto sites = createSites();
  Option option;
  auto m = sites.classify(option);
  CPPUNIT_ASSERT_EQUAL(std::string("(none)"), m.site);
  CPPUNIT_ASSERT(!m.listed);
  CPPUNIT_ASSERT_EQUAL(2, m.limit);
  // The download URI never decides the site.
  option.put(PREF_HEADER, "Accept: */*\n");
  CPPUNIT_ASSERT_EQUAL(SITE_NONE, sites.classify(option).site);
}

void SiteConcurrencyTest::testUnparsableReferer()
{
  auto sites = createSites();
  auto option = createDownloadOption("Referer: a.test/page\n", "");
  CPPUNIT_ASSERT_EQUAL(SITE_NONE, sites.classify(option).site);
  option = createDownloadOption("", "not a uri");
  CPPUNIT_ASSERT_EQUAL(SITE_NONE, sites.classify(option).site);
  // An unparsable non-empty Referer line decides the site; neither the
  // referer option nor a later Referer line is used.
  option = createDownloadOption("Referer: a.test/page\n", "https://b.test/");
  CPPUNIT_ASSERT_EQUAL(std::string(),
                       SiteConcurrency::getRefererHost(option));
  CPPUNIT_ASSERT_EQUAL(SITE_NONE, sites.classify(option).site);
  option = createDownloadOption(
      "Referer: a.test/page\nReferer: https://b.test/\n", "");
  CPPUNIT_ASSERT_EQUAL(SITE_NONE, sites.classify(option).site);
}

void SiteConcurrencyTest::testFirstRefererHeaderWins()
{
  auto sites = createSites();
  auto option = createDownloadOption(
      "Referer: https://a.test/\nReferer: https://b.test/\n", "");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"),
                       SiteConcurrency::getRefererHost(option));
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), sites.classify(option).site);
  option = createDownloadOption(
      "Referer: https://b.test/\nReferer: https://a.test/\n", "");
  CPPUNIT_ASSERT_EQUAL(std::string("b.test"), sites.classify(option).site);
}

void SiteConcurrencyTest::testSubdomainMatch()
{
  auto sites = createSites();
  auto m = sites.matchHost("members.a.test");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), m.site);
  CPPUNIT_ASSERT(m.listed);
  CPPUNIT_ASSERT_EQUAL(1, m.limit);
  m = sites.matchHost("cdn.members.a.test");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), m.site);
  // Suffix without a label boundary is not a subdomain.
  m = sites.matchHost("aa.test");
  CPPUNIT_ASSERT_EQUAL(std::string("aa.test"), m.site);
  CPPUNIT_ASSERT(!m.listed);
  CPPUNIT_ASSERT_EQUAL(2, m.limit);
  // A listed site does not match its parent.
  SiteConcurrency deep({{"members.c.test", 4}}, 0);
  m = deep.matchHost("c.test");
  CPPUNIT_ASSERT_EQUAL(std::string("c.test"), m.site);
  CPPUNIT_ASSERT(!m.listed);
  CPPUNIT_ASSERT_EQUAL(0, m.limit);
}

void SiteConcurrencyTest::testLongestMatchWins()
{
  auto sites = createSites();
  auto m = sites.matchHost("y.x.b.test");
  CPPUNIT_ASSERT_EQUAL(std::string("x.b.test"), m.site);
  CPPUNIT_ASSERT_EQUAL(5, m.limit);
  m = sites.matchHost("x.b.test");
  CPPUNIT_ASSERT_EQUAL(std::string("x.b.test"), m.site);
  m = sites.matchHost("y.b.test");
  CPPUNIT_ASSERT_EQUAL(std::string("b.test"), m.site);
  CPPUNIT_ASSERT_EQUAL(3, m.limit);
  // List order does not matter.
  SiteConcurrency reversed({{"x.b.test", 5}, {"b.test", 3}}, 0);
  CPPUNIT_ASSERT_EQUAL(std::string("x.b.test"),
                       reversed.matchHost("y.x.b.test").site);
}

void SiteConcurrencyTest::testRegistrableDomain()
{
  // An unlisted host's site is its registrable domain.
  auto sites = createSites();
  auto m = sites.matchHost("members.example.com");
  CPPUNIT_ASSERT_EQUAL(std::string("example.com"), m.site);
  CPPUNIT_ASSERT(!m.listed);
  CPPUNIT_ASSERT_EQUAL(2, m.limit);
  CPPUNIT_ASSERT_EQUAL(std::string("example.com"),
                       sites.matchHost("dl.example.com").site);
  CPPUNIT_ASSERT_EQUAL(std::string("example.com"),
                       sites.matchHost("a.b.example.com").site);
  m = sites.matchHost("example.com");
  CPPUNIT_ASSERT_EQUAL(std::string("example.com"), m.site);
  CPPUNIT_ASSERT(!m.listed);
  // A multi-label public suffix.
  CPPUNIT_ASSERT_EQUAL(std::string("example.co.uk"),
                       sites.matchHost("members.example.co.uk").site);
  // A suffix from the list's private section: each user is a site.
  CPPUNIT_ASSERT_EQUAL(std::string("foo.github.io"),
                       sites.matchHost("foo.github.io").site);
  CPPUNIT_ASSERT_EQUAL(std::string("foo.github.io"),
                       sites.matchHost("dl.foo.github.io").site);
  // No registrable domain: the whole host.
  CPPUNIT_ASSERT_EQUAL(std::string("localhost"),
                       sites.matchHost("localhost").site);
  CPPUNIT_ASSERT_EQUAL(std::string("co.uk"), sites.matchHost("co.uk").site);
  // A listed host has its own limit; its siblings keep the default.
  SiteConcurrency listed({{"dl.example.com", 4}}, 2);
  m = listed.matchHost("dl.example.com");
  CPPUNIT_ASSERT_EQUAL(std::string("dl.example.com"), m.site);
  CPPUNIT_ASSERT(m.listed);
  CPPUNIT_ASSERT_EQUAL(4, m.limit);
  m = listed.matchHost("members.example.com");
  CPPUNIT_ASSERT_EQUAL(std::string("example.com"), m.site);
  CPPUNIT_ASSERT(!m.listed);
  CPPUNIT_ASSERT_EQUAL(2, m.limit);
}

void SiteConcurrencyTest::testSiteOfHost()
{
  CPPUNIT_ASSERT_EQUAL(std::string("example.co.uk"),
                       siteOfHost("members.example.co.uk"));
  CPPUNIT_ASSERT_EQUAL(std::string("localhost"), siteOfHost("localhost"));
  // IP literals are their own site.
  CPPUNIT_ASSERT_EQUAL(std::string("10.0.0.1"), siteOfHost("10.0.0.1"));
  CPPUNIT_ASSERT_EQUAL(std::string("2001:db8::1"), siteOfHost("2001:db8::1"));
  CPPUNIT_ASSERT_EQUAL(std::string("::ffff:10.0.0.1"),
                       siteOfHost("::ffff:10.0.0.1"));
  CPPUNIT_ASSERT_EQUAL(std::string(), siteOfHost(""));
}

void SiteConcurrencyTest::testIpLiteral()
{
  SiteConcurrency sites({{"192.0.2.10", 1}, {"2001:db8::1", 2}}, 3);
  auto m = sites.matchHost("192.0.2.10");
  CPPUNIT_ASSERT_EQUAL(std::string("192.0.2.10"), m.site);
  CPPUNIT_ASSERT(m.listed);
  CPPUNIT_ASSERT_EQUAL(1, m.limit);
  // Unlisted IPv4 address is its own site, not a registrable domain.
  m = sites.matchHost("10.0.0.1");
  CPPUNIT_ASSERT_EQUAL(std::string("10.0.0.1"), m.site);
  CPPUNIT_ASSERT(!m.listed);
  CPPUNIT_ASSERT_EQUAL(3, m.limit);
  // IPv6 from a bracketed referer URI.
  auto option = createDownloadOption("Referer: http://[2001:DB8::1]:8080/\n",
                                     "");
  m = sites.classify(option);
  CPPUNIT_ASSERT_EQUAL(std::string("2001:db8::1"), m.site);
  CPPUNIT_ASSERT(m.listed);
  CPPUNIT_ASSERT_EQUAL(2, m.limit);
  option = createDownloadOption("", "http://[2001:db8::2]/");
  m = sites.classify(option);
  CPPUNIT_ASSERT_EQUAL(std::string("2001:db8::2"), m.site);
  CPPUNIT_ASSERT(!m.listed);
}

void SiteConcurrencyTest::testNormalizeHost()
{
  CPPUNIT_ASSERT_EQUAL(std::string("members.example.com"),
                       SiteConcurrency::normalizeHost("Members.Example.COM."));
  auto sites = createSites();
  auto option =
      createDownloadOption("Referer: https://WWW.A.Test./page?q=1\n", "");
  CPPUNIT_ASSERT_EQUAL(std::string("www.a.test"),
                       SiteConcurrency::getRefererHost(option));
  auto m = sites.classify(option);
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), m.site);
  CPPUNIT_ASSERT(m.listed);
  m = sites.matchHost("Members.Example.COM.");
  CPPUNIT_ASSERT_EQUAL(std::string("example.com"), m.site);
}

void SiteConcurrencyTest::testUsesGlobalOptionNotDownloadOption()
{
  option_->put(PREF_SITE_CONCURRENT, "a.test:1");
  option_->put(PREF_SITE_CONCURRENT_DEFAULT, "2");
  // A download's option is a copy of the global option made when it
  // was added.
  Option download(*option_);
  download.put(PREF_REFERER, "https://c.test/");
  // The engine's global option changes after the download was added;
  // the download's copy keeps the stale values.
  option_->put(PREF_SITE_CONCURRENT, "a.test:1,c.test:4");
  option_->put(PREF_SITE_CONCURRENT_DEFAULT, "3");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1"),
                       download.get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("2"),
                       download.get(PREF_SITE_CONCURRENT_DEFAULT));

  SiteConcurrency sites(*e_);
  auto m = sites.classify(download);
  CPPUNIT_ASSERT_EQUAL(std::string("c.test"), m.site);
  CPPUNIT_ASSERT(m.listed);
  CPPUNIT_ASSERT_EQUAL(4, m.limit);
  download.put(PREF_REFERER, "https://d.test/");
  m = sites.classify(download);
  CPPUNIT_ASSERT_EQUAL(std::string("d.test"), m.site);
  CPPUNIT_ASSERT(!m.listed);
  CPPUNIT_ASSERT_EQUAL(3, m.limit);

  CPPUNIT_ASSERT_EQUAL((size_t)2, sites.getSites().size());
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), sites.getSites()[0].site);
  CPPUNIT_ASSERT_EQUAL(1, sites.getSites()[0].limit);
  CPPUNIT_ASSERT_EQUAL(std::string("c.test"), sites.getSites()[1].site);
  CPPUNIT_ASSERT_EQUAL(4, sites.getSites()[1].limit);
  CPPUNIT_ASSERT_EQUAL(3, sites.getDefaultLimit());
}

void SiteConcurrencyTest::testUnsetOptions()
{
  SiteConcurrency sites(*e_);
  CPPUNIT_ASSERT(sites.getSites().empty());
  CPPUNIT_ASSERT_EQUAL(0, sites.getDefaultLimit());
  Option download;
  download.put(PREF_REFERER, "https://a.test/");
  auto m = sites.classify(download);
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), m.site);
  CPPUNIT_ASSERT(!m.listed);
  CPPUNIT_ASSERT_EQUAL(0, m.limit);
  CPPUNIT_ASSERT_EQUAL(0, sites.classify(Option()).limit);
}

void SiteConcurrencyTest::testParseSites()
{
  auto sites = SiteConcurrency::parseSites(
      " a.test:1 , b.test:3,192.0.2.10:2,::1:4,xn--bcher-kva.example:1");
  CPPUNIT_ASSERT_EQUAL((size_t)5, sites.size());
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"), sites[0].site);
  CPPUNIT_ASSERT_EQUAL(1, sites[0].limit);
  CPPUNIT_ASSERT_EQUAL(std::string("b.test"), sites[1].site);
  CPPUNIT_ASSERT_EQUAL(3, sites[1].limit);
  CPPUNIT_ASSERT_EQUAL(std::string("192.0.2.10"), sites[2].site);
  CPPUNIT_ASSERT_EQUAL(std::string("::1"), sites[3].site);
  CPPUNIT_ASSERT_EQUAL(4, sites[3].limit);
  CPPUNIT_ASSERT_EQUAL(
      std::string("a.test:1,b.test:3,192.0.2.10:2,::1:4,"
                  "xn--bcher-kva.example:1"),
      SiteConcurrency::formatSites(sites));
  CPPUNIT_ASSERT(SiteConcurrency::parseSites("").empty());
  CPPUNIT_ASSERT(SiteConcurrency::parseSites("  ").empty());
  CPPUNIT_ASSERT_EQUAL(std::string(), SiteConcurrency::formatSites({}));
}

void SiteConcurrencyTest::testParseSites_reject()
{
  const char* bad[] = {
      // syntax
      "a.test", "a.test:", ":1", "a.test:x", "a.test:1,,b.test:2", "a.test:1,",
      ",a.test:1", "a.test:1:2", "a.test:+1", "a.test: 1",
      // N < 1
      "a.test:0", "a.test:-1", "b.test:2,a.test:0",
      // N too large
      "a.test:4294967296",
      // duplicate hosts
      "a.test:1,a.test:2", "a.test:1,b.test:2,a.test:1",
      // uppercase host
      "A.test:1", "a.test:1,B.TEST:2", "2001:DB8::1:1",
      // invalid host
      "a_b.test:1", "-a.test:1", "a-.test:1", "a..test:1", ".a.test:1",
      "a.test.:1", "http://a.test:1", "[::1]:1", "a test:1", "*.a.test:1",
      "(none):1"};
  for (auto s : bad) {
    try {
      SiteConcurrency::parseSites(s);
      CPPUNIT_FAIL(std::string("exception must be thrown: ") + s);
    }
    catch (Exception& e) {
    }
  }
}

void SiteConcurrencyTest::testOptionHandlerFlags()
{
  auto& parser = OptionParser::getInstance();
  auto h = parser->find(PREF_SITE_CONCURRENT);
  CPPUNIT_ASSERT(h);
  CPPUNIT_ASSERT(h->getChangeGlobalOption());
  CPPUNIT_ASSERT(!h->getInitialOption());
  CPPUNIT_ASSERT(!h->getChangeOption());
  CPPUNIT_ASSERT(!h->getChangeOptionForReserved());
  CPPUNIT_ASSERT(h->getDefaultValue().empty());

  h = parser->find(PREF_SITE_CONCURRENT_DEFAULT);
  CPPUNIT_ASSERT(h);
  CPPUNIT_ASSERT(h->getChangeGlobalOption());
  CPPUNIT_ASSERT(!h->getInitialOption());
  CPPUNIT_ASSERT(!h->getChangeOption());
  CPPUNIT_ASSERT(!h->getChangeOptionForReserved());
  CPPUNIT_ASSERT_EQUAL(std::string("0"), h->getDefaultValue());

  h = parser->find(PREF_SITE_CONCURRENT_FILE);
  CPPUNIT_ASSERT(h);
  CPPUNIT_ASSERT(!h->getChangeGlobalOption());
  CPPUNIT_ASSERT(!h->getInitialOption());
  CPPUNIT_ASSERT(!h->getChangeOption());
  CPPUNIT_ASSERT(!h->getChangeOptionForReserved());
}

void SiteConcurrencyTest::testSiteConcurrentDefault()
{
  auto h = OptionParser::getInstance()->find(PREF_SITE_CONCURRENT_DEFAULT);
  auto& option = *option_;
  h->parse(option, "0");
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       option.get(PREF_SITE_CONCURRENT_DEFAULT));
  CPPUNIT_ASSERT_EQUAL(0, SiteConcurrency(*e_).getDefaultLimit());
  h->parse(option, "7");
  CPPUNIT_ASSERT_EQUAL(std::string("7"),
                       option.get(PREF_SITE_CONCURRENT_DEFAULT));
  try {
    h->parse(option, "-1");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    h->parse(option, "x");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("7"),
                       option.get(PREF_SITE_CONCURRENT_DEFAULT));
}

void SiteConcurrencyTest::testConfigFile()
{
  auto& option = *option_;
  std::stringstream ss;
  ss << "site-concurrent=a.test:1, b.test:3\n"
     << "site-concurrent-default=2\n"
     << "site-concurrent-file=/ariam/site-concurrent.conf\n";
  OptionParser::getInstance()->parse(option, ss);
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1,b.test:3"),
                       option.get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("/ariam/site-concurrent.conf"),
                       option.get(PREF_SITE_CONCURRENT_FILE));
  SiteConcurrency sites(*e_);
  CPPUNIT_ASSERT_EQUAL((size_t)2, sites.getSites().size());
  CPPUNIT_ASSERT_EQUAL(2, sites.getDefaultLimit());

  std::stringstream bad;
  bad << "site-concurrent=a.test:1,b.test:0\n";
  try {
    OptionParser::getInstance()->parse(option, bad);
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1,b.test:3"),
                       option.get(PREF_SITE_CONCURRENT));
}

void SiteConcurrencyTest::testChangeGlobalOption()
{
  rpc::ChangeGlobalOptionRpcMethod m;
  auto req = createReq(rpc::ChangeGlobalOptionRpcMethod::getMethodName());
  auto opt = Dict::g();
  opt->put(PREF_SITE_CONCURRENT->k, "a.test:1,b.test:3");
  opt->put(PREF_SITE_CONCURRENT_DEFAULT->k, "2");
  opt->put(PREF_SITE_CONCURRENT_FILE->k, "/ariam/site-concurrent.conf");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1,b.test:3"),
                       e_->getOption()->get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("2"),
                       e_->getOption()->get(PREF_SITE_CONCURRENT_DEFAULT));
  // site-concurrent-file is not changeable over RPC.
  CPPUNIT_ASSERT(!e_->getOption()->defined(PREF_SITE_CONCURRENT_FILE));

  // Empty value removes every listed site.
  req = createReq(rpc::ChangeGlobalOptionRpcMethod::getMethodName());
  opt = Dict::g();
  opt->put(PREF_SITE_CONCURRENT->k, "");
  req.params->append(std::move(opt));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT(SiteConcurrency(*e_).getSites().empty());
}

void SiteConcurrencyTest::testChangeGlobalOption_withBadSiteConcurrent()
{
  option_->put(PREF_SITE_CONCURRENT, "a.test:1");
  option_->put(PREF_SITE_CONCURRENT_DEFAULT, "2");
  const char* bad[] = {"a.test:1,b.test:0", "a.test:1,a.test:2", "A.test:1",
                       "a.test"};
  for (auto s : bad) {
    rpc::ChangeGlobalOptionRpcMethod m;
    auto req = createReq(rpc::ChangeGlobalOptionRpcMethod::getMethodName());
    auto opt = Dict::g();
    opt->put(PREF_SITE_CONCURRENT_DEFAULT->k, "5");
    opt->put(PREF_SITE_CONCURRENT->k, s);
    req.params->append(std::move(opt));
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(1, res.code);
    CPPUNIT_ASSERT_EQUAL(std::string("a.test:1"),
                         option_->get(PREF_SITE_CONCURRENT));
    CPPUNIT_ASSERT_EQUAL(std::string("2"),
                         option_->get(PREF_SITE_CONCURRENT_DEFAULT));
  }
}

} // namespace aria2
