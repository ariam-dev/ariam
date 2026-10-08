#include "RequestGroup.h"

#include <cppunit/extensions/HelperMacros.h>

#include "Option.h"
#include "DownloadContext.h"
#include "FileEntry.h"
#include "PieceStorage.h"
#include "DownloadResult.h"
#include "RequestGroupMan.h"
#include "DefaultBtProgressInfoFile.h"
#include "File.h"
#include "TestUtil.h"

namespace aria2 {

class RequestGroupTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(RequestGroupTest);
  CPPUNIT_TEST(testGetFirstFilePath);
  CPPUNIT_TEST(testTryAutoFileRenaming);
  CPPUNIT_TEST(testTryAutoFileRenaming_activeDownload);
  CPPUNIT_TEST(testTryAutoFileRenaming_pausedDownload);
  CPPUNIT_TEST(testAdjustFilename_pausedDownload);
  CPPUNIT_TEST(testAdjustFilename_controlFileSuffix);
  CPPUNIT_TEST(testCreateDownloadResult);
  CPPUNIT_TEST_SUITE_END();

private:
  std::shared_ptr<Option> option_;

public:
  void setUp() { option_.reset(new Option()); }

  void testGetFirstFilePath();
  void testTryAutoFileRenaming();
  void testTryAutoFileRenaming_activeDownload();
  void testTryAutoFileRenaming_pausedDownload();
  void testAdjustFilename_pausedDownload();
  void testAdjustFilename_controlFileSuffix();
  void testCreateDownloadResult();
};

CPPUNIT_TEST_SUITE_REGISTRATION(RequestGroupTest);

void RequestGroupTest::testGetFirstFilePath()
{
  std::shared_ptr<DownloadContext> ctx(
      new DownloadContext(1_k, 1_k, "/tmp/myfile"));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);

  CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile"), group.getFirstFilePath());

  group.markInMemoryDownload();

  CPPUNIT_ASSERT_EQUAL(std::string("[MEMORY]myfile"), group.getFirstFilePath());
}

void RequestGroupTest::testTryAutoFileRenaming()
{
  std::shared_ptr<DownloadContext> ctx(
      new DownloadContext(1_k, 1_k, "/tmp/myfile"));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);

  option_->put(PREF_AUTO_FILE_RENAMING, "false");
  try {
    group.tryAutoFileRenaming();
  }
  catch (const Exception& ex) {
    CPPUNIT_ASSERT_EQUAL(error_code::FILE_ALREADY_EXISTS, ex.getErrorCode());
  }

  option_->put(PREF_AUTO_FILE_RENAMING, "true");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile.1"), group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp/myfile.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile.1.txt"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/myfile");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/myfile.1"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/myfile.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/myfile.1.txt"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath(".bashrc");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string(".bashrc.1"), group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath(".bashrc.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string(".bashrc.1.txt"), group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/.bashrc");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/.bashrc.1"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/.bashrc.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/.bashrc.1.txt"),
                       group.getFirstFilePath());
}

void RequestGroupTest::testTryAutoFileRenaming_activeDownload()
{
  const std::string path =
      A2_TEST_OUT_DIR "/aria2_RequestGroupTest_activeDownload/myfile.txt";
  const std::string path1 =
      A2_TEST_OUT_DIR "/aria2_RequestGroupTest_activeDownload/myfile.1.txt";
  const std::string path2 =
      A2_TEST_OUT_DIR "/aria2_RequestGroupTest_activeDownload/myfile.2.txt";
  // myfile.1.txt is a partial download with its control file.
  createFile(path1, 0);
  createFile(path1 + ".aria2", 0);
  File(path2).remove();
  option_->put(PREF_AUTO_FILE_RENAMING, "true");

  auto ctx = std::make_shared<DownloadContext>(1_k, 1_k, path);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(ctx);

  auto otherCtx = std::make_shared<DownloadContext>(1_k, 1_k, path1);
  auto other = std::make_shared<RequestGroup>(GroupId::create(), option_);
  other->setDownloadContext(otherCtx);

  RequestGroupMan gm(std::vector<std::shared_ptr<RequestGroup>>(), 1,
                     option_.get());
  gm.addRequestGroup(other);
  gm.addRequestGroup(group);
  group->setRequestGroupMan(&gm);

  group->tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(path2, group->getFirstFilePath());

  // Only myfile.txt is owned: myfile.1.txt is resumed.
  otherCtx->getFirstFileEntry()->setPath(path);
  ctx->getFirstFileEntry()->setPath(path);
  group->tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(path1, group->getFirstFilePath());
}

