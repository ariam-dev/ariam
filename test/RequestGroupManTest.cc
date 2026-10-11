#include "RequestGroupMan.h"

#include <fstream>

#include <cppunit/extensions/HelperMacros.h>

#include "TestUtil.h"
#include "prefs.h"
#include "DownloadContext.h"
#include "RequestGroup.h"
#include "Option.h"
#include "DownloadResult.h"
#include "FileEntry.h"
#include "ServerStatMan.h"
#include "ServerStat.h"
#include "File.h"
#include "array_fun.h"
#include "RecoverableException.h"
#include "util.h"
#include "DownloadEngine.h"
#include "SelectEventPoll.h"
#include "UriListParser.h"
#include "OptionParser.h"
#include "OptionHandler.h"

namespace aria2 {

class RequestGroupManTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(RequestGroupManTest);
  CPPUNIT_TEST(testIsSameFileBeingDownloaded);
  CPPUNIT_TEST(testIsSameFileBeingDownloaded_paused);
  CPPUNIT_TEST(testGetInitialCommands);
  CPPUNIT_TEST(testLoadServerStat);
  CPPUNIT_TEST(testSaveServerStat);
  CPPUNIT_TEST(testChangeReservedGroupPosition);
  CPPUNIT_TEST(testFillRequestGroupFromReserver);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_uriParser);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_skippedOwnsPath);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_siteAcceptance);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_siteSlotFrees);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_siteFifo);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_siteCeiling);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_siteDefault);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_siteDefaultUnlimited);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_siteReclassify);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_siteUnset);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_siteParseError);
  CPPUNIT_TEST(testInsertReservedGroup);
  CPPUNIT_TEST(testAddDownloadResult);
  CPPUNIT_TEST(testHalt_interruptedDownload);
  CPPUNIT_TEST_SUITE_END();

private:
  std::unique_ptr<DownloadEngine> e_;
  std::shared_ptr<Option> option_;
  RequestGroupMan* rgman_;

public:
  void setUp()
  {
    option_ = std::make_shared<Option>();
    option_->put(PREF_PIECE_LENGTH, "1048576");
    // To enable paused RequestGroup
    option_->put(PREF_ENABLE_RPC, A2_V_TRUE);
    File(option_->get(PREF_DIR)).mkdirs();
    e_ = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
    e_->setOption(option_.get());
    auto rgman = make_unique<RequestGroupMan>(
        std::vector<std::shared_ptr<RequestGroup>>{}, 3, option_.get());
    rgman_ = rgman.get();
    e_->setRequestGroupMan(std::move(rgman));
  }

  void testIsSameFileBeingDownloaded();
  void testIsSameFileBeingDownloaded_paused();
  void testGetInitialCommands();
  void testLoadServerStat();
  void testSaveServerStat();
  void testChangeReservedGroupPosition();
  void testFillRequestGroupFromReserver();
  void testFillRequestGroupFromReserver_uriParser();
  void testFillRequestGroupFromReserver_skippedOwnsPath();
  void testFillRequestGroupFromReserver_siteAcceptance();
  void testFillRequestGroupFromReserver_siteSlotFrees();
  void testFillRequestGroupFromReserver_siteFifo();
  void testFillRequestGroupFromReserver_siteCeiling();
  void testFillRequestGroupFromReserver_siteDefault();
  void testFillRequestGroupFromReserver_siteDefaultUnlimited();
  void testFillRequestGroupFromReserver_siteReclassify();
  void testFillRequestGroupFromReserver_siteUnset();
  void testFillRequestGroupFromReserver_siteParseError();
  void testInsertReservedGroup();
  void testAddDownloadResult();
  void testHalt_interruptedDownload();
};

CPPUNIT_TEST_SUITE_REGISTRATION(RequestGroupManTest);

void RequestGroupManTest::testIsSameFileBeingDownloaded()
{
  std::shared_ptr<RequestGroup> rg1(
      new RequestGroup(GroupId::create(), util::copy(option_)));
  std::shared_ptr<RequestGroup> rg2(
      new RequestGroup(GroupId::create(), util::copy(option_)));

  std::shared_ptr<DownloadContext> dctx1(
      new DownloadContext(0, 0, "aria2.tar.bz2"));
  std::shared_ptr<DownloadContext> dctx2(
      new DownloadContext(0, 0, "aria2.tar.bz2"));

  rg1->setDownloadContext(dctx1);
  rg2->setDownloadContext(dctx2);

  RequestGroupMan gm(std::vector<std::shared_ptr<RequestGroup>>(), 1,
                     option_.get());

  gm.addRequestGroup(rg1);
  gm.addRequestGroup(rg2);

  CPPUNIT_ASSERT(gm.isSameFileBeingDownloaded(rg1.get()));

  dctx2->getFirstFileEntry()->setPath("aria2.tar.gz");

  CPPUNIT_ASSERT(!gm.isSameFileBeingDownloaded(rg1.get()));
}

