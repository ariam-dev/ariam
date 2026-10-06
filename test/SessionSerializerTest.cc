#include "SessionSerializer.h"

#include <iostream>
#include <fstream>

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

namespace aria2 {

class SessionSerializerTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SessionSerializerTest);
  CPPUNIT_TEST(testSave);
  CPPUNIT_TEST(testSaveSkipsErrorDownload);
  CPPUNIT_TEST_SUITE_END();

public:
  void testSave();
  void testSaveSkipsErrorDownload();
};

CPPUNIT_TEST_SUITE_REGISTRATION(SessionSerializerTest);

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

} // namespace aria2
