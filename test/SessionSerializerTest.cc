#include "SessionSerializer.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>

#include <cppunit/extensions/HelperMacros.h>

#include "TestUtil.h"
#include "RequestGroupMan.h"
#include "RequestGroup.h"
#include "DownloadResult.h"
#include "GroupId.h"
#include "fmt.h"
#include "array_fun.h"
#include "download_helper.h"
#include "prefs.h"
#include "Option.h"
#include "a2functional.h"
#include "FileEntry.h"
#include "SelectEventPoll.h"
#include "DownloadEngine.h"
#include "SiteConcurrency.h"
#include "UriListParser.h"
#include "util.h"

namespace aria2 {

class SessionSerializerTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SessionSerializerTest);
  CPPUNIT_TEST(testSave);
  CPPUNIT_TEST(testSaveSkipsErrorDownload);
  CPPUNIT_TEST(testSaveSiteHint);
  CPPUNIT_TEST(testSaveOut);
  CPPUNIT_TEST(testSaveOutNotReadBack);
  CPPUNIT_TEST(testLoadOut);
  CPPUNIT_TEST_SUITE_END();

public:
  void testSave();
  void testSaveSkipsErrorDownload();
  void testSaveSiteHint();
  void testSaveOut();
  void testSaveOutNotReadBack();
  void testLoadOut();
};

CPPUNIT_TEST_SUITE_REGISTRATION(SessionSerializerTest);

namespace {
std::map<std::string, std::vector<std::string>>
readEntries(const std::string& filename)
{
  std::map<std::string, std::vector<std::string>> res;
  std::ifstream ss(filename.c_str(), std::ios::binary);
  std::string line;
  std::string uri;
  while (std::getline(ss, line)) {
    if (!line.empty() && line[0] == ' ') {
      res[uri].push_back(line);
    }
    else {
      uri = line;
      res[uri];
    }
  }
  return res;
}

std::vector<std::string> outLines(const std::vector<std::string>& lines)
{
  std::vector<std::string> res;
  for (const auto& line : lines) {
    if (line.compare(0, 5, " out=") == 0) {
      res.push_back(line);
    }
  }
  return res;
}

void saveGroups(const std::vector<std::shared_ptr<RequestGroup>>& groups,
                const std::string& filename)
{
  std::shared_ptr<Option> option(new Option());
  option->put(PREF_MAX_DOWNLOAD_RESULT, "10");
  RequestGroupMan rgman{groups, 1, option.get()};
  SessionSerializer s(&rgman);
  CPPUNIT_ASSERT(s.save(filename));
}
} // namespace