void RequestGroupTest::testTryAutoFileRenaming_pausedDownload()
{
  const std::string path =
      A2_TEST_OUT_DIR "/aria2_RequestGroupTest_pausedDownload/myfile.txt";
  const std::string path1 =
      A2_TEST_OUT_DIR "/aria2_RequestGroupTest_pausedDownload/myfile.1.txt";
  const std::string path2 =
      A2_TEST_OUT_DIR "/aria2_RequestGroupTest_pausedDownload/myfile.2.txt";
  // myfile.1.txt is a partial download with its control file.
  createFile(path1, 0);
  createFile(path1 + ".aria2", 0);
  File(path2).remove();
  option_->put(PREF_AUTO_FILE_RENAMING, "true");

  auto ctx = std::make_shared<DownloadContext>(1_k, 1_k, path);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(ctx);

  auto otherCtx = std::make_shared<DownloadContext>(1_k, 1_k, path1);
  auto other = std::make_shared<RequestGroup>(GroupId::create(), option_);
  other->setDownloadContext(otherCtx);
  other->setPauseRequested(true);

  RequestGroupMan gm(std::vector<std::shared_ptr<RequestGroup>>(), 1,
                     option_.get());
  gm.addReservedGroup(other);
  gm.addRequestGroup(group);
  group->setRequestGroupMan(&gm);

  group->tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(path2, group->getFirstFilePath());
}

void RequestGroupTest::testAdjustFilename_pausedDownload()
{
  const std::string path =
      A2_TEST_OUT_DIR "/aria2_RequestGroupTest_adjustFilename/myfile.txt";
  const std::string path1 =
      A2_TEST_OUT_DIR "/aria2_RequestGroupTest_adjustFilename/myfile.1.txt";
  // myfile.txt is the paused download's partial file with its control file.
  createFile(path, 1_k);
  createFile(path + ".aria2", 0);
  File(path1).remove();
  File(path1 + ".aria2").remove();
  option_->put(PREF_AUTO_FILE_RENAMING, "true");

  auto otherCtx = std::make_shared<DownloadContext>(1_k, 1_k, path);
  auto other = std::make_shared<RequestGroup>(GroupId::create(), option_);
  other->setDownloadContext(otherCtx);
  other->setPauseRequested(true);

  auto ctx = std::make_shared<DownloadContext>(1_k, 1_k, path);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(ctx);

  RequestGroupMan gm(std::vector<std::shared_ptr<RequestGroup>>(), 1,
                     option_.get());
  gm.addReservedGroup(other);
  gm.addRequestGroup(group);
  group->setRequestGroupMan(&gm);

  // The paused download owns myfile.txt: a download of the same name and
  // length does not resume its control file.
  group->adjustFilename(
      std::make_shared<DefaultBtProgressInfoFile>(ctx, nullptr, option_.get()));
  CPPUNIT_ASSERT_EQUAL(path1, group->getFirstFilePath());
  CPPUNIT_ASSERT_EQUAL(path, other->getFirstFilePath());
  CPPUNIT_ASSERT(File(path + ".aria2").exists());
  CPPUNIT_ASSERT_EQUAL((int64_t)1_k, File(path).size());
}