void RequestGroupManTest::testIsSameFileBeingDownloaded_paused()
{
  auto rg1 = std::make_shared<RequestGroup>(GroupId::create(),
                                            util::copy(option_));
  auto rg2 = std::make_shared<RequestGroup>(GroupId::create(),
                                            util::copy(option_));

  auto dctx1 = std::make_shared<DownloadContext>(0, 0, "aria2.tar.bz2");
  auto dctx2 = std::make_shared<DownloadContext>(0, 0, "aria2.tar.bz2");

  rg1->setDownloadContext(dctx1);
  rg2->setDownloadContext(dctx2);
  rg2->setPauseRequested(true);

  RequestGroupMan gm(std::vector<std::shared_ptr<RequestGroup>>(), 1,
                     option_.get());

  gm.addRequestGroup(rg1);
  gm.addReservedGroup(rg2);

  CPPUNIT_ASSERT(gm.isSameFileBeingDownloaded(rg1.get()));

  dctx2->getFirstFileEntry()->setPath("aria2.tar.gz");

  CPPUNIT_ASSERT(!gm.isSameFileBeingDownloaded(rg1.get()));
}

void RequestGroupManTest::testGetInitialCommands()
{
  // TODO implement later
}

void RequestGroupManTest::testSaveServerStat()
{
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 0,
                     option_.get());
  std::shared_ptr<ServerStat> ss_localhost(new ServerStat("localhost", "http"));
  rm.addServerStat(ss_localhost);
  File f(A2_TEST_OUT_DIR "/aria2_RequestGroupManTest_testSaveServerStat");
  if (f.exists()) {
    f.remove();
  }
  CPPUNIT_ASSERT(rm.saveServerStat(f.getPath()));
  CPPUNIT_ASSERT(f.isFile());

  f.remove();
  CPPUNIT_ASSERT(f.mkdirs());
  CPPUNIT_ASSERT(!rm.saveServerStat(f.getPath()));
}

void RequestGroupManTest::testLoadServerStat()
{
  File f(A2_TEST_OUT_DIR "/aria2_RequestGroupManTest_testLoadServerStat");
  std::ofstream o(f.getPath().c_str(), std::ios::binary);
  o << "host=localhost, protocol=http, dl_speed=0, last_updated=1219505257,"
    << "status=OK";
  o.close();

  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 0,
                     option_.get());
  std::cerr << "testLoadServerStat" << std::endl;
  CPPUNIT_ASSERT(rm.loadServerStat(f.getPath()));
  std::shared_ptr<ServerStat> ss_localhost =
      rm.findServerStat("localhost", "http");
  CPPUNIT_ASSERT(ss_localhost);
  CPPUNIT_ASSERT_EQUAL(std::string("localhost"), ss_localhost->getHostname());
}

void RequestGroupManTest::testChangeReservedGroupPosition()
{
  std::vector<std::shared_ptr<RequestGroup>> gs{
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)),
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)),
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)),
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_))};
  RequestGroupMan rm(gs, 0, option_.get());

  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), 0, OFFSET_MODE_SET));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), 1, OFFSET_MODE_SET));
  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), 10, OFFSET_MODE_SET));
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), -10, OFFSET_MODE_SET));

  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 0, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)2, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 1, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), -1, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), -10, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 1, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 10, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), -2, OFFSET_MODE_CUR));

  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), 0, OFFSET_MODE_END));
  CPPUNIT_ASSERT_EQUAL((size_t)2, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), -1, OFFSET_MODE_END));
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), -10, OFFSET_MODE_END));
  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), 10, OFFSET_MODE_END));

  CPPUNIT_ASSERT_EQUAL((size_t)4, rm.getReservedGroups().size());

  try {
    rm.changeReservedGroupPosition(GroupId::create()->getNumericId(), 0,
                                   OFFSET_MODE_CUR);
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (RecoverableException& e) {
    // success
  }
}