void SessionSerializerTest::testSave()
{
#if defined(ENABLE_BITTORRENT) && defined(ENABLE_METALINK)
  std::vector<std::string> uris{
      "http://localhost/file", "http://mirror/file",
      A2_TEST_DIR "/test.torrent", A2_TEST_DIR "/serialize_session.meta4",
      "magnet:?xt=urn:btih:248D0A1CD08284299DE78D5C1ED359BB46717D8C"};
  std::vector<std::shared_ptr<RequestGroup>> result;
  std::shared_ptr<Option> option(new Option());
  option->put(PREF_DIR, "/tmp");
  createRequestGroupForUri(result, option, uris);
  CPPUNIT_ASSERT_EQUAL((size_t)5, result.size());
  result[4]->getOption()->put(PREF_PAUSE, A2_V_TRUE);
  option->put(PREF_MAX_DOWNLOAD_RESULT, "10");
  RequestGroupMan rgman{result, 1, option.get()};
  SessionSerializer s(&rgman);
  std::shared_ptr<DownloadResult> drs[] = {
      // REMOVED downloads will not be saved.
      createDownloadResult(error_code::REMOVED, "http://removed"),
      // Error downloads will not be saved.
      createDownloadResult(error_code::TIME_OUT, "http://error"),
      createDownloadResult(error_code::FINISHED, "http://finished"),
      createDownloadResult(error_code::FINISHED, "http://force-save")};
  // This URI will be discarded because same URI exists in remaining
  // URIs.
  drs[3]->fileEntries[0]->getRemainingUris().push_back("http://force-save");
  drs[3]->fileEntries[0]->getRemainingUris().push_back("http://force-save3");
  // This URI will be discarded because same URI exists in remaining
  // URIs.
  drs[3]->fileEntries[0]->getSpentUris().push_back("http://force-save");
  drs[3]->fileEntries[0]->getSpentUris().push_back("http://force-save2");

  drs[3]->option->put(PREF_FORCE_SAVE, A2_V_TRUE);
  for (size_t i = 0; i < sizeof(drs) / sizeof(drs[0]); ++i) {
    rgman.addDownloadResult(drs[i]);
  }

  DownloadEngine e(make_unique<SelectEventPoll>());
  e.setOption(option.get());
  rgman.fillRequestGroupFromReserver(&e);
  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman.getRequestGroups().size());

  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSave";
  s.save(filename);
  std::ifstream ss(filename.c_str(), std::ios::binary);
  std::string line;
  std::getline(ss, line);
  // finished and force-save option
  CPPUNIT_ASSERT_EQUAL(std::string("http://force-save\thttp://force-save3\t"
                                   "http://force-save2\t"),
                       line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(fmt(" gid=%s", drs[3]->gid->toHex().c_str()), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" force-save=true"), line);
  // Check active download is also saved
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(uris[0] + "\t" + uris[1] + "\t", line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(
      fmt(" gid=%s", GroupId::toHex(result[0]->getGID()).c_str()), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(uris[2], line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(
      fmt(" gid=%s", GroupId::toHex(result[1]->getGID()).c_str()), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(uris[3], line);
  std::getline(ss, line);
  // local metalink download does not save meaningful GID
  CPPUNIT_ASSERT(fmt(" gid=%s", GroupId::toHex(result[2]->getGID()).c_str()) !=
                 line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(uris[4], line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(
      fmt(" gid=%s", GroupId::toHex(result[4]->getGID()).c_str()), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" pause=true"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT(!ss);
#endif // defined(ENABLE_BITTORRENT) && defined(ENABLE_METALINK)
}

void SessionSerializerTest::testSaveSkipsErrorDownload()
{
  std::shared_ptr<DownloadResult> drs[] = {
      createDownloadResult(error_code::TIME_OUT, "http://error"),
      createDownloadResult(error_code::RESOURCE_NOT_FOUND, "http://not-found")};
  for (const auto& dr : drs) {
    dr->fileEntries[0]->getSpentUris().swap(
        dr->fileEntries[0]->getRemainingUris());
  }
  // --save-not-found does not save an error download.
  drs[1]->option->put(PREF_SAVE_NOT_FOUND, A2_V_TRUE);
  std::vector<std::shared_ptr<RequestGroup>> groups{
      createRequestGroup(0, 0, "waiting", "http://waiting",
                         std::make_shared<Option>()),
      createRequestGroup(0, 0, "paused", "http://paused",
                         std::make_shared<Option>())};
  groups[1]->setPauseRequested(true);
  std::shared_ptr<Option> option(new Option());
  option->put(PREF_MAX_DOWNLOAD_RESULT, "10");
  RequestGroupMan rgman{groups, 1, option.get()};
  for (const auto& dr : drs) {
    rgman.addDownloadResult(dr);
  }
  SessionSerializer s(&rgman);
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSaveSkipsErrorDownload";
  CPPUNIT_ASSERT(s.save(filename));
  std::ifstream ss(filename.c_str(), std::ios::binary);
  std::string line;
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string("http://waiting\t"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(
      fmt(" gid=%s", GroupId::toHex(groups[0]->getGID()).c_str()), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string("http://paused\t"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(
      fmt(" gid=%s", GroupId::toHex(groups[1]->getGID()).c_str()), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" pause=true"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT(!ss);
}

void SessionSerializerTest::testSaveSiteHint()
{
  const std::string hint = "https://Members.Example.co.uk/x";
  SiteConcurrency sites({}, 0);
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSaveSiteHint";
  std::string gid;
  std::string site;
  {
    auto option = std::make_shared<Option>();
    option->put(PREF_SITE_HINT, hint);
    std::vector<std::shared_ptr<RequestGroup>> groups{
        createRequestGroup(0, 0, "waiting", "http://waiting", option)};
    gid = GroupId::toHex(groups[0]->getGID());
    site = sites.classify(*groups[0]->getOption()).site;
    CPPUNIT_ASSERT_EQUAL(std::string("example.co.uk"), site);
    std::shared_ptr<Option> globalOption(new Option());
    globalOption->put(PREF_MAX_DOWNLOAD_RESULT, "10");
    RequestGroupMan rgman{groups, 1, globalOption.get()};
    SessionSerializer s(&rgman);
    CPPUNIT_ASSERT(s.save(filename));
  }
  {
    std::ifstream ss(filename.c_str(), std::ios::binary);
    std::string line;
    std::getline(ss, line);
    CPPUNIT_ASSERT_EQUAL(std::string("http://waiting\t"), line);
    std::getline(ss, line);
    CPPUNIT_ASSERT_EQUAL(" gid=" + gid, line);
    std::getline(ss, line);
    CPPUNIT_ASSERT_EQUAL(" site-hint=" + hint, line);
    std::getline(ss, line);
    CPPUNIT_ASSERT(!ss);
  }
  // The saved download is gone, so its GID is free for the restored
  // one, as at a restart.
  UriListParser parser(filename);
  std::vector<std::shared_ptr<RequestGroup>> restored;
  Option templateOption;
  CPPUNIT_ASSERT(createRequestGroupFromUriListParser(restored, &templateOption,
                                                     &parser));
  CPPUNIT_ASSERT_EQUAL((size_t)1, restored.size());
  CPPUNIT_ASSERT_EQUAL(gid, GroupId::toHex(restored[0]->getGID()));
  CPPUNIT_ASSERT_EQUAL(hint, restored[0]->getOption()->get(PREF_SITE_HINT));
  CPPUNIT_ASSERT_EQUAL(site, sites.classify(*restored[0]->getOption()).site);
}

void SessionSerializerTest::testSaveOut()
{
  const std::string dir = A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_out";
  auto option = std::make_shared<Option>();
  option->put(PREF_DIR, dir);
  auto renamedOption = util::copy(option);
  renamedOption->put(PREF_OUT, "x.bin");
  std::vector<std::shared_ptr<RequestGroup>> groups{
      createRequestGroup(0, 0, dir + "/a.bin", "http://host/a",
                         util::copy(option)),
      // The response path replaces the out option.
      createRequestGroup(0, 0, dir + "/x.1.bin", "http://host/x",
                         renamedOption)};
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSaveOut";
  saveGroups(groups, filename);
  auto entries = readEntries(filename);
  CPPUNIT_ASSERT_EQUAL((size_t)2, entries.size());
  auto outs = outLines(entries["http://host/a\t"]);
  CPPUNIT_ASSERT_EQUAL((size_t)1, outs.size());
  CPPUNIT_ASSERT_EQUAL(std::string(" out=a.bin"), outs[0]);
  outs = outLines(entries["http://host/x\t"]);
  CPPUNIT_ASSERT_EQUAL((size_t)1, outs.size());
  CPPUNIT_ASSERT_EQUAL(std::string(" out=x.1.bin"), outs[0]);
}

void SessionSerializerTest::testSaveOutNotReadBack()
{
  const std::string dir = A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_out";
  auto option = std::make_shared<Option>();
  option->put(PREF_DIR, dir);
  // No path; paths whose name the session parser strips or the out
  // option expands.
  std::vector<std::shared_ptr<RequestGroup>> groups{
      createRequestGroup(0, 0, "", "http://host/none", util::copy(option)),
      createRequestGroup(0, 0, dir + "/ a.bin", "http://host/lead",
                         util::copy(option)),
      createRequestGroup(0, 0, dir + "/a.bin ", "http://host/trail",
                         util::copy(option)),
      createRequestGroup(0, 0, dir + "/${HOME}.bin", "http://host/home",
                         util::copy(option))};
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSaveOutNotReadBack";
  saveGroups(groups, filename);
  auto entries = readEntries(filename);
  CPPUNIT_ASSERT_EQUAL((size_t)4, entries.size());
  for (const auto& entry : entries) {
    CPPUNIT_ASSERT(outLines(entry.second).empty());
    CPPUNIT_ASSERT(std::find(std::begin(entry.second), std::end(entry.second),
                             " dir=" + dir) != std::end(entry.second));
  }
}

void SessionSerializerTest::testLoadOut()
{
  const std::string dir = A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_out";
  const std::string path = dir + "/a.bin";
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testLoadOut";
  {
    auto option = std::make_shared<Option>();
    option->put(PREF_DIR, dir);
    saveGroups({createRequestGroup(0, 0, path, "http://host/a", option)},
               filename);
  }
  // The saved download is gone, so its GID is free for the restored
  // one, as at a restart.
  UriListParser parser(filename);
  std::vector<std::shared_ptr<RequestGroup>> restored;
  Option templateOption;
  CPPUNIT_ASSERT(createRequestGroupFromUriListParser(restored, &templateOption,
                                                     &parser));
  CPPUNIT_ASSERT_EQUAL((size_t)1, restored.size());
  CPPUNIT_ASSERT_EQUAL(path, restored[0]->getFirstFilePath());

  // Before any response, the restored download owns its path while
  // waiting and while paused.
  auto other = createRequestGroup(0, 0, path, "http://host/b",
                                  std::make_shared<Option>());
  std::shared_ptr<Option> globalOption(new Option());
  globalOption->put(PREF_MAX_DOWNLOAD_RESULT, "10");
  RequestGroupMan rgman{restored, 1, globalOption.get()};
  CPPUNIT_ASSERT(rgman.isFileOwned(path, other.get()));
  restored[0]->setPauseRequested(true);
  CPPUNIT_ASSERT(rgman.isFileOwned(path, other.get()));
}

} // namespace aria2