void RequestGroupTest::testAdjustFilename_controlFileSuffix()
{
  const std::string dir =
      A2_TEST_OUT_DIR "/aria2_RequestGroupTest_controlFileSuffix";
  File(dir).mkdirs();
  for (const auto& name : {"f.bin.aria2_", "f.bin.aria2_.aria2",
                           "f.bin.aria2__temp_", "f.bin.aria2__temp_.aria2",
                           "f.bin.1.aria2_", "f.bin.1.aria2_.aria2",
                           "f.bin.ARIA2", "f.bin.ARIA2.aria2"}) {
    File(dir + "/" + name).remove();
  }
  option_->put(PREF_AUTO_FILE_RENAMING, "true");

  auto ctx = std::make_shared<DownloadContext>(1_k, 1_k, dir + "/f.bin.aria2");
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(ctx);

  auto infoFile =
      std::make_shared<DefaultBtProgressInfoFile>(ctx, nullptr, option_.get());
  group->adjustFilename(infoFile);
  CPPUNIT_ASSERT_EQUAL(dir + "/f.bin.aria2_", group->getFirstFilePath());
  CPPUNIT_ASSERT_EQUAL(dir + "/f.bin.aria2_.aria2", infoFile->getFilename());

  ctx->getFirstFileEntry()->setPath(dir + "/f.bin.aria2__temp");
  group->adjustFilename(
      std::make_shared<DefaultBtProgressInfoFile>(ctx, nullptr, option_.get()));
  CPPUNIT_ASSERT_EQUAL(dir + "/f.bin.aria2__temp_", group->getFirstFilePath());

  createFile(dir + "/f.bin.aria2_", 0);
  ctx->getFirstFileEntry()->setPath(dir + "/f.bin.aria2");
  group->adjustFilename(
      std::make_shared<DefaultBtProgressInfoFile>(ctx, nullptr, option_.get()));
  CPPUNIT_ASSERT_EQUAL(dir + "/f.bin.1.aria2_", group->getFirstFilePath());

  ctx->getFirstFileEntry()->setPath(dir + "/f.bin.ARIA2");
  group->adjustFilename(
      std::make_shared<DefaultBtProgressInfoFile>(ctx, nullptr, option_.get()));
  CPPUNIT_ASSERT_EQUAL(dir + "/f.bin.ARIA2", group->getFirstFilePath());
}

void RequestGroupTest::testCreateDownloadResult()
{
  std::shared_ptr<DownloadContext> ctx(
      new DownloadContext(1_k, 1_m, "/tmp/myfile"));
  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.initPieceStorage();
  {
    std::shared_ptr<DownloadResult> result = group.createDownloadResult();

    CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile"),
                         result->fileEntries[0]->getPath());
    CPPUNIT_ASSERT_EQUAL((int64_t)1_m,
                         result->fileEntries.back()->getLastOffset());
    CPPUNIT_ASSERT_EQUAL((uint64_t)0, result->sessionDownloadLength);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, result->sessionTime.count());
    // result is UNKNOWN_ERROR if download has not completed and no specific
    // error has been reported
    CPPUNIT_ASSERT_EQUAL(error_code::UNKNOWN_ERROR, result->result);

    // if haltReason is set to RequestGroup::USER_REQUEST, download
    // result will become REMOVED.
    group.setHaltRequested(true, RequestGroup::USER_REQUEST);
    result = group.createDownloadResult();
    CPPUNIT_ASSERT_EQUAL(error_code::REMOVED, result->result);
    // if haltReason is set to RequestGroup::SHUTDOWN_SIGNAL, download
    // result will become IN_PROGRESS.
    group.setHaltRequested(true, RequestGroup::SHUTDOWN_SIGNAL);
    result = group.createDownloadResult();
    CPPUNIT_ASSERT_EQUAL(error_code::IN_PROGRESS, result->result);
  }
  {
    group.setLastErrorCode(error_code::RESOURCE_NOT_FOUND);

    std::shared_ptr<DownloadResult> result = group.createDownloadResult();

    CPPUNIT_ASSERT_EQUAL(error_code::RESOURCE_NOT_FOUND, result->result);
  }
  {
    group.getPieceStorage()->markAllPiecesDone();

    std::shared_ptr<DownloadResult> result = group.createDownloadResult();

    CPPUNIT_ASSERT_EQUAL(error_code::FINISHED, result->result);
  }
}

} // namespace aria2