void RequestGroupManTest::testFillRequestGroupFromReserver()
{
  std::shared_ptr<RequestGroup> rgs[] = {
      createRequestGroup(0, 0, "foo1", "http://host/foo1", util::copy(option_)),
      createRequestGroup(0, 0, "foo2", "http://host/foo2", util::copy(option_)),
      createRequestGroup(0, 0, "foo3", "http://host/foo3", util::copy(option_)),
      // Intentionally same path/URI for first RequestGroup and set
      // length explicitly to do duplicate filename check.
      createRequestGroup(0, 10, "foo1", "http://host/foo1",
                         util::copy(option_)),
      createRequestGroup(0, 0, "foo4", "http://host/foo4", util::copy(option_)),
      createRequestGroup(0, 0, "foo5", "http://host/foo5",
                         util::copy(option_))};
  rgs[1]->setPauseRequested(true);
  for (const auto& i : rgs) {
    rgman_->addReservedGroup(i);
  }
  rgman_->fillRequestGroupFromReserver(e_.get());

  CPPUNIT_ASSERT_EQUAL((size_t)2, rgman_->getReservedGroups().size());
}

void RequestGroupManTest::testFillRequestGroupFromReserver_uriParser()
{
  std::shared_ptr<RequestGroup> rgs[] = {
      createRequestGroup(0, 0, "mem1", "http://mem1", util::copy(option_)),
      createRequestGroup(0, 0, "mem2", "http://mem2", util::copy(option_)),
  };
  rgs[0]->setPauseRequested(true);
  for (const auto& i : rgs) {
    rgman_->addReservedGroup(i);
  }

  std::shared_ptr<UriListParser> flp(
      new UriListParser(A2_TEST_DIR "/filelist2.txt"));
  rgman_->setUriListParser(flp);

  rgman_->fillRequestGroupFromReserver(e_.get());

  RequestGroupList::const_iterator itr;
  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman_->getReservedGroups().size());
  itr = rgman_->getReservedGroups().begin();
  CPPUNIT_ASSERT_EQUAL(rgs[0]->getGID(), (*itr)->getGID());
  CPPUNIT_ASSERT_EQUAL((size_t)3, rgman_->getRequestGroups().size());
}

void RequestGroupManTest::testFillRequestGroupFromReserver_skippedOwnsPath()
{
  std::string dir =
      A2_TEST_OUT_DIR "/aria2_RequestGroupManTest_skippedOwnsPath";
  File(dir).mkdirs();
  File(dir + "/x.1.bin").remove();
  File(dir + "/x.1.bin.ariam").remove();
  auto a = createRequestGroup(0, 0, dir + "/x.bin", "http://host/a",
                              util::copy(option_));
  a->setPauseRequested(true);
  auto opt = util::copy(option_);
  opt->put(PREF_AUTO_FILE_RENAMING, A2_V_TRUE);
  // The engine has no FileAllocationMan.
  opt->put(PREF_FILE_ALLOCATION, V_NONE);
  auto b = createRequestGroup(1024, 10, dir + "/x.bin", "http://host/b", opt);
  rgman_->addReservedGroup(a);
  rgman_->addReservedGroup(b);
  rgman_->fillRequestGroupFromReserver(e_.get());
  // A is skipped and stays in the queue, where it owns x.bin while B
  // starts, so B is renamed.
  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman_->getRequestGroups().size());
  CPPUNIT_ASSERT_EQUAL(b->getGID(),
                       (*rgman_->getRequestGroups().begin())->getGID());
  CPPUNIT_ASSERT_EQUAL((int)RequestGroup::STATE_ACTIVE, b->getState());
  CPPUNIT_ASSERT_EQUAL(dir + "/x.1.bin", b->getFirstFilePath());
  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman_->getReservedGroups().size());
  CPPUNIT_ASSERT_EQUAL(a->getGID(), getReservedGroup(rgman_, 0)->getGID());
}

namespace {
std::shared_ptr<RequestGroup> createSiteGroup(const std::string& name,
                                              const std::string& host,
                                              const std::shared_ptr<Option>& op)
{
  auto opt = util::copy(op);
  if (!host.empty()) {
    opt->put(PREF_REFERER, "https://" + host + "/page");
  }
  return createRequestGroup(0, 0, "sc_" + name, "http://host/" + name, opt);
}

void setSiteLimits(Option& option, const std::string& sites,
                   const std::string& defaultLimit)
{
  const auto& parser = OptionParser::getInstance();
  parser->find(PREF_SITE_CONCURRENT)->parse(option, sites);
  parser->find(PREF_SITE_CONCURRENT_DEFAULT)->parse(option, defaultLimit);
}

// Stands in for a started download with one running command.
void activate(RequestGroupMan* rgman, const std::shared_ptr<RequestGroup>& g)
{
  g->setState(RequestGroup::STATE_ACTIVE);
  g->setRequestGroupMan(rgman);
  g->increaseNumCommand();
  rgman->addRequestGroup(g);
}

std::string names(const RequestGroupList& list)
{
  std::string res;
  for (const auto& g : list) {
    if (!res.empty()) {
      res += " ";
    }
    res += g->getFirstFilePath().substr(3);
  }
  return res;
}
} // namespace

void RequestGroupManTest::testFillRequestGroupFromReserver_siteAcceptance()
{
  setSiteLimits(*option_, "a.test:1,b.test:3", "2");
  rgman_->setMaxConcurrentDownloads(6);
  // A subdomain of a listed site counts toward that site; an unlisted
  // site is its registrable domain.
  for (const auto& p : std::vector<std::pair<std::string, std::string>>{
           {"a1", "a.test"},
           {"a2", "members.a.test"},
           {"a3", "a.test"},
           {"b1", "b.test"},
           {"b2", "cdn.b.test"},
           {"b3", "b.test"},
           {"b4", "b.test"},
           {"c1", "c.test"},
           {"c2", "www.c.test"},
           {"c3", "c.test"}}) {
    rgman_->addReservedGroup(createSiteGroup(p.first, p.second, option_));
  }
  rgman_->fillRequestGroupFromReserver(e_.get());
  // b4 is skipped although slots are left: the B downloads started in
  // the same call count toward B's limit.
  CPPUNIT_ASSERT_EQUAL(std::string("a1 b1 b2 b3 c1 c2"),
                       names(rgman_->getRequestGroups()));
  CPPUNIT_ASSERT_EQUAL(std::string("a2 a3 b4 c3"),
                       names(rgman_->getReservedGroups()));
  for (const auto& g : rgman_->getReservedGroups()) {
    CPPUNIT_ASSERT_EQUAL((int)RequestGroup::STATE_WAITING, g->getState());
  }
}

void RequestGroupManTest::testFillRequestGroupFromReserver_siteSlotFrees()
{
  setSiteLimits(*option_, "a.test:1,b.test:3", "2");
  rgman_->setMaxConcurrentDownloads(6);
  std::vector<std::shared_ptr<RequestGroup>> running{
      createSiteGroup("a1", "a.test", option_),
      createSiteGroup("b1", "b.test", option_),
      createSiteGroup("b2", "b.test", option_),
      createSiteGroup("b3", "b.test", option_),
      createSiteGroup("c1", "c.test", option_),
      createSiteGroup("c2", "c.test", option_)};
  for (const auto& g : running) {
    activate(rgman_, g);
  }
  for (const auto& p : std::vector<std::pair<std::string, std::string>>{
           {"a2", "a.test"}, {"a3", "a.test"}, {"b4", "b.test"}, {"c3", "c.test"}}) {
    rgman_->addReservedGroup(createSiteGroup(p.first, p.second, option_));
  }
  rgman_->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL(std::string("a2 a3 b4 c3"),
                       names(rgman_->getReservedGroups()));

  // b2 ends, which frees a B slot and requests a queue check.
  rgman_->clearQueueCheck();
  running[2]->decreaseNumCommand();
  CPPUNIT_ASSERT(rgman_->queueCheckRequested());
  rgman_->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL(std::string("a1 b1 b3 c1 c2 b4"),
                       names(rgman_->getRequestGroups()));
  CPPUNIT_ASSERT_EQUAL(std::string("a2 a3 c3"),
                       names(rgman_->getReservedGroups()));
}

void RequestGroupManTest::testFillRequestGroupFromReserver_siteFifo()
{
  setSiteLimits(*option_, "a.test:1", "0");
  auto a1 = createSiteGroup("a1", "a.test", option_);
  activate(rgman_, a1);
  for (const auto& p : std::vector<std::pair<std::string, std::string>>{
           {"a2", "a.test"}, {"a3", "a.test"}, {"x1", "x.test"}, {"x2", "x.test"}}) {
    rgman_->addReservedGroup(createSiteGroup(p.first, p.second, option_));
  }
  // Downloads of an uncapped site queued after the capped site's
  // waiting downloads start.
  rgman_->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL(std::string("a1 x1 x2"),
                       names(rgman_->getRequestGroups()));
  CPPUNIT_ASSERT_EQUAL(std::string("a2 a3"),
                       names(rgman_->getReservedGroups()));
  // The capped site's waiting downloads keep their order.
  a1->decreaseNumCommand();
  rgman_->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL(std::string("x1 x2 a2"),
                       names(rgman_->getRequestGroups()));
  CPPUNIT_ASSERT_EQUAL(std::string("a3"), names(rgman_->getReservedGroups()));
}

void RequestGroupManTest::testFillRequestGroupFromReserver_siteCeiling()
{
  setSiteLimits(*option_, "a.test:5", "5");
  // max-concurrent-downloads is 3.
  for (const auto& p : std::vector<std::pair<std::string, std::string>>{
           {"a1", "a.test"}, {"c1", "c.test"}, {"a2", "a.test"}, {"c2", "c.test"}}) {
    rgman_->addReservedGroup(createSiteGroup(p.first, p.second, option_));
  }
  rgman_->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL(std::string("a1 c1 a2"),
                       names(rgman_->getRequestGroups()));
  CPPUNIT_ASSERT_EQUAL(std::string("c2"), names(rgman_->getReservedGroups()));
}

void RequestGroupManTest::testFillRequestGroupFromReserver_siteDefault()
{
  setSiteLimits(*option_, "", "1");
  rgman_->setMaxConcurrentDownloads(6);
  // Every unlisted site, and the no-referer site, has its own default
  // limit. An unlisted subdomain counts toward its registrable domain.
  for (const auto& p : std::vector<std::pair<std::string, std::string>>{
           {"c1", "c.test"},
           {"c2", "x.c.test"},
           {"d1", "d.test"},
           {"d2", "d.test"},
           {"n1", ""},
           {"n2", ""}}) {
    rgman_->addReservedGroup(createSiteGroup(p.first, p.second, option_));
  }
  rgman_->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL(std::string("c1 d1 n1"),
                       names(rgman_->getRequestGroups()));
  CPPUNIT_ASSERT_EQUAL(std::string("c2 d2 n2"),
                       names(rgman_->getReservedGroups()));
}

void RequestGroupManTest::testFillRequestGroupFromReserver_siteDefaultUnlimited()
{
  setSiteLimits(*option_, "a.test:1", "0");
  rgman_->setMaxConcurrentDownloads(4);
  for (const auto& p : std::vector<std::pair<std::string, std::string>>{
           {"c1", "c.test"}, {"c2", "c.test"}, {"n1", ""}, {"c3", "c.test"}}) {
    rgman_->addReservedGroup(createSiteGroup(p.first, p.second, option_));
  }
  rgman_->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL(std::string("c1 c2 n1 c3"),
                       names(rgman_->getRequestGroups()));
  CPPUNIT_ASSERT(rgman_->getReservedGroups().empty());
}

void RequestGroupManTest::testFillRequestGroupFromReserver_siteReclassify()
{
  setSiteLimits(*option_, "b.test:1", "0");
  activate(rgman_, createSiteGroup("b1", "b.test", option_));
  rgman_->addReservedGroup(createSiteGroup("x1", "x.b.test", option_));
  rgman_->fillRequestGroupFromReserver(e_.get());
  // x.b.test belongs to site b.test, which is at its limit.
  CPPUNIT_ASSERT_EQUAL(std::string("x1"), names(rgman_->getReservedGroups()));
  // Listing x.b.test moves the queued download to its own site.
  setSiteLimits(*option_, "b.test:1,x.b.test:1", "0");
  rgman_->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL(std::string("b1 x1"),
                       names(rgman_->getRequestGroups()));
  CPPUNIT_ASSERT(rgman_->getReservedGroups().empty());
}

void RequestGroupManTest::testFillRequestGroupFromReserver_siteUnset()
{
  // No site option is set: only max-concurrent-downloads (3) limits.
  for (const auto& name : {"a1", "a2", "a3", "a4"}) {
    rgman_->addReservedGroup(createSiteGroup(name, "a.test", option_));
  }
  rgman_->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL(std::string("a1 a2 a3"),
                       names(rgman_->getRequestGroups()));
  CPPUNIT_ASSERT_EQUAL(std::string("a4"), names(rgman_->getReservedGroups()));
}

void RequestGroupManTest::testFillRequestGroupFromReserver_siteParseError()
{
  setSiteLimits(*option_, "a.test:1", "0");
  activate(rgman_, createSiteGroup("a1", "a.test", option_));
  for (const auto& p : std::vector<std::pair<std::string, std::string>>{
           {"a2", "a.test"}, {"x1", "x.test"}, {"a3", "a.test"}}) {
    rgman_->addReservedGroup(createSiteGroup(p.first, p.second, option_));
  }
  // The next input file entry has an option line that does not parse.
  std::string path =
      A2_TEST_OUT_DIR "/aria2_RequestGroupManTest_siteParseError.txt";
  std::ofstream o(path.c_str(), std::ios::binary);
  o << "http://host/bad\n"
    << " split=bad\n";
  o.close();
  rgman_->setUriListParser(std::make_shared<UriListParser>(path));
  try {
    rgman_->fillRequestGroupFromReserver(e_.get());
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (RecoverableException& e) {
    // success
  }
  // The downloads skipped before the error stay queued in their order.
  CPPUNIT_ASSERT_EQUAL(std::string("a1 x1"),
                       names(rgman_->getRequestGroups()));
  CPPUNIT_ASSERT_EQUAL(std::string("a2 a3"),
                       names(rgman_->getReservedGroups()));
}

void RequestGroupManTest::testInsertReservedGroup()
{
  std::vector<std::shared_ptr<RequestGroup>> rgs1{
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_))),
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_)))};
  std::vector<std::shared_ptr<RequestGroup>> rgs2{
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_))),
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_)))};
  rgman_->insertReservedGroup(0, rgs1);
  CPPUNIT_ASSERT_EQUAL((size_t)2, rgman_->getReservedGroups().size());
  RequestGroupList::const_iterator itr;
  itr = rgman_->getReservedGroups().begin();
  CPPUNIT_ASSERT_EQUAL(rgs1[0]->getGID(), (*itr++)->getGID());
  CPPUNIT_ASSERT_EQUAL(rgs1[1]->getGID(), (*itr++)->getGID());

  rgman_->insertReservedGroup(1, rgs2);
  CPPUNIT_ASSERT_EQUAL((size_t)4, rgman_->getReservedGroups().size());
  itr = rgman_->getReservedGroups().begin();
  ++itr;
  CPPUNIT_ASSERT_EQUAL(rgs2[0]->getGID(), (*itr++)->getGID());
  CPPUNIT_ASSERT_EQUAL(rgs2[1]->getGID(), (*itr++)->getGID());
}

void RequestGroupManTest::testAddDownloadResult()
{
  std::string uri = "http://example.org";
  rgman_->setMaxDownloadResult(3);
  rgman_->addDownloadResult(createDownloadResult(error_code::TIME_OUT, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  CPPUNIT_ASSERT_EQUAL(error_code::TIME_OUT,
                       rgman_->getDownloadStat().getLastErrorResult());
}

void RequestGroupManTest::testHalt_interruptedDownload()
{
  for (bool force : {false, true}) {
    auto running = std::make_shared<RequestGroup>(GroupId::create(), option_);
    running->setDownloadContext(
        std::make_shared<DownloadContext>(1_k, 1_m, "/tmp/running"));
    running->initPieceStorage();
    // A mirror failed; the download went on from another one.
    running->setLastErrorCode(error_code::RESOURCE_NOT_FOUND);
    rgman_->addRequestGroup(running);

    auto failed = std::make_shared<RequestGroup>(GroupId::create(), option_);
    failed->setDownloadContext(
        std::make_shared<DownloadContext>(1_k, 1_m, "/tmp/failed"));
    failed->initPieceStorage();
    // A command halts its group when the download fails.
    failed->setLastErrorCode(error_code::RESOURCE_NOT_FOUND);
    failed->setHaltRequested(true);
    rgman_->addRequestGroup(failed);

    if (force) {
      rgman_->forceHalt();
    }
    else {
      rgman_->halt();
    }
    CPPUNIT_ASSERT_EQUAL(error_code::IN_PROGRESS,
                         running->createDownloadResult()->result);
    CPPUNIT_ASSERT_EQUAL(error_code::RESOURCE_NOT_FOUND,
                         failed->createDownloadResult()->result);
  }
}

} // namespace aria2
