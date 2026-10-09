#include "RpcMethod.h"

#include <map>

#include <cppunit/extensions/HelperMacros.h>

#include "DownloadEngine.h"
#include "SelectEventPoll.h"
#include "Option.h"
#include "RequestGroupMan.h"
#include "RequestGroup.h"
#include "RpcMethodImpl.h"
#include "OptionParser.h"
#include "OptionHandler.h"
#include "RpcRequest.h"
#include "RpcResponse.h"
#include "prefs.h"
#include "TestUtil.h"
#include "DownloadContext.h"
#include "FeatureConfig.h"
#include "util.h"
#include "array_fun.h"
#include "download_helper.h"
#include "FileEntry.h"
#include "RpcMethodFactory.h"
#include "FillRequestGroupCommand.h"
#include "SiteConcurrency.h"
#include "rpc_helper.h"
#ifdef ENABLE_BITTORRENT
#  include "BtRegistry.h"
#  include "BtRuntime.h"
#  include "bittorrent_helper.h"
#endif // ENABLE_BITTORRENT

namespace aria2 {

namespace rpc {

class RpcMethodTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(RpcMethodTest);
  CPPUNIT_TEST(testAuthorize);
  CPPUNIT_TEST(testAddUri);
  CPPUNIT_TEST(testAddUri_withoutUri);
  CPPUNIT_TEST(testAddUri_notUri);
  CPPUNIT_TEST(testAddUri_withBadOption);
  CPPUNIT_TEST(testAddUri_withPosition);
  CPPUNIT_TEST(testAddUri_withBadPosition);
  CPPUNIT_TEST(testAddUri_withSiteHint);
  CPPUNIT_TEST(testAddUri_withBadSiteHint);
#ifdef ENABLE_BITTORRENT
  CPPUNIT_TEST(testAddTorrent);
  CPPUNIT_TEST(testAddTorrent_withoutTorrent);
  CPPUNIT_TEST(testAddTorrent_notBase64Torrent);
  CPPUNIT_TEST(testAddTorrent_withPosition);
#endif // ENABLE_BITTORRENT
#ifdef ENABLE_METALINK
  CPPUNIT_TEST(testAddMetalink);
  CPPUNIT_TEST(testAddMetalink_withoutMetalink);
  CPPUNIT_TEST(testAddMetalink_notBase64Metalink);
  CPPUNIT_TEST(testAddMetalink_withPosition);
#endif // ENABLE_METALINK
  CPPUNIT_TEST(testGetOption);
  CPPUNIT_TEST(testChangeOption);
  CPPUNIT_TEST(testChangeOption_withBadOption);
  CPPUNIT_TEST(testChangeOption_withNotAllowedOption);
  CPPUNIT_TEST(testChangeOption_withoutGid);
  CPPUNIT_TEST(testChangeOption_siteOfWaiting);
  CPPUNIT_TEST(testChangeOption_siteHint);
  CPPUNIT_TEST(testChangeGlobalOption);
  CPPUNIT_TEST(testChangeGlobalOption_raiseSiteLimit);
  CPPUNIT_TEST(testChangeGlobalOption_withBadOption);
  CPPUNIT_TEST(testChangeGlobalOption_withNotAllowedOption);
  CPPUNIT_TEST(testTellStatus_withoutGid);
  CPPUNIT_TEST(testTellWaiting);
  CPPUNIT_TEST(testTellWaiting_fail);
  CPPUNIT_TEST(testTellWaiting_int64Num);
  CPPUNIT_TEST(testGetVersion);
  CPPUNIT_TEST(testNoSuchMethod);
  CPPUNIT_TEST(testGatherStoppedDownload);
#ifdef ENABLE_BITTORRENT
  CPPUNIT_TEST(testGatherStoppedDownload_bt);
#endif // ENABLE_BITTORRENT
  CPPUNIT_TEST(testGatherProgressCommon);
  CPPUNIT_TEST(testGatherProgressCommon_siteAndName);
  CPPUNIT_TEST(testGatherStoppedDownload_siteAndName);
  CPPUNIT_TEST(testTellStatus_siteAndName);
  CPPUNIT_TEST(testGetSiteSummary);
  CPPUNIT_TEST(testGetSiteSummary_unset);
  CPPUNIT_TEST(testGetSiteSummary_authorize);
#ifdef ENABLE_BITTORRENT
  CPPUNIT_TEST(testGatherBitTorrentMetadata);
#endif // ENABLE_BITTORRENT
  CPPUNIT_TEST(testChangePosition);
  CPPUNIT_TEST(testChangePosition_fail);
  CPPUNIT_TEST(testGetSessionInfo);
  CPPUNIT_TEST(testChangeUri);
  CPPUNIT_TEST(testChangeUri_fail);
  CPPUNIT_TEST(testPause);
  CPPUNIT_TEST(testSystemMulticall);
  CPPUNIT_TEST(testSystemMulticall_fail);
  CPPUNIT_TEST(testSystemListMethods);
  CPPUNIT_TEST(testSystemListNotifications);
  CPPUNIT_TEST(testPassphrase_allowlist);
  CPPUNIT_TEST(testPassphrase_deniedMethods);
  CPPUNIT_TEST(testPassphrase_tokenCall);
  CPPUNIT_TEST(testPassphrase_changeGlobalOption);
  CPPUNIT_TEST(testPassphrase_systemMethods);
  CPPUNIT_TEST(testPassphrase_multicall);
  CPPUNIT_TEST(testPassphrase_redaction);
  CPPUNIT_TEST_SUITE_END();

private:
  std::shared_ptr<DownloadEngine> e_;
  std::shared_ptr<Option> option_;

public:
  void setUp()
  {
    option_ = std::make_shared<Option>();
    option_->put(PREF_DIR, A2_TEST_OUT_DIR "/aria2_RpcMethodTest");
    option_->put(PREF_PIECE_LENGTH, "1048576");
    option_->put(PREF_MAX_DOWNLOAD_RESULT, "10");
    File(option_->get(PREF_DIR)).mkdirs();
    e_ = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
    e_->setOption(option_.get());
    e_->setRequestGroupMan(make_unique<RequestGroupMan>(
        std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get()));
  }

  void testAuthorize();
  void testAddUri();
  void testAddUri_withoutUri();
  void testAddUri_notUri();
  void testAddUri_withBadOption();
  void testAddUri_withPosition();
  void testAddUri_withBadPosition();
  void testAddUri_withSiteHint();
  void testAddUri_withBadSiteHint();
#ifdef ENABLE_BITTORRENT
  void testAddTorrent();
  void testAddTorrent_withoutTorrent();
  void testAddTorrent_notBase64Torrent();
  void testAddTorrent_withPosition();
#endif // ENABLE_BITTORRENT
#ifdef ENABLE_METALINK
  void testAddMetalink();
  void testAddMetalink_withoutMetalink();
  void testAddMetalink_notBase64Metalink();
  void testAddMetalink_withPosition();
#endif // ENABLE_METALINK
  void testGetOption();
  void testChangeOption();
  void testChangeOption_withBadOption();
  void testChangeOption_withNotAllowedOption();
  void testChangeOption_withoutGid();
  void testChangeOption_siteOfWaiting();
  void testChangeOption_siteHint();
  void testChangeGlobalOption();
  void testChangeGlobalOption_raiseSiteLimit();
  void testChangeGlobalOption_withBadOption();
  void testChangeGlobalOption_withNotAllowedOption();
  void testTellStatus_withoutGid();
  void testTellWaiting();
  void testTellWaiting_fail();
  void testTellWaiting_int64Num();
  void testGetVersion();
  void testNoSuchMethod();
  void testGatherStoppedDownload();
#ifdef ENABLE_BITTORRENT
  void testGatherStoppedDownload_bt();
#endif // ENABLE_BITTORRENT
  void testGatherProgressCommon();
  void testGatherProgressCommon_siteAndName();
  void testGatherStoppedDownload_siteAndName();
  void testTellStatus_siteAndName();
  void testGetSiteSummary();
  void testGetSiteSummary_unset();
  void testGetSiteSummary_authorize();
#ifdef ENABLE_BITTORRENT
  void testGatherBitTorrentMetadata();
#endif // ENABLE_BITTORRENT
  void testChangePosition();
  void testChangePosition_fail();
  void testGetSessionInfo();
  void testChangeUri();
  void testChangeUri_fail();
  void testPause();
  void testSystemMulticall();
  void testSystemMulticall_fail();
  void testSystemListMethods();
  void testSystemListNotifications();
  void testPassphrase_allowlist();
  void testPassphrase_deniedMethods();
  void testPassphrase_tokenCall();
  void testPassphrase_changeGlobalOption();
  void testPassphrase_systemMethods();
  void testPassphrase_multicall();
  void testPassphrase_redaction();
};

CPPUNIT_TEST_SUITE_REGISTRATION(RpcMethodTest);

namespace {
std::string getString(const Dict* dict, const std::string& key)
{
  return downcast<String>(dict->get(key))->s();
}
} // namespace

namespace {
RpcRequest createReq(std::string methodName)
{
  return {std::move(methodName), List::g()};
}
} // namespace

void RpcMethodTest::testAuthorize()
{
  // Select RPC method which takes non-string parameter to make sure
  // that token: prefixed parameter is stripped before the call.
  TellActiveRpcMethod m;
  // no secret token set and no token: prefixed parameter is given
  {
    auto req = createReq(TellActiveRpcMethod::getMethodName());
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  // no secret token set and token: prefixed parameter is given
  {
    auto req = createReq(GetVersionRpcMethod::getMethodName());
    req.params->append("token:foo");
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  e_->getOption()->put(PREF_RPC_SECRET, "foo");
  // secret token set and no token: prefixed parameter is given
  {
    auto req = createReq(GetVersionRpcMethod::getMethodName());
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(1, res.code);
  }
  // secret token set and token: prefixed parameter is given
  {
    auto req = createReq(GetVersionRpcMethod::getMethodName());
    req.params->append("token:foo");
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  // secret token set and bad token: prefixed parameter is given
  {
    auto req = createReq(GetVersionRpcMethod::getMethodName());
    req.params->append("token:foo2");
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(1, res.code);
  }
}

void RpcMethodTest::testAddUri()
{
  AddUriRpcMethod m;
  {
    auto req = createReq(AddUriRpcMethod::getMethodName());
    auto urisParam = List::g();
    urisParam->append("http://localhost/");
    req.params->append(std::move(urisParam));
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    const RequestGroupList& rgs = e_->getRequestGroupMan()->getReservedGroups();
    CPPUNIT_ASSERT_EQUAL((size_t)1, rgs.size());
    CPPUNIT_ASSERT_EQUAL(std::string("http://localhost/"),
                         (*rgs.begin())
                             ->getDownloadContext()
                             ->getFirstFileEntry()
                             ->getRemainingUris()
                             .front());
  }
  {
    auto req = createReq(AddUriRpcMethod::getMethodName());
    auto urisParam = List::g();
    urisParam->append("http://localhost/");
    req.params->append(std::move(urisParam));
    // with options
    auto opt = Dict::g();
    opt->put(PREF_DIR->k, "/sink");
    req.params->append(std::move(opt));
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    a2_gid_t gid;
    CPPUNIT_ASSERT_EQUAL(
        0, GroupId::toNumericId(gid, downcast<String>(res.param)->s().c_str()));
    CPPUNIT_ASSERT_EQUAL(std::string("/sink"),
                         findReservedGroup(e_->getRequestGroupMan().get(), gid)
                             ->getOption()
                             ->get(PREF_DIR));
  }
}

void RpcMethodTest::testAddUri_withoutUri()
{
  AddUriRpcMethod m;
  auto res = m.execute(createReq(AddUriRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddUri_notUri()
{
  AddUriRpcMethod m;
  auto req = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append("not uri");
  req.params->append(std::move(urisParam));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddUri_withBadOption()
{
  AddUriRpcMethod m;
  auto req = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append("http://localhost");
  req.params->append(std::move(urisParam));
  auto opt = Dict::g();
  opt->put(PREF_FILE_ALLOCATION->k, "badvalue");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddUri_withPosition()
{
  AddUriRpcMethod m;
  auto req1 = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam1 = List::g();
  urisParam1->append("http://uri1");
  req1.params->append(std::move(urisParam1));
  auto res1 = m.execute(std::move(req1), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res1.code);

  auto req2 = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam2 = List::g();
  urisParam2->append("http://uri2");
  req2.params->append(std::move(urisParam2));
  req2.params->append(Dict::g());
  req2.params->append(Integer::g(0));
  m.execute(std::move(req2), e_.get());

  std::string uri = getReservedGroup(e_->getRequestGroupMan().get(), 0)
                        ->getDownloadContext()
                        ->getFirstFileEntry()
                        ->getRemainingUris()[0];

  CPPUNIT_ASSERT_EQUAL(std::string("http://uri2"), uri);
}

void RpcMethodTest::testAddUri_withBadPosition()
{
  AddUriRpcMethod m;
  auto req = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append("http://localhost/");
  req.params->append(std::move(urisParam));
  req.params->append(Dict::g());
  req.params->append(Integer::g(-1));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

#ifdef ENABLE_BITTORRENT
namespace {
RpcRequest createAddTorrentReq()
{
  auto req = createReq(AddTorrentRpcMethod::getMethodName());
  req.params->append(readFile(A2_TEST_DIR "/single.torrent"));
  auto uris = List::g();
  uris->append("http://localhost/aria2-0.8.2.tar.bz2");
  req.params->append(std::move(uris));
  return req;
}
} // namespace

void RpcMethodTest::testAddTorrent()
{
  File(e_->getOption()->get(PREF_DIR) +
       "/0a3893293e27ac0490424c06de4d09242215f0a6.torrent")
      .remove();
  AddTorrentRpcMethod m;
  {
    // Saving upload metadata is disabled by option.
    auto res = m.execute(createAddTorrentReq(), e_.get());
    CPPUNIT_ASSERT(!File(e_->getOption()->get(PREF_DIR) +
                         "/0a3893293e27ac0490424c06de4d09242215f0a6.torrent")
                        .exists());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    CPPUNIT_ASSERT_EQUAL(sizeof(a2_gid_t) * 2,
                         downcast<String>(res.param)->s().size());
  }
  e_->getOption()->put(PREF_RPC_SAVE_UPLOAD_METADATA, A2_V_TRUE);
  {
    auto res = m.execute(createAddTorrentReq(), e_.get());
    CPPUNIT_ASSERT(File(e_->getOption()->get(PREF_DIR) +
                        "/0a3893293e27ac0490424c06de4d09242215f0a6.torrent")
                       .exists());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    a2_gid_t gid;
    CPPUNIT_ASSERT_EQUAL(
        0, GroupId::toNumericId(gid, downcast<String>(res.param)->s().c_str()));

    auto group = findReservedGroup(e_->getRequestGroupMan().get(), gid);
    CPPUNIT_ASSERT(group);
    CPPUNIT_ASSERT_EQUAL(e_->getOption()->get(PREF_DIR) +
                             "/aria2-0.8.2.tar.bz2",
                         group->getFirstFilePath());
    CPPUNIT_ASSERT_EQUAL((size_t)1, group->getDownloadContext()
                                        ->getFirstFileEntry()
                                        ->getRemainingUris()
                                        .size());
    CPPUNIT_ASSERT_EQUAL(std::string("http://localhost/aria2-0.8.2.tar.bz2"),
                         group->getDownloadContext()
                             ->getFirstFileEntry()
                             ->getRemainingUris()[0]);
  }
  {
    auto req = createAddTorrentReq();
    // with options
    std::string dir = A2_TEST_OUT_DIR "/aria2_RpcMethodTest_testAddTorrent";
    File(dir).mkdirs();
    auto opt = Dict::g();
    opt->put(PREF_DIR->k, dir);
    File(dir + "/0a3893293e27ac0490424c06de4d09242215f0a6.torrent").remove();
    req.params->append(std::move(opt));

    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    a2_gid_t gid;
    CPPUNIT_ASSERT_EQUAL(
        0, GroupId::toNumericId(gid, downcast<String>(res.param)->s().c_str()));
    CPPUNIT_ASSERT_EQUAL(dir + "/aria2-0.8.2.tar.bz2",
                         findReservedGroup(e_->getRequestGroupMan().get(), gid)
                             ->getFirstFilePath());
    CPPUNIT_ASSERT(
        File(dir + "/0a3893293e27ac0490424c06de4d09242215f0a6.torrent")
            .exists());
  }
}

void RpcMethodTest::testAddTorrent_withoutTorrent()
{
  AddTorrentRpcMethod m;
  auto res =
      m.execute(createReq(AddTorrentRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddTorrent_notBase64Torrent()
{
  AddTorrentRpcMethod m;
  auto req = createReq(AddTorrentRpcMethod::getMethodName());
  req.params->append("not torrent");
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddTorrent_withPosition()
{
  AddTorrentRpcMethod m;
  auto req1 = createReq(AddTorrentRpcMethod::getMethodName());
  req1.params->append(readFile(A2_TEST_DIR "/test.torrent"));
  req1.params->append(List::g());
  req1.params->append(Dict::g());
  auto res1 = m.execute(std::move(req1), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res1.code);

  auto req2 = createReq(AddTorrentRpcMethod::getMethodName());
  req2.params->append(readFile(A2_TEST_DIR "/single.torrent"));
  req2.params->append(List::g());
  req2.params->append(Dict::g());
  req2.params->append(Integer::g(0));
  m.execute(std::move(req2), e_.get());

  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       getReservedGroup(e_->getRequestGroupMan().get(), 0)
                           ->getDownloadContext()
                           ->getFileEntries()
                           .size());
}

#endif // ENABLE_BITTORRENT

#ifdef ENABLE_METALINK
namespace {
RpcRequest createAddMetalinkReq()
{
  auto req = createReq(AddMetalinkRpcMethod::getMethodName());
  req.params->append(readFile(A2_TEST_DIR "/2files.metalink"));
  return req;
}
} // namespace

void RpcMethodTest::testAddMetalink()
{
  File(e_->getOption()->get(PREF_DIR) +
       "/c908634fbc257fd56f0114912c2772aeeb4064f4.meta4")
      .remove();
  AddMetalinkRpcMethod m;
  {
    // Saving upload metadata is disabled by option.
    auto res = m.execute(createAddMetalinkReq(), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    const List* resParams = downcast<List>(res.param);
    CPPUNIT_ASSERT_EQUAL((size_t)2, resParams->size());
    a2_gid_t gid1, gid2;
    CPPUNIT_ASSERT_EQUAL(
        0, GroupId::toNumericId(
               gid1, downcast<String>(resParams->get(0))->s().c_str()));
    CPPUNIT_ASSERT_EQUAL(
        0, GroupId::toNumericId(
               gid2, downcast<String>(resParams->get(1))->s().c_str()));
    CPPUNIT_ASSERT(!File(e_->getOption()->get(PREF_DIR) +
                         "/c908634fbc257fd56f0114912c2772aeeb4064f4.meta4")
                        .exists());
  }
  e_->getOption()->put(PREF_RPC_SAVE_UPLOAD_METADATA, A2_V_TRUE);
  {
    auto res = m.execute(createAddMetalinkReq(), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    const List* resParams = downcast<List>(res.param);
    CPPUNIT_ASSERT_EQUAL((size_t)2, resParams->size());
    a2_gid_t gid3, gid4;
    CPPUNIT_ASSERT_EQUAL(
        0, GroupId::toNumericId(
               gid3, downcast<String>(resParams->get(0))->s().c_str()));
    CPPUNIT_ASSERT_EQUAL(
        0, GroupId::toNumericId(
               gid4, downcast<String>(resParams->get(1))->s().c_str()));
    CPPUNIT_ASSERT(File(e_->getOption()->get(PREF_DIR) +
                        "/c908634fbc257fd56f0114912c2772aeeb4064f4.meta4")
                       .exists());

    auto tar = findReservedGroup(e_->getRequestGroupMan().get(), gid3);
    CPPUNIT_ASSERT(tar);
    CPPUNIT_ASSERT_EQUAL(e_->getOption()->get(PREF_DIR) +
                             "/aria2-5.0.0.tar.bz2",
                         tar->getFirstFilePath());
    auto deb = findReservedGroup(e_->getRequestGroupMan().get(), gid4);
    CPPUNIT_ASSERT(deb);
    CPPUNIT_ASSERT_EQUAL(e_->getOption()->get(PREF_DIR) + "/aria2-5.0.0.deb",
                         deb->getFirstFilePath());
  }
  {
    auto req = createAddMetalinkReq();
    // with options
    std::string dir = A2_TEST_OUT_DIR "/aria2_RpcMethodTest_testAddMetalink";
    File(dir).mkdirs();
    auto opt = Dict::g();
    opt->put(PREF_DIR->k, dir);
    File(dir + "/c908634fbc257fd56f0114912c2772aeeb4064f4.meta4").remove();
    req.params->append(std::move(opt));

    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    const List* resParams = downcast<List>(res.param);
    CPPUNIT_ASSERT_EQUAL((size_t)2, resParams->size());
    a2_gid_t gid5;
    CPPUNIT_ASSERT_EQUAL(
        0, GroupId::toNumericId(
               gid5, downcast<String>(resParams->get(0))->s().c_str()));
    CPPUNIT_ASSERT_EQUAL(dir + "/aria2-5.0.0.tar.bz2",
                         findReservedGroup(e_->getRequestGroupMan().get(), gid5)
                             ->getFirstFilePath());
    CPPUNIT_ASSERT(
        File(dir + "/c908634fbc257fd56f0114912c2772aeeb4064f4.meta4").exists());
  }
}

void RpcMethodTest::testAddMetalink_withoutMetalink()
{
  AddMetalinkRpcMethod m;
  auto res =
      m.execute(createReq(AddMetalinkRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddMetalink_notBase64Metalink()
{
  AddMetalinkRpcMethod m;
  auto req = createReq(AddMetalinkRpcMethod::getMethodName());
  req.params->append("not metalink");
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddMetalink_withPosition()
{
  AddUriRpcMethod m1;
  auto req1 = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam1 = List::g();
  urisParam1->append("http://uri");
  req1.params->append(std::move(urisParam1));
  auto res1 = m1.execute(std::move(req1), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res1.code);

  AddMetalinkRpcMethod m2;
  auto req2 = createReq(AddMetalinkRpcMethod::getMethodName());
  req2.params->append(readFile(A2_TEST_DIR "/2files.metalink"));
  req2.params->append(Dict::g());
  req2.params->append(Integer::g(0));
  auto res2 = m2.execute(std::move(req2), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res2.code);

  CPPUNIT_ASSERT_EQUAL(
      e_->getOption()->get(PREF_DIR) + "/aria2-5.0.0.tar.bz2",
      getReservedGroup(e_->getRequestGroupMan().get(), 0)->getFirstFilePath());
}

#endif // ENABLE_METALINK

void RpcMethodTest::testGetOption()
{
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->getOption()->put(PREF_DIR, "alpha");
  e_->getRequestGroupMan()->addReservedGroup(group);
  auto dr = createDownloadResult(error_code::FINISHED, "http://host/fin");
  dr->option->put(PREF_DIR, "bravo");
  e_->getRequestGroupMan()->addDownloadResult(dr);

  GetOptionRpcMethod m;
  auto req = createReq(GetOptionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const Dict* resopt = downcast<Dict>(res.param);
  CPPUNIT_ASSERT_EQUAL(std::string("alpha"),
                       downcast<String>(resopt->get(PREF_DIR->k))->s());

  req = createReq(GetOptionRpcMethod::getMethodName());
  req.params->append(dr->gid->toHex());
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resopt = downcast<Dict>(res.param);
  CPPUNIT_ASSERT_EQUAL(std::string("bravo"),
                       downcast<String>(resopt->get(PREF_DIR->k))->s());
  // Invalid GID
  req = createReq(GetOptionRpcMethod::getMethodName());
  req.params->append(GroupId::create()->toHex());
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testChangeOption()
{
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  e_->getRequestGroupMan()->addReservedGroup(group);

  ChangeOptionRpcMethod m;
  auto req = createReq(ChangeOptionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto opt = Dict::g();
  opt->put(PREF_MAX_DOWNLOAD_LIMIT->k, "100K");
#ifdef ENABLE_BITTORRENT
  opt->put(PREF_BT_MAX_PEERS->k, "100");
  opt->put(PREF_BT_REQUEST_PEER_SPEED_LIMIT->k, "300K");
  opt->put(PREF_MAX_UPLOAD_LIMIT->k, "50K");

  {
    auto btObject = make_unique<BtObject>();
    btObject->btRuntime = std::make_shared<BtRuntime>();
    e_->getBtRegistry()->put(group->getGID(), std::move(btObject));
  }
#endif // ENABLE_BITTORRENT
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());

  auto option = group->getOption();

  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL((int)100_k, group->getMaxDownloadSpeedLimit());
  CPPUNIT_ASSERT_EQUAL(std::string("102400"),
                       option->get(PREF_MAX_DOWNLOAD_LIMIT));
#ifdef ENABLE_BITTORRENT
  CPPUNIT_ASSERT_EQUAL(std::string("307200"),
                       option->get(PREF_BT_REQUEST_PEER_SPEED_LIMIT));

  CPPUNIT_ASSERT_EQUAL(std::string("100"), option->get(PREF_BT_MAX_PEERS));
  CPPUNIT_ASSERT_EQUAL(
      100, e_->getBtRegistry()->get(group->getGID())->btRuntime->getMaxPeers());

  CPPUNIT_ASSERT_EQUAL((int)50_k, group->getMaxUploadSpeedLimit());
  CPPUNIT_ASSERT_EQUAL(std::string("51200"),
                       option->get(PREF_MAX_UPLOAD_LIMIT));
#endif // ENABLE_BITTORRENT
}

void RpcMethodTest::testChangeOption_withBadOption()
{
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  e_->getRequestGroupMan()->addReservedGroup(group);

  ChangeOptionRpcMethod m;
  auto req = createReq(ChangeOptionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto opt = Dict::g();
  opt->put(PREF_MAX_DOWNLOAD_LIMIT->k, "badvalue");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testChangeOption_withNotAllowedOption()
{
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  e_->getRequestGroupMan()->addReservedGroup(group);

  ChangeOptionRpcMethod m;
  auto req = createReq(ChangeOptionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto opt = Dict::g();
  opt->put(PREF_MAX_OVERALL_DOWNLOAD_LIMIT->k, "100K");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  // The unacceptable options are just ignored.
  CPPUNIT_ASSERT_EQUAL(0, res.code);
}

void RpcMethodTest::testChangeOption_withoutGid()
{
  ChangeOptionRpcMethod m;
  auto res =
      m.execute(createReq(ChangeOptionRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

namespace {
std::vector<std::shared_ptr<RequestGroup>>
startSaturatedSite(const std::shared_ptr<DownloadEngine>& e,
                   const std::shared_ptr<Option>& option)
{
  auto& rgman = e->getRequestGroupMan();
  rgman->setMaxConcurrentDownloads(3);
  OptionParser::getInstance()->find(PREF_SITE_CONCURRENT)->parse(*option,
                                                                 "a.test:1");
  std::vector<std::shared_ptr<RequestGroup>> groups;
  for (int i = 0; i < 2; ++i) {
    auto opt = util::copy(option);
    opt->put(PREF_REFERER, "https://a.test/");
    auto name = "site" + util::itos(i);
    groups.push_back(createRequestGroup(0, 0, name, "http://host/" + name, opt));
  }
  rgman->addReservedGroup(groups);
  rgman->fillRequestGroupFromReserver(e.get());
  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman->getRequestGroups().size());
  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman->getReservedGroups().size());
  CPPUNIT_ASSERT_EQUAL(groups[1]->getGID(),
                       (*rgman->getReservedGroups().begin())->getGID());
  rgman->clearQueueCheck();
  return groups;
}

void runFillRequestGroupCommand(const std::shared_ptr<DownloadEngine>& e)
{
  // Unless execute() returns true, it hands the command over to e.
  auto command = new FillRequestGroupCommand(e->newCUID(), e.get());
  if (command->execute()) {
    delete command;
  }
}
} // namespace

void RpcMethodTest::testChangeOption_siteOfWaiting()
{
  auto groups = startSaturatedSite(e_, option_);
  auto& rgman = e_->getRequestGroupMan();

  ChangeOptionRpcMethod m;
  auto req = createReq(ChangeOptionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(groups[1]->getGID()));
  auto opt = Dict::g();
  opt->put(PREF_REFERER->k, "https://c.test/");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT(rgman->queueCheckRequested());
  CPPUNIT_ASSERT_EQUAL(
      std::string("c.test"),
      SiteConcurrency(*e_).classify(*groups[1]->getOption()).site);

  runFillRequestGroupCommand(e_);
  CPPUNIT_ASSERT_EQUAL((size_t)2, rgman->getRequestGroups().size());
  CPPUNIT_ASSERT(rgman->getReservedGroups().empty());
}

void RpcMethodTest::testChangeGlobalOption_raiseSiteLimit()
{
  auto groups = startSaturatedSite(e_, option_);
  auto& rgman = e_->getRequestGroupMan();

  ChangeGlobalOptionRpcMethod m;
  auto req = createReq(ChangeGlobalOptionRpcMethod::getMethodName());
  auto opt = Dict::g();
  opt->put(PREF_SITE_CONCURRENT->k, "a.test:2");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT(rgman->queueCheckRequested());

  // The waiting download starts; the running one is not completed,
  // cancelled, or restarted.
  runFillRequestGroupCommand(e_);
  CPPUNIT_ASSERT_EQUAL((size_t)2, rgman->getRequestGroups().size());
  CPPUNIT_ASSERT(rgman->getReservedGroups().empty());
  for (const auto& g : groups) {
    CPPUNIT_ASSERT_EQUAL((int)RequestGroup::STATE_ACTIVE, g->getState());
    CPPUNIT_ASSERT(!g->isHaltRequested());
    CPPUNIT_ASSERT(!g->isPauseRequested());
    CPPUNIT_ASSERT(!g->isRestartRequested());
  }
  CPPUNIT_ASSERT_EQUAL(groups[0]->getGID(),
                       (*rgman->getRequestGroups().begin())->getGID());
  CPPUNIT_ASSERT(rgman->getDownloadResults().empty());
}

void RpcMethodTest::testChangeGlobalOption()
{
  ChangeGlobalOptionRpcMethod m;
  auto req = createReq(ChangeGlobalOptionRpcMethod::getMethodName());
  auto opt = Dict::g();
  opt->put(PREF_MAX_OVERALL_DOWNLOAD_LIMIT->k, "100K");
#ifdef ENABLE_BITTORRENT
  opt->put(PREF_MAX_OVERALL_UPLOAD_LIMIT->k, "50K");
#endif // ENABLE_BITTORRENT
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());

  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(
      (int)100_k, e_->getRequestGroupMan()->getMaxOverallDownloadSpeedLimit());
  CPPUNIT_ASSERT_EQUAL(std::string("102400"),
                       e_->getOption()->get(PREF_MAX_OVERALL_DOWNLOAD_LIMIT));
#ifdef ENABLE_BITTORRENT
  CPPUNIT_ASSERT_EQUAL(
      (int)50_k, e_->getRequestGroupMan()->getMaxOverallUploadSpeedLimit());
  CPPUNIT_ASSERT_EQUAL(std::string("51200"),
                       e_->getOption()->get(PREF_MAX_OVERALL_UPLOAD_LIMIT));
#endif // ENABLE_BITTORRENT
}

void RpcMethodTest::testChangeGlobalOption_withBadOption()
{
  ChangeGlobalOptionRpcMethod m;
  auto req = createReq(ChangeGlobalOptionRpcMethod::getMethodName());
  auto opt = Dict::g();
  opt->put(PREF_MAX_OVERALL_DOWNLOAD_LIMIT->k, "badvalue");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testChangeGlobalOption_withNotAllowedOption()
{
  ChangeGlobalOptionRpcMethod m;
  auto req = createReq(ChangeGlobalOptionRpcMethod::getMethodName());
  auto opt = Dict::g();
  opt->put(PREF_ENABLE_RPC->k, "100K");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  // The unacceptable options are just ignored.
  CPPUNIT_ASSERT_EQUAL(0, res.code);
}

void RpcMethodTest::testNoSuchMethod()
{
  NoSuchMethodRpcMethod m;
  auto res = m.execute(createReq("make.hamburger"), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
  CPPUNIT_ASSERT_EQUAL(std::string("No such method: make.hamburger"),
                       getString(downcast<Dict>(res.param), "faultString"));
}

void RpcMethodTest::testTellStatus_withoutGid()
{
  TellStatusRpcMethod m;
  auto res =
      m.execute(createReq(TellStatusRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

namespace {
void addUri(const std::string& uri, const std::shared_ptr<DownloadEngine>& e)
{
  AddUriRpcMethod m;
  auto req = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append(uri);
  req.params->append(std::move(urisParam));
  CPPUNIT_ASSERT_EQUAL(0, m.execute(std::move(req), e.get()).code);
}
} // namespace

#ifdef ENABLE_BITTORRENT
namespace {
void addTorrent(const std::string& torrentFile,
                const std::shared_ptr<DownloadEngine>& e)
{
  AddTorrentRpcMethod m;
  auto req = createReq(AddTorrentRpcMethod::getMethodName());
  req.params->append(readFile(torrentFile));
  auto res = m.execute(std::move(req), e.get());
}
} // namespace
#endif // ENABLE_BITTORRENT

void RpcMethodTest::testTellWaiting()
{
  addUri("http://1/", e_);
  addUri("http://2/", e_);
  addUri("http://3/", e_);
#ifdef ENABLE_BITTORRENT
  addTorrent(A2_TEST_DIR "/single.torrent", e_);
#else  // !ENABLE_BITTORRENT
  addUri("http://4/", e_);
#endif // !ENABLE_BITTORRENT
  auto& rgman = e_->getRequestGroupMan();
  TellWaitingRpcMethod m;
  auto req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(1));
  req.params->append(Integer::g(2));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const List* resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)2, resParams->size());
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 1)->getGID()),
      getString(downcast<Dict>(resParams->get(0)), "gid"));
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 2)->getGID()),
      getString(downcast<Dict>(resParams->get(1)), "gid"));
  // waiting.size() == offset+num
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(1));
  req.params->append(Integer::g(3));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)3, resParams->size());
  // waiting.size() < offset+num
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(1));
  req.params->append(Integer::g(4));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)3, resParams->size());

  // offset = INT32_MAX
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(INT32_MAX));
  req.params->append(Integer::g(1));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)0, resParams->size());
  // num = INT32_MAX
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(1));
  req.params->append(Integer::g(INT32_MAX));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)3, resParams->size());
  // offset=INT32_MAX and num = INT32_MAX
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(INT32_MAX));
  req.params->append(Integer::g(INT32_MAX));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)0, resParams->size());
  // offset=INT32_MIN and num = INT32_MAX
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(INT32_MIN));
  req.params->append(Integer::g(INT32_MAX));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)0, resParams->size());

  // negative offset
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(-1));
  req.params->append(Integer::g(2));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)2, resParams->size());
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 3)->getGID()),
      getString(downcast<Dict>(resParams->get(0)), "gid"));
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 2)->getGID()),
      getString(downcast<Dict>(resParams->get(1)), "gid"));
  // negative offset and size < num
  req = RpcRequest(TellWaitingRpcMethod::getMethodName(), List::g());
  req.params->append(Integer::g(-1));
  req.params->append(Integer::g(100));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)4, resParams->size());
  // nagative offset and normalized offset < 0
  req = RpcRequest(TellWaitingRpcMethod::getMethodName(), List::g());
  req.params->append(Integer::g(-5));
  req.params->append(Integer::g(100));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)0, resParams->size());
  // nagative offset and normalized offset == 0
  req = RpcRequest(TellWaitingRpcMethod::getMethodName(), List::g());
  req.params->append(Integer::g(-4));
  req.params->append(Integer::g(100));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)1, resParams->size());
}

void RpcMethodTest::testTellWaiting_fail()
{
  TellWaitingRpcMethod m;
  auto res =
      m.execute(createReq(TellWaitingRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testTellWaiting_int64Num()
{
  addUri("http://1/", e_);
  addUri("http://2/", e_);
  addUri("http://3/", e_);
  auto& rgman = e_->getRequestGroupMan();
  TellWaitingRpcMethod m;
  auto req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(0));
  req.params->append(Integer::g(INT64_MAX));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const List* resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)3, resParams->size());
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(2));
  req.params->append(Integer::g(INT64_MAX));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)1, resParams->size());
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 2)->getGID()),
      getString(downcast<Dict>(resParams->get(0)), "gid"));
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(-1));
  req.params->append(Integer::g(INT64_MAX));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)3, resParams->size());
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 2)->getGID()),
      getString(downcast<Dict>(resParams->get(0)), "gid"));
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 1)->getGID()),
      getString(downcast<Dict>(resParams->get(1)), "gid"));
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 0)->getGID()),
      getString(downcast<Dict>(resParams->get(2)), "gid"));
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(3));
  req.params->append(Integer::g(INT64_MAX));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)0, resParams->size());
}

void RpcMethodTest::testGetVersion()
{
  GetVersionRpcMethod m;
  auto res =
      m.execute(createReq(GetVersionRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const Dict* resParams = downcast<Dict>(res.param);
  CPPUNIT_ASSERT_EQUAL(std::string(PACKAGE_VERSION),
                       getString(resParams, "version"));
  const List* featureList = downcast<List>(resParams->get("enabledFeatures"));
  std::string features;
  for (auto i = featureList->begin(); i != featureList->end(); ++i) {
    const String* s = downcast<String>(*i);
    features += s->s();
    features += ", ";
  }
  CPPUNIT_ASSERT_EQUAL(featureSummary() + ", ", features);
}

void RpcMethodTest::testGatherStoppedDownload()
{
  std::vector<std::shared_ptr<FileEntry>> fileEntries;
  std::vector<a2_gid_t> followedBy;
  followedBy.push_back(3);
  followedBy.push_back(4);
  auto d = std::make_shared<DownloadResult>();
  d->gid = GroupId::create();
  d->fileEntries = fileEntries;
  d->inMemoryDownload = false;
  d->sessionDownloadLength = UINT64_MAX;
  d->sessionTime = 1_s;
  d->result = error_code::FINISHED;
  d->followedBy = followedBy;
  d->following = 1;
  d->belongsTo = 2;
  auto entry = Dict::g();
  std::vector<std::string> keys;
  gatherStoppedDownload(entry.get(), d, SiteConcurrency(*e_), keys);

  const List* followedByRes = downcast<List>(entry->get("followedBy"));
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(3),
                       downcast<String>(followedByRes->get(0))->s());
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(4),
                       downcast<String>(followedByRes->get(1))->s());
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(1),
                       downcast<String>(entry->get("following"))->s());
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(2),
                       downcast<String>(entry->get("belongsTo"))->s());

  keys.push_back("gid");

  entry = Dict::g();
  gatherStoppedDownload(entry.get(), d, SiteConcurrency(*e_), keys);
  CPPUNIT_ASSERT_EQUAL((size_t)1, entry->size());
  CPPUNIT_ASSERT(entry->containsKey("gid"));
}

#ifdef ENABLE_BITTORRENT
void RpcMethodTest::testGatherStoppedDownload_bt()
{
  auto d = std::make_shared<DownloadResult>();
  d->gid = GroupId::create();
  d->infoHash = "2089b05ecca3d829cee5497d2703803b52216d19";
  d->attrs = std::vector<std::shared_ptr<ContextAttribute>>(MAX_CTX_ATTR);

  auto torrentAttr = std::make_shared<TorrentAttribute>();
  torrentAttr->creationDate = 1000000007;
  d->attrs[CTX_ATTR_BT] = torrentAttr;

  auto entry = Dict::g();
  gatherStoppedDownload(entry.get(), d, SiteConcurrency(*e_), {});

  auto btDict = downcast<Dict>(entry->get("bittorrent"));
  CPPUNIT_ASSERT(btDict);

  CPPUNIT_ASSERT_EQUAL((int64_t)1000000007,
                       downcast<Integer>(btDict->get("creationDate"))->i());
}
#endif // ENABLE_BITTORRENT

void RpcMethodTest::testGatherProgressCommon()
{
  auto dctx = std::make_shared<DownloadContext>(0, 0, "aria2.tar.bz2");
  std::string uris[] = {"http://localhost/aria2.tar.bz2"};
  dctx->getFirstFileEntry()->addUris(std::begin(uris), std::end(uris));
  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);
  std::vector<std::shared_ptr<RequestGroup>> followedBy;
  for (int i = 0; i < 2; ++i) {
    followedBy.push_back(
        std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)));
  }

  group->followedBy(followedBy.begin(), followedBy.end());
  auto leader = GroupId::create();
  group->following(leader->getNumericId());
  auto parent = GroupId::create();
  group->belongsTo(parent->getNumericId());

  auto entry = Dict::g();
  std::vector<std::string> keys;
  gatherProgressCommon(entry.get(), group, SiteConcurrency(*e_), keys);

  const List* followedByRes = downcast<List>(entry->get("followedBy"));
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(followedBy[0]->getGID()),
                       downcast<String>(followedByRes->get(0))->s());
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(followedBy[1]->getGID()),
                       downcast<String>(followedByRes->get(1))->s());
  CPPUNIT_ASSERT_EQUAL(leader->toHex(),
                       downcast<String>(entry->get("following"))->s());
  CPPUNIT_ASSERT_EQUAL(parent->toHex(),
                       downcast<String>(entry->get("belongsTo"))->s());
  const List* files = downcast<List>(entry->get("files"));
  CPPUNIT_ASSERT_EQUAL((size_t)1, files->size());
  const Dict* file = downcast<Dict>(files->get(0));
  CPPUNIT_ASSERT_EQUAL(std::string("aria2.tar.bz2"),
                       downcast<String>(file->get("path"))->s());
  CPPUNIT_ASSERT_EQUAL(
      uris[0],
      downcast<String>(
          downcast<Dict>(downcast<List>(file->get("uris"))->get(0))->get("uri"))
          ->s());
  CPPUNIT_ASSERT_EQUAL(e_->getOption()->get(PREF_DIR),
                       downcast<String>(entry->get("dir"))->s());

  keys.push_back("gid");
  entry = Dict::g();
  gatherProgressCommon(entry.get(), group, SiteConcurrency(*e_), keys);

  CPPUNIT_ASSERT_EQUAL((size_t)1, entry->size());
  CPPUNIT_ASSERT(entry->containsKey("gid"));
}

namespace {
void setSiteLimits(Option& option, const std::string& sites,
                   const std::string& defaultLimit)
{
  const auto& parser = OptionParser::getInstance();
  parser->find(PREF_SITE_CONCURRENT)->parse(option, sites);
  parser->find(PREF_SITE_CONCURRENT_DEFAULT)->parse(option, defaultLimit);
}

std::string getStringValue(const Dict* dict, const std::string& key)
{
  auto s = downcast<String>(dict->get(key));
  CPPUNIT_ASSERT_MESSAGE(key, s);
  return s->s();
}
} // namespace

void RpcMethodTest::testGatherProgressCommon_siteAndName()
{
  setSiteLimits(*option_, "a.test:1", "0");
  // Started: its path is known. The Referer header line beats the
  // referer option.
  auto opt = util::copy(option_);
  opt->put(PREF_HEADER, "Referer: https://www.a.test/page");
  opt->put(PREF_REFERER, "https://b.test/");
  auto started = createRequestGroup(0, 0, "/dl/sub/aria2.tar.bz2",
                                    "http://host/other.bin", opt);
  // Never started and no out: its path is empty.
  auto queued = createRequestGroup(
      0, 0, "", "http://host/dir/my%20file.iso?token=x#frag",
      util::copy(option_));
  auto queuedDir =
      createRequestGroup(0, 0, "", "http://host/dir/", util::copy(option_));

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), started, SiteConcurrency(*e_), {});
  CPPUNIT_ASSERT_EQUAL(std::string("aria2.tar.bz2"),
                       getStringValue(entry.get(), "name"));
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"),
                       getStringValue(entry.get(), "site"));
  CPPUNIT_ASSERT_EQUAL(SiteConcurrency(*e_).classify(*opt).site,
                       getStringValue(entry.get(), "site"));

  entry = Dict::g();
  gatherProgressCommon(entry.get(), queued, SiteConcurrency(*e_), {});
  CPPUNIT_ASSERT_EQUAL(std::string("my file.iso"),
                       getStringValue(entry.get(), "name"));
  CPPUNIT_ASSERT_EQUAL(std::string("(none)"),
                       getStringValue(entry.get(), "site"));

  // An empty last segment is named as aria2 names the file.
  entry = Dict::g();
  gatherProgressCommon(entry.get(), queuedDir, SiteConcurrency(*e_), {"name"});
  CPPUNIT_ASSERT_EQUAL(std::string("index.html"),
                       getStringValue(entry.get(), "name"));

  // The engine's site list is used, not the download's copy of it.
  setSiteLimits(*option_, "www.a.test:2", "0");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1"),
                       opt->get(PREF_SITE_CONCURRENT));
  entry = Dict::g();
  gatherProgressCommon(entry.get(), started, SiteConcurrency(*e_), {"site"});
  CPPUNIT_ASSERT_EQUAL(std::string("www.a.test"),
                       getStringValue(entry.get(), "site"));

  // The keys filter.
  for (const auto& key : {"site", "name"}) {
    entry = Dict::g();
    gatherProgressCommon(entry.get(), queued, SiteConcurrency(*e_), {key});
    CPPUNIT_ASSERT_EQUAL((size_t)1, entry->size());
    CPPUNIT_ASSERT(entry->containsKey(key));
  }
  entry = Dict::g();
  gatherProgressCommon(entry.get(), queued, SiteConcurrency(*e_),
                       {"gid", "dir"});
  CPPUNIT_ASSERT_EQUAL((size_t)2, entry->size());
  CPPUNIT_ASSERT(!entry->containsKey("site"));
  CPPUNIT_ASSERT(!entry->containsKey("name"));
}

void RpcMethodTest::testGatherStoppedDownload_siteAndName()
{
  // Path /tmp/path is known.
  auto dr = createDownloadResult(error_code::FINISHED, "http://host/fin");
  dr->option->put(PREF_REFERER, "https://dl.a.test/");
  // A stale site list in the download's own option is not used.
  dr->option->put(PREF_SITE_CONCURRENT, "dl.a.test:1");
  // Stopped before its path was determined.
  auto never = createDownloadResult(error_code::REMOVED,
                                    "http://host/get/report.pdf?id=7#p2");
  never->fileEntries[0]->setPath("");

  auto entry = Dict::g();
  gatherStoppedDownload(entry.get(), dr, SiteConcurrency(*e_), {});
  CPPUNIT_ASSERT_EQUAL(std::string("path"),
                       getStringValue(entry.get(), "name"));
  CPPUNIT_ASSERT_EQUAL(std::string("a.test"),
                       getStringValue(entry.get(), "site"));

  setSiteLimits(*option_, "a.test:2", "1");
  entry = Dict::g();
  gatherStoppedDownload(entry.get(), dr, SiteConcurrency(*e_), {"site"});
  CPPUNIT_ASSERT_EQUAL(SiteConcurrency(*e_).classify(*dr->option).site,
                       getStringValue(entry.get(), "site"));

  entry = Dict::g();
  gatherStoppedDownload(entry.get(), never, SiteConcurrency(*e_), {});
  CPPUNIT_ASSERT_EQUAL(std::string("report.pdf"),
                       getStringValue(entry.get(), "name"));
  CPPUNIT_ASSERT_EQUAL(std::string("(none)"),
                       getStringValue(entry.get(), "site"));

  // The keys filter.
  for (const auto& key : {"site", "name"}) {
    entry = Dict::g();
    gatherStoppedDownload(entry.get(), dr, SiteConcurrency(*e_), {key});
    CPPUNIT_ASSERT_EQUAL((size_t)1, entry->size());
    CPPUNIT_ASSERT(entry->containsKey(key));
  }
  entry = Dict::g();
  gatherStoppedDownload(entry.get(), dr, SiteConcurrency(*e_),
                        {"gid", "status"});
  CPPUNIT_ASSERT_EQUAL((size_t)2, entry->size());
  CPPUNIT_ASSERT(!entry->containsKey("site"));
  CPPUNIT_ASSERT(!entry->containsKey("name"));
}

namespace {
std::unique_ptr<List> createKeys(const std::vector<std::string>& keys)
{
  auto list = List::g();
  for (const auto& k : keys) {
    list->append(k);
  }
  return list;
}
} // namespace

void RpcMethodTest::testTellStatus_siteAndName()
{
  setSiteLimits(*option_, "b.test:1", "0");
  AddUriRpcMethod addUriMethod;
  auto addReq = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append("http://host/files/big.iso?sig=1");
  addReq.params->append(std::move(urisParam));
  auto opt = Dict::g();
  opt->put(PREF_REFERER->k, "https://x.b.test/");
  addReq.params->append(std::move(opt));
  auto addRes = addUriMethod.execute(std::move(addReq), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, addRes.code);
  auto gid = downcast<String>(addRes.param)->s();

  TellStatusRpcMethod m;
  auto req = createReq(TellStatusRpcMethod::getMethodName());
  req.params->append(gid);
  req.params->append(createKeys({"site", "name"}));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  auto entry = downcast<Dict>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)2, entry->size());
  CPPUNIT_ASSERT_EQUAL(std::string("b.test"), getStringValue(entry, "site"));
  CPPUNIT_ASSERT_EQUAL(std::string("big.iso"), getStringValue(entry, "name"));

  auto dr = createDownloadResult(error_code::FINISHED, "http://host/fin");
  dr->option->put(PREF_REFERER, "https://b.test/");
  e_->getRequestGroupMan()->addDownloadResult(dr);
  req = createReq(TellStatusRpcMethod::getMethodName());
  req.params->append(dr->gid->toHex());
  req.params->append(createKeys({"site"}));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  entry = downcast<Dict>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)1, entry->size());
  CPPUNIT_ASSERT_EQUAL(std::string("b.test"), getStringValue(entry, "site"));

  TellStoppedRpcMethod stoppedMethod;
  req = createReq(TellStoppedRpcMethod::getMethodName());
  req.params->append(Integer::g(0));
  req.params->append(Integer::g(10));
  req.params->append(createKeys({"name"}));
  res = stoppedMethod.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  auto list = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)1, list->size());
  entry = downcast<Dict>(list->get(0));
  CPPUNIT_ASSERT_EQUAL((size_t)1, entry->size());
  CPPUNIT_ASSERT_EQUAL(std::string("path"), getStringValue(entry, "name"));
}

void RpcMethodTest::testAddUri_withSiteHint()
{
  const std::string hint = "https://Members.Example.co.uk/x";
  AddUriRpcMethod addUriMethod;
  auto addReq = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append("http://192.0.2.1/f.bin");
  addReq.params->append(std::move(urisParam));
  auto opt = Dict::g();
  opt->put(PREF_SITE_HINT->k, hint);
  addReq.params->append(std::move(opt));
  auto addRes = addUriMethod.execute(std::move(addReq), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, addRes.code);
  auto gid = downcast<String>(addRes.param)->s();
  a2_gid_t n;
  CPPUNIT_ASSERT_EQUAL(0, GroupId::toNumericId(n, gid.c_str()));
  auto group = findReservedGroup(e_->getRequestGroupMan().get(), n);
  CPPUNIT_ASSERT(group);
  CPPUNIT_ASSERT_EQUAL(hint, group->getOption()->get(PREF_SITE_HINT));
  CPPUNIT_ASSERT(group->getOption()->blank(PREF_HEADER));
  CPPUNIT_ASSERT(group->getOption()->blank(PREF_REFERER));

  TellStatusRpcMethod m;
  auto req = createReq(TellStatusRpcMethod::getMethodName());
  req.params->append(gid);
  req.params->append(createKeys({"site"}));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(std::string("example.co.uk"),
                       getStringValue(downcast<Dict>(res.param), "site"));
}

void RpcMethodTest::testAddUri_withBadSiteHint()
{
  AddUriRpcMethod m;
  // No host; a newline that would add a session option line; a
  // trailing space that the session reader would strip; longer than
  // 65535 bytes.
  auto tooLong = std::string("https://example.com/") + std::string(65535, 'a');
  CPPUNIT_ASSERT_EQUAL((size_t)65555, tooLong.size());
  for (const auto& hint :
       {std::string("https:///path"),
        std::string("https://example.com/a\n referer=https://example.com/"),
        std::string("https://example.com/ "), tooLong}) {
    auto req = createReq(AddUriRpcMethod::getMethodName());
    auto urisParam = List::g();
    urisParam->append("http://localhost/");
    req.params->append(std::move(urisParam));
    auto opt = Dict::g();
    opt->put(PREF_SITE_HINT->k, hint);
    req.params->append(std::move(opt));
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(1, res.code);
  }
  CPPUNIT_ASSERT_EQUAL((size_t)0,
                       e_->getRequestGroupMan()->getReservedGroups().size());
}

void RpcMethodTest::testChangeOption_siteHint()
{
  auto option = util::copy(option_);
  option->put(PREF_SITE_HINT, "https://a.test/");
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option);
  e_->getRequestGroupMan()->addReservedGroup(group);

  // changeOption and changeGlobalOption ignore it without an error.
  ChangeOptionRpcMethod m;
  auto req = createReq(ChangeOptionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto opt = Dict::g();
  opt->put(PREF_SITE_HINT->k, "https://b.test/");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(std::string("https://a.test/"),
                       group->getOption()->get(PREF_SITE_HINT));

  ChangeGlobalOptionRpcMethod gm;
  req = createReq(ChangeGlobalOptionRpcMethod::getMethodName());
  opt = Dict::g();
  opt->put(PREF_SITE_HINT->k, "https://b.test/");
  req.params->append(std::move(opt));
  res = gm.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT(!e_->getOption()->defined(PREF_SITE_HINT));
}

namespace {
// Formats the ariam.getSiteSummary result as
// "DEFAULT CEILING | SITE LISTED LIMIT A/W/P; ...". Every value must be
// a string.
std::string getSiteSummary(const std::shared_ptr<DownloadEngine>& e)
{
  auto res = getMethod(GetSiteSummaryRpcMethod::getMethodName())
                 ->execute(createReq(GetSiteSummaryRpcMethod::getMethodName()),
                           e.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  auto summary = downcast<Dict>(res.param);
  CPPUNIT_ASSERT(summary);
  CPPUNIT_ASSERT_EQUAL((size_t)3, summary->size());
  auto out = getStringValue(summary, "default") + " " +
             getStringValue(summary, "ceiling") + " |";
  auto sites = downcast<List>(summary->get("sites"));
  CPPUNIT_ASSERT(sites);
  bool first = true;
  for (auto& v : *sites) {
    auto s = downcast<Dict>(v);
    CPPUNIT_ASSERT(s);
    CPPUNIT_ASSERT_EQUAL((size_t)6, s->size());
    out += first ? " " : "; ";
    first = false;
    out += getStringValue(s, "site") + " " + getStringValue(s, "listed") +
           " " + getStringValue(s, "limit") + " " +
           getStringValue(s, "active") + "/" + getStringValue(s, "waiting") +
           "/" + getStringValue(s, "paused");
  }
  return out;
}

// Formats per-site "A/W/P" counts as "SITE A/W/P; ..." in site order.
std::string formatTally(const std::map<std::string, std::string>& tally)
{
  std::string out;
  for (const auto& t : tally) {
    if (!out.empty()) {
      out += "; ";
    }
    out += t.first + " " + t.second;
  }
  return out;
}

// Per-site counts of the summary's sites that have downloads.
std::string getSummaryTally(const std::shared_ptr<DownloadEngine>& e)
{
  auto res = getMethod(GetSiteSummaryRpcMethod::getMethodName())
                 ->execute(createReq(GetSiteSummaryRpcMethod::getMethodName()),
                           e.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  std::map<std::string, std::string> tally;
  for (auto& v : *downcast<List>(downcast<Dict>(res.param)->get("sites"))) {
    auto s = downcast<Dict>(v);
    auto counts = getStringValue(s, "active") + "/" +
                  getStringValue(s, "waiting") + "/" +
                  getStringValue(s, "paused");
    if (counts != "0/0/0") {
      tally[getStringValue(s, "site")] = counts;
    }
  }
  return formatTally(tally);
}

struct SiteTally {
  int active = 0;
  int waiting = 0;
  int paused = 0;
};

// Per-site counts from the site and status keys of tellActive and
// tellWaiting.
std::string getTellTally(const std::shared_ptr<DownloadEngine>& e)
{
  std::map<std::string, SiteTally> tally;
  {
    TellActiveRpcMethod m;
    auto req = createReq(TellActiveRpcMethod::getMethodName());
    req.params->append(createKeys({"site"}));
    auto res = m.execute(std::move(req), e.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    for (auto& v : *downcast<List>(res.param)) {
      ++tally[getStringValue(downcast<Dict>(v), "site")].active;
    }
  }
  {
    TellWaitingRpcMethod m;
    auto req = createReq(TellWaitingRpcMethod::getMethodName());
    req.params->append(Integer::g(0));
    req.params->append(Integer::g(1000));
    req.params->append(createKeys({"site", "status"}));
    auto res = m.execute(std::move(req), e.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    for (auto& v : *downcast<List>(res.param)) {
      auto d = downcast<Dict>(v);
      auto& t = tally[getStringValue(d, "site")];
      if (getStringValue(d, "status") == "paused") {
        ++t.paused;
      }
      else {
        ++t.waiting;
      }
    }
  }
  std::map<std::string, std::string> res;
  for (const auto& t : tally) {
    res[t.first] = util::itos(t.second.active) + "/" +
                   util::itos(t.second.waiting) + "/" +
                   util::itos(t.second.paused);
  }
  return formatTally(res);
}
} // namespace

void RpcMethodTest::testGetSiteSummary()
{
  // Listed sites are not in byte order, and idle.test has no download.
  setSiteLimits(*option_, "b.test:3,a.test:1,idle.test:4", "2");
  auto& rgman = e_->getRequestGroupMan();
  rgman->setMaxConcurrentDownloads(6);
  std::map<std::string, std::shared_ptr<RequestGroup>> groups;
  for (const auto& p : std::vector<std::pair<std::string, std::string>>{
           {"a1", "a.test"},
           {"a2", "members.a.test"},
           {"a3", "a.test"},
           {"b1", "b.test"},
           {"b2", "cdn.b.test"},
           {"b3", "b.test"},
           {"b4", "b.test"},
           {"c1", "dl.c.test"},
           {"c2", "www.c.test"},
           {"c3", "dl.c.test"},
           {"n1", ""}}) {
    auto opt = util::copy(option_);
    if (!p.second.empty()) {
      opt->put(PREF_REFERER, "https://" + p.second + "/page");
    }
    auto g = createRequestGroup(0, 0, "sum_" + p.first,
                                "http://host/" + p.first, opt);
    groups[p.first] = g;
    rgman->addReservedGroup(g);
  }
  rgman->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL((size_t)6, rgman->getRequestGroups().size());
  for (const auto& name : {"a1", "b1", "b2", "b3", "c1", "c2"}) {
    CPPUNIT_ASSERT_EQUAL((int)RequestGroup::STATE_ACTIVE,
                         groups[name]->getState());
  }
  groups["a3"]->setPauseRequested(true);

  CPPUNIT_ASSERT_EQUAL(std::string("2 6 | b.test true 3 3/1/0; "
                                   "a.test true 1 1/1/1; "
                                   "idle.test true 4 0/0/0; "
                                   "(none) false 2 0/1/0; "
                                   "c.test false 2 2/1/0"),
                       getSiteSummary(e_));
  CPPUNIT_ASSERT_EQUAL(getTellTally(e_), getSummaryTally(e_));

  // Listing dl.c.test reclassifies both its active and its queued
  // download; the new default and ceiling are reported.
  ChangeGlobalOptionRpcMethod m;
  auto req = createReq(ChangeGlobalOptionRpcMethod::getMethodName());
  auto opt = Dict::g();
  opt->put(PREF_SITE_CONCURRENT->k,
           "b.test:3,a.test:1,idle.test:4,dl.c.test:1");
  opt->put(PREF_SITE_CONCURRENT_DEFAULT->k, "0");
  opt->put(PREF_MAX_CONCURRENT_DOWNLOADS->k, "8");
  req.params->append(std::move(opt));
  CPPUNIT_ASSERT_EQUAL(0, m.execute(std::move(req), e_.get()).code);

  CPPUNIT_ASSERT_EQUAL(std::string("0 8 | b.test true 3 3/1/0; "
                                   "a.test true 1 1/1/1; "
                                   "idle.test true 4 0/0/0; "
                                   "dl.c.test true 1 1/1/0; "
                                   "(none) false 0 0/1/0; "
                                   "c.test false 0 1/0/0"),
                       getSiteSummary(e_));
  CPPUNIT_ASSERT_EQUAL(getTellTally(e_), getSummaryTally(e_));

  TellStatusRpcMethod tellStatus;
  auto statusReq = createReq(TellStatusRpcMethod::getMethodName());
  statusReq.params->append(GroupId::toHex(groups["c3"]->getGID()));
  statusReq.params->append(createKeys({"site"}));
  auto statusRes = tellStatus.execute(std::move(statusReq), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, statusRes.code);
  CPPUNIT_ASSERT_EQUAL(std::string("dl.c.test"),
                       getStringValue(downcast<Dict>(statusRes.param), "site"));
}

void RpcMethodTest::testGetSiteSummary_unset()
{
  // Site options unset: no listed site and no per-site limit.
  CPPUNIT_ASSERT_EQUAL(std::string("0 1 |"), getSiteSummary(e_));
  addUri("http://host/x", e_);
  CPPUNIT_ASSERT_EQUAL(std::string("0 1 | (none) false 0 0/1/0"),
                       getSiteSummary(e_));
  // A listed site is reported without downloads.
  setSiteLimits(*option_, "idle.test:4", "0");
  CPPUNIT_ASSERT_EQUAL(
      std::string("0 1 | idle.test true 4 0/0/0; (none) false 0 0/1/0"),
      getSiteSummary(e_));
}

void RpcMethodTest::testGetSiteSummary_authorize()
{
  auto m = getMethod(GetSiteSummaryRpcMethod::getMethodName());
  CPPUNIT_ASSERT(dynamic_cast<GetSiteSummaryRpcMethod*>(m));
  e_->getOption()->put(PREF_RPC_SECRET, "foo");
  {
    auto req = createReq(GetSiteSummaryRpcMethod::getMethodName());
    auto res = m->execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(1, res.code);
    CPPUNIT_ASSERT(not_authorized(res));
  }
  {
    auto req = createReq(GetSiteSummaryRpcMethod::getMethodName());
    req.params->append("token:foo2");
    auto res = m->execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(1, res.code);
    CPPUNIT_ASSERT(not_authorized(res));
  }
  {
    auto req = createReq(GetSiteSummaryRpcMethod::getMethodName());
    req.params->append("token:foo");
    auto res = m->execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    CPPUNIT_ASSERT(!not_authorized(res));
    CPPUNIT_ASSERT(downcast<Dict>(res.param));
  }
}

#ifdef ENABLE_BITTORRENT
void RpcMethodTest::testGatherBitTorrentMetadata()
{
  auto option = std::make_shared<Option>();
  option->put(PREF_DIR, ".");
  auto dctx = std::make_shared<DownloadContext>();
  bittorrent::load(A2_TEST_DIR "/test.torrent", dctx, option);
  auto btDict = Dict::g();
  gatherBitTorrentMetadata(btDict.get(), bittorrent::getTorrentAttrs(dctx));
  CPPUNIT_ASSERT_EQUAL(std::string("REDNOAH.COM RULES"),
                       downcast<String>(btDict->get("comment"))->s());
  CPPUNIT_ASSERT_EQUAL((int64_t)1123456789,
                       downcast<Integer>(btDict->get("creationDate"))->i());
  CPPUNIT_ASSERT_EQUAL(std::string("multi"),
                       downcast<String>(btDict->get("mode"))->s());
  CPPUNIT_ASSERT_EQUAL(
      std::string("aria2-test"),
      downcast<String>(downcast<Dict>(btDict->get("info"))->get("name"))->s());
  const List* announceList = downcast<List>(btDict->get("announceList"));
  CPPUNIT_ASSERT_EQUAL((size_t)3, announceList->size());
  CPPUNIT_ASSERT_EQUAL(
      std::string("http://tracker1"),
      downcast<String>(downcast<List>(announceList->get(0))->get(0))->s());
  CPPUNIT_ASSERT_EQUAL(
      std::string("http://tracker2"),
      downcast<String>(downcast<List>(announceList->get(1))->get(0))->s());
  CPPUNIT_ASSERT_EQUAL(
      std::string("http://tracker3"),
      downcast<String>(downcast<List>(announceList->get(2))->get(0))->s());
  // Remove some keys
  auto modBtAttrs = bittorrent::getTorrentAttrs(dctx);
  modBtAttrs->comment.clear();
  modBtAttrs->creationDate = 0;
  modBtAttrs->mode = BT_FILE_MODE_NONE;
  modBtAttrs->metadata.clear();
  btDict = Dict::g();
  gatherBitTorrentMetadata(btDict.get(), modBtAttrs);
  CPPUNIT_ASSERT(!btDict->containsKey("comment"));
  CPPUNIT_ASSERT(!btDict->containsKey("creationDate"));
  CPPUNIT_ASSERT(!btDict->containsKey("mode"));
  CPPUNIT_ASSERT(!btDict->containsKey("info"));
  CPPUNIT_ASSERT(btDict->containsKey("announceList"));
}
#endif // ENABLE_BITTORRENT

void RpcMethodTest::testChangePosition()
{
  e_->getRequestGroupMan()->addReservedGroup(
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)));
  e_->getRequestGroupMan()->addReservedGroup(
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)));

  a2_gid_t gid = getReservedGroup(e_->getRequestGroupMan().get(), 0)->getGID();
  ChangePositionRpcMethod m;
  auto req = createReq(ChangePositionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(gid));
  req.params->append(Integer::g(1));
  req.params->append("POS_SET");
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL((int64_t)1, downcast<Integer>(res.param)->i());
  CPPUNIT_ASSERT_EQUAL(
      gid, getReservedGroup(e_->getRequestGroupMan().get(), 1)->getGID());
}

void RpcMethodTest::testChangePosition_fail()
{
  ChangePositionRpcMethod m;
  auto res =
      m.execute(createReq(ChangePositionRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  auto req = createReq(ChangePositionRpcMethod::getMethodName());
  req.params->append("1");
  req.params->append(Integer::g(2));
  req.params->append("bad keyword");
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

namespace {
RpcRequest createChangeUriReq(a2_gid_t gid, size_t fileIndex)
{
  auto req = createReq(ChangeUriRpcMethod::getMethodName());

  req.params->append(GroupId::toHex(gid));   // GID
  req.params->append(Integer::g(fileIndex)); // index of FileEntry
  auto removeuris = List::g();
  removeuris->append("http://example.org/mustremove1");
  removeuris->append("http://example.org/mustremove2");
  removeuris->append("http://example.org/notexist");
  req.params->append(std::move(removeuris));
  return req;
}
} // namespace

void RpcMethodTest::testChangeUri()
{
  std::shared_ptr<FileEntry> files[3];
  for (int i = 0; i < 3; ++i) {
    files[i].reset(new FileEntry());
  }
  files[1]->addUri("http://example.org/aria2.tar.bz2");
  files[1]->addUri("http://example.org/mustremove1");
  files[1]->addUri("http://example.org/mustremove2");
  auto dctx = std::make_shared<DownloadContext>();
  dctx->setFileEntries(&files[0], &files[3]);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(dctx);
  e_->getRequestGroupMan()->addReservedGroup(group);

  ChangeUriRpcMethod m;
  auto req = createChangeUriReq(group->getGID(), 2);
  auto adduris = List::g();
  adduris->append("http://example.org/added1");
  adduris->append("http://example.org/added2");
  adduris->append("baduri");
  adduris->append("http://example.org/added3");
  req.params->append(std::move(adduris));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)2, downcast<Integer>(downcast<List>(res.param)->get(0))->i());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)3, downcast<Integer>(downcast<List>(res.param)->get(1))->i());
  CPPUNIT_ASSERT_EQUAL((size_t)0, files[0]->getRemainingUris().size());
  CPPUNIT_ASSERT_EQUAL((size_t)0, files[2]->getRemainingUris().size());
  std::deque<std::string> uris = files[1]->getRemainingUris();
  CPPUNIT_ASSERT_EQUAL((size_t)4, uris.size());
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/aria2.tar.bz2"),
                       uris[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added1"), uris[1]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added2"), uris[2]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added3"), uris[3]);

  req = createChangeUriReq(group->getGID(), 2);
  // Change adduris
  adduris = List::g();
  adduris->append("http://example.org/added1-1");
  adduris->append("http://example.org/added1-2");
  req.params->append(std::move(adduris));
  // Set position parameter
  req.params->append(Integer::g(2));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)0, downcast<Integer>(downcast<List>(res.param)->get(0))->i());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)2, downcast<Integer>(downcast<List>(res.param)->get(1))->i());
  uris = files[1]->getRemainingUris();
  CPPUNIT_ASSERT_EQUAL((size_t)6, uris.size());
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added1-1"), uris[2]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added1-2"), uris[3]);

  // Change index of FileEntry
  req = createChangeUriReq(group->getGID(), 1);
  adduris = List::g();
  adduris->append("http://example.org/added1-1");
  adduris->append("http://example.org/added1-2");
  req.params->append(std::move(adduris));
  // Set position far beyond the size of uris in FileEntry.
  req.params->append(Integer::g(1000));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)0, downcast<Integer>(downcast<List>(res.param)->get(0))->i());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)2, downcast<Integer>(downcast<List>(res.param)->get(1))->i());
  uris = files[0]->getRemainingUris();
  CPPUNIT_ASSERT_EQUAL((size_t)2, uris.size());
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added1-1"), uris[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added1-2"), uris[1]);
}

namespace {
RpcRequest createChangeUriEmptyReq(a2_gid_t gid, size_t fileIndex)
{
  auto req = createReq(ChangeUriRpcMethod::getMethodName());

  req.params->append(GroupId::toHex(gid));   // GID
  req.params->append(Integer::g(fileIndex)); // index of FileEntry
  req.params->append(List::g());             // remove uris
  req.params->append(List::g());             // append uris
  return req;
}
} // namespace

void RpcMethodTest::testChangeUri_fail()
{
  std::shared_ptr<FileEntry> files[3];
  for (int i = 0; i < 3; ++i) {
    files[i] = std::make_shared<FileEntry>();
  }
  auto dctx = std::make_shared<DownloadContext>();
  dctx->setFileEntries(&files[0], &files[3]);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(dctx);
  e_->getRequestGroupMan()->addReservedGroup(group);

  ChangeUriRpcMethod m;
  auto req = createChangeUriEmptyReq(group->getGID(), 1);
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);

  req = createChangeUriEmptyReq(group->getGID(), 0);
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because 2nd argument is less than 1.
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  req = createChangeUriEmptyReq(GroupId::create()->getNumericId(), 1);
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because the given GID does not exist.
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  req = createChangeUriEmptyReq(group->getGID(), 4);
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because FileEntry#3 does not exist.
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  req = createChangeUriEmptyReq(group->getGID(), 1);
  req.params->set(1, String::g("0"));
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because index of FileEntry is string.
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  req = createChangeUriEmptyReq(group->getGID(), 1);
  req.params->set(2, String::g("http://url"));
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because 3rd param is not list.
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  req = createChangeUriEmptyReq(group->getGID(), 1);
  req.params->set(2, List::g());
  req.params->set(3, String::g("http://url"));
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because 4th param is not list.
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testGetSessionInfo()
{
  GetSessionInfoRpcMethod m;
  auto res =
      m.execute(createReq(GetSessionInfoRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(util::toHex(e_->getSessionId()),
                       getString(downcast<Dict>(res.param), "sessionId"));
}

void RpcMethodTest::testPause()
{
  std::vector<std::string> uris{
      "http://url1",
      "http://url2",
      "http://url3",
  };
  option_->put(PREF_FORCE_SEQUENTIAL, A2_V_TRUE);
  std::vector<std::shared_ptr<RequestGroup>> groups;
  createRequestGroupForUri(groups, option_, uris);
  CPPUNIT_ASSERT_EQUAL((size_t)3, groups.size());
  e_->getRequestGroupMan()->addReservedGroup(groups);
  {
    PauseRpcMethod m;
    auto req = createReq(PauseRpcMethod::getMethodName());
    req.params->append(GroupId::toHex(groups[0]->getGID()));
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  CPPUNIT_ASSERT(groups[0]->isPauseRequested());
  {
    UnpauseRpcMethod m;
    auto req = createReq(UnpauseRpcMethod::getMethodName());
    req.params->append(GroupId::toHex(groups[0]->getGID()));
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  CPPUNIT_ASSERT(!groups[0]->isPauseRequested());
  {
    PauseAllRpcMethod m;
    auto req = createReq(PauseAllRpcMethod::getMethodName());
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  for (size_t i = 0; i < groups.size(); ++i) {
    CPPUNIT_ASSERT(groups[i]->isPauseRequested());
  }
  {
    UnpauseAllRpcMethod m;
    auto req = createReq(UnpauseAllRpcMethod::getMethodName());
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  for (size_t i = 0; i < groups.size(); ++i) {
    CPPUNIT_ASSERT(!groups[i]->isPauseRequested());
  }
  {
    ForcePauseAllRpcMethod m;
    auto req = createReq(ForcePauseAllRpcMethod::getMethodName());
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  for (size_t i = 0; i < groups.size(); ++i) {
    CPPUNIT_ASSERT(groups[i]->isPauseRequested());
  }
}

void RpcMethodTest::testSystemMulticall()
{
  SystemMulticallRpcMethod m;
  auto req = createReq("system.multicall");
  auto reqparams = List::g();
  for (int i = 0; i < 2; ++i) {
    auto dict = Dict::g();
    dict->put("methodName", AddUriRpcMethod::getMethodName());
    auto params = List::g();
    auto urisParam = List::g();
    urisParam->append("http://localhost/" + util::itos(i));
    params->append(std::move(urisParam));
    dict->put("params", std::move(params));
    reqparams->append(std::move(dict));
  }
  {
    auto dict = Dict::g();
    dict->put("methodName", "not exists");
    dict->put("params", List::g());
    reqparams->append(std::move(dict));
  }
  {
    reqparams->append("not struct");
  }
  {
    auto dict = Dict::g();
    dict->put("methodName", "system.multicall");
    dict->put("params", List::g());
    reqparams->append(std::move(dict));
  }
  {
    // missing params
    auto dict = Dict::g();
    dict->put("methodName", GetVersionRpcMethod::getMethodName());
    reqparams->append(std::move(dict));
  }
  {
    auto dict = Dict::g();
    dict->put("methodName", GetVersionRpcMethod::getMethodName());
    dict->put("params", List::g());
    reqparams->append(std::move(dict));
  }
  req.params->append(std::move(reqparams));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const List* resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)7, resParams->size());
  auto& rgman = e_->getRequestGroupMan();
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 0)->getGID()),
      downcast<String>(downcast<List>(resParams->get(0))->get(0))->s());
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 1)->getGID()),
      downcast<String>(downcast<List>(resParams->get(1))->get(0))->s());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)1,
      downcast<Integer>(downcast<Dict>(resParams->get(2))->get("faultCode"))
          ->i());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)1,
      downcast<Integer>(downcast<Dict>(resParams->get(3))->get("faultCode"))
          ->i());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)1,
      downcast<Integer>(downcast<Dict>(resParams->get(4))->get("faultCode"))
          ->i());
  CPPUNIT_ASSERT(downcast<List>(resParams->get(5)));
  CPPUNIT_ASSERT(downcast<List>(resParams->get(6)));
}

void RpcMethodTest::testSystemMulticall_fail()
{
  SystemMulticallRpcMethod m;
  auto res = m.execute(createReq("system.multicall"), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testSystemListMethods()
{
  SystemListMethodsRpcMethod m;
  auto res = m.execute(createReq("system.listMethods"), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);

  const auto resParams = downcast<List>(res.param);
  auto& allNames = allMethodNames();

  CPPUNIT_ASSERT_EQUAL(allNames.size(), resParams->size());

  bool siteSummaryListed = false;
  for (size_t i = 0; i < allNames.size(); ++i) {
    const auto s = downcast<String>(resParams->get(i));
    CPPUNIT_ASSERT(s);
    CPPUNIT_ASSERT_EQUAL(allNames[i], s->s());
    if (s->s() == GetSiteSummaryRpcMethod::getMethodName()) {
      siteSummaryListed = true;
    }
  }
  CPPUNIT_ASSERT(siteSummaryListed);
}

void RpcMethodTest::testSystemListNotifications()
{
  SystemListNotificationsRpcMethod m;
  auto res = m.execute(createReq("system.listNotifications"), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);

  const auto resParams = downcast<List>(res.param);
  auto& allNames = allNotificationsNames();

  CPPUNIT_ASSERT_EQUAL(allNames.size(), resParams->size());

  for (size_t i = 0; i < allNames.size(); ++i) {
    const auto s = downcast<String>(resParams->get(i));
    CPPUNIT_ASSERT(s);
    CPPUNIT_ASSERT_EQUAL(allNames[i], s->s());
  }
}

namespace {
const char SECRET[] = "secret";

// The methods the status page passphrase may call, besides
// aria2.changeGlobalOption with the site keys.
const std::vector<std::string> PASSPHRASE_METHODS = {
    "aria2.tellActive",     "aria2.tellWaiting",
    "aria2.tellStopped",    "aria2.tellStatus",
    "aria2.getGlobalStat",  "aria2.pause",
    "aria2.unpause",        "aria2.remove",
    "aria2.purgeDownloadResult", "ariam.getSiteSummary",
    "system.multicall"};

// Runs methodName through dispatch() as a passphrase-authorized call.
RpcResponse passphraseCall(DownloadEngine* e, const std::string& methodName,
                           std::unique_ptr<List> params = List::g())
{
  RpcRequest req(methodName, std::move(params), Integer::g(1), true);
  req.passphraseAuthorized = true;
  return dispatch(std::move(req), e);
}

std::unique_ptr<List> withToken(std::unique_ptr<List> params,
                                const std::string& token = SECRET)
{
  auto res = List::g();
  res->append("token:" + token);
  for (auto& v : *params) {
    res->append(std::move(v));
  }
  return res;
}

std::unique_ptr<List> paramsOf(std::unique_ptr<ValueBase> v)
{
  auto params = List::g();
  params->append(std::move(v));
  return params;
}

std::unique_ptr<List> gidParams(const std::string& gid)
{
  return paramsOf(String::g(gid));
}

std::unique_ptr<List> rangeParams()
{
  auto params = List::g();
  params->append(Integer::g(0));
  params->append(Integer::g(10));
  return params;
}

std::unique_ptr<List>
optionParams(const std::vector<std::pair<std::string, std::string>>& options)
{
  auto dict = Dict::g();
  for (const auto& kv : options) {
    dict->put(kv.first, kv.second);
  }
  return paramsOf(std::move(dict));
}

// A system.multicall method spec.
std::unique_ptr<Dict> createCall(const std::string& methodName,
                                 std::unique_ptr<List> params = List::g())
{
  auto call = Dict::g();
  call->put("methodName", methodName);
  call->put("params", std::move(params));
  return call;
}

void assertRuns(const RpcResponse& res, const std::string& what)
{
  CPPUNIT_ASSERT_EQUAL_MESSAGE(what, 0, res.code);
  CPPUNIT_ASSERT_MESSAGE(what, !not_authorized(res));
}

void assertUnauthorizedFault(const ValueBase* fault, const std::string& what)
{
  auto error = downcast<Dict>(fault);
  CPPUNIT_ASSERT_MESSAGE(what, error);
  CPPUNIT_ASSERT_EQUAL_MESSAGE(what, std::string("Unauthorized"),
                               getStringValue(error, "message"));
  auto code = downcast<Integer>(error->get("code"));
  CPPUNIT_ASSERT_MESSAGE(what, code);
  CPPUNIT_ASSERT_EQUAL_MESSAGE(what, (Integer::ValueType)1, code->i());
}

void assertUnauthorized(const RpcResponse& res, const std::string& what)
{
  CPPUNIT_ASSERT_EQUAL_MESSAGE(what, 1, res.code);
  CPPUNIT_ASSERT_MESSAGE(what, not_authorized(res));
  assertUnauthorizedFault(res.param.get(), what);
}
} // namespace

void RpcMethodTest::testPassphrase_allowlist()
{
  option_->put(PREF_RPC_SECRET, SECRET);
  auto& rgman = e_->getRequestGroupMan();
  auto active = createRequestGroup(0, 0, "pp_active", "http://host/active",
                                   util::copy(option_));
  auto waiting = createRequestGroup(0, 0, "pp_waiting", "http://host/waiting",
                                    util::copy(option_));
  rgman->addReservedGroup(active);
  rgman->addReservedGroup(waiting);
  rgman->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL((int)RequestGroup::STATE_ACTIVE, active->getState());
  auto stopped = createDownloadResult(error_code::FINISHED, "http://host/done");
  rgman->addDownloadResult(stopped);
  auto waitingGid = GroupId::toHex(waiting->getGID());

  assertRuns(passphraseCall(e_.get(), "aria2.tellActive"), "tellActive");
  assertRuns(passphraseCall(e_.get(), "aria2.tellWaiting", rangeParams()),
             "tellWaiting");
  assertRuns(passphraseCall(e_.get(), "aria2.tellStopped", rangeParams()),
             "tellStopped");
  assertRuns(passphraseCall(e_.get(), "aria2.tellStatus", gidParams(waitingGid)),
             "tellStatus");
  assertRuns(passphraseCall(e_.get(), "aria2.tellStatus",
                            gidParams(stopped->gid->toHex())),
             "tellStatus stopped");
  assertRuns(passphraseCall(e_.get(), "aria2.getGlobalStat"), "getGlobalStat");
  assertRuns(passphraseCall(e_.get(), "ariam.getSiteSummary"),
             "getSiteSummary");
  assertRuns(passphraseCall(e_.get(), "aria2.pause", gidParams(waitingGid)),
             "pause");
  CPPUNIT_ASSERT(waiting->isPauseRequested());
  assertRuns(passphraseCall(e_.get(), "aria2.unpause", gidParams(waitingGid)),
             "unpause");
  CPPUNIT_ASSERT(!waiting->isPauseRequested());
  assertRuns(passphraseCall(e_.get(), "aria2.changeGlobalOption",
                            optionParams({{PREF_SITE_CONCURRENT->k, "a.test:1"}})),
             "changeGlobalOption");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1"),
                       option_->get(PREF_SITE_CONCURRENT));
  {
    auto calls = List::g();
    calls->append(createCall("aria2.getGlobalStat"));
    auto res = passphraseCall(e_.get(), "system.multicall",
                              paramsOf(std::move(calls)));
    assertRuns(res, "multicall");
    auto list = downcast<List>(res.param);
    CPPUNIT_ASSERT_EQUAL((size_t)1, list->size());
    CPPUNIT_ASSERT(downcast<List>(list->get(0)));
  }
  assertRuns(passphraseCall(e_.get(), "aria2.purgeDownloadResult"),
             "purgeDownloadResult");
  CPPUNIT_ASSERT_EQUAL((size_t)0, rgman->getDownloadResults().size());
  assertRuns(passphraseCall(e_.get(), "aria2.remove", gidParams(waitingGid)),
             "remove");
  CPPUNIT_ASSERT(!rgman->findGroup(waiting->getGID()));
}

void RpcMethodTest::testPassphrase_deniedMethods()
{
  option_->put(PREF_RPC_SECRET, SECRET);
  auto& rgman = e_->getRequestGroupMan();
  auto names = allMethodNames();
  names.push_back("aria2.noSuchMethod");
  names.push_back("");
  size_t denied = 0;
  for (const auto& name : names) {
    if (name == ChangeGlobalOptionRpcMethod::getMethodName() ||
        std::find(std::begin(PASSPHRASE_METHODS), std::end(PASSPHRASE_METHODS),
                  name) != std::end(PASSPHRASE_METHODS)) {
      continue;
    }
    // Run, each would answer with a result or a parameter error instead.
    assertUnauthorized(passphraseCall(e_.get(), name), name);
    ++denied;
  }
  CPPUNIT_ASSERT(denied >= 20);

  // Denied before it runs: nothing is added, changed, or removed.
  auto group = createRequestGroup(0, 0, "pp_denied", "http://host/denied",
                                  util::copy(option_));
  rgman->addReservedGroup(group);
  auto gid = GroupId::toHex(group->getGID());
  {
    auto uris = List::g();
    uris->append("http://host/added");
    assertUnauthorized(
        passphraseCall(e_.get(), "aria2.addUri", paramsOf(std::move(uris))),
        "addUri");
    CPPUNIT_ASSERT_EQUAL((size_t)1, rgman->getReservedGroups().size());
  }
  {
    auto params = gidParams(gid);
    auto options = Dict::g();
    options->put(PREF_MAX_DOWNLOAD_LIMIT->k, "1K");
    params->append(std::move(options));
    assertUnauthorized(
        passphraseCall(e_.get(), "aria2.changeOption", std::move(params)),
        "changeOption");
    CPPUNIT_ASSERT(!group->getOption()->defined(PREF_MAX_DOWNLOAD_LIMIT));
  }
  for (const auto& name : {"aria2.forceRemove", "aria2.getOption",
                           "aria2.getUris", "aria2.getFiles"}) {
    assertUnauthorized(passphraseCall(e_.get(), name, gidParams(gid)), name);
  }
  CPPUNIT_ASSERT(rgman->findGroup(group->getGID()));
}

void RpcMethodTest::testPassphrase_tokenCall()
{
  option_->put(PREF_RPC_SECRET, SECRET);
  auto& rgman = e_->getRequestGroupMan();
  {
    // A call that carries a token is authorized by its token alone,
    // on the passphrase path too.
    auto uris = List::g();
    uris->append("http://host/token");
    assertRuns(passphraseCall(e_.get(), "aria2.addUri",
                              withToken(paramsOf(std::move(uris)))),
               "addUri with token");
    CPPUNIT_ASSERT_EQUAL((size_t)1, rgman->getReservedGroups().size());
  }
  // A wrong token is not saved by the passphrase: an allowed call is
  // unauthorized, and a denied one stays denied.
  assertUnauthorized(passphraseCall(e_.get(), "aria2.tellActive",
                                    withToken(List::g(), "wrong")),
                     "tellActive with a wrong token");
  assertUnauthorized(passphraseCall(e_.get(), "aria2.getGlobalStat",
                                    withToken(List::g(), "wrong")),
                     "getGlobalStat with a wrong token");
  const auto sites = option_->get(PREF_SITE_CONCURRENT);
  assertUnauthorized(
      passphraseCall(e_.get(), "aria2.changeGlobalOption",
                     withToken(optionParams({{PREF_SITE_CONCURRENT->k,
                                              "wrong.test:1"}}),
                               "wrong")),
      "changeGlobalOption with a wrong token");
  CPPUNIT_ASSERT_EQUAL(sites, option_->get(PREF_SITE_CONCURRENT));
  {
    auto uris = List::g();
    uris->append("http://host/wrong");
    assertUnauthorized(passphraseCall(e_.get(), "aria2.addUri",
                                      withToken(paramsOf(std::move(uris)),
                                                "wrong")),
                       "addUri with a wrong token");
    CPPUNIT_ASSERT_EQUAL((size_t)1, rgman->getReservedGroups().size());
  }
  // "token:" alone is a token too: an empty, wrong one.
  assertUnauthorized(passphraseCall(e_.get(), "aria2.tellActive",
                                    withToken(List::g(), "")),
                     "tellActive with an empty token");
  // Without the passphrase, a call without a token is unauthorized.
  assertUnauthorized(dispatch(RpcRequest("aria2.tellActive", List::g(),
                                         Integer::g(1), true),
                              e_.get()),
                     "tellActive without token");
}

void RpcMethodTest::testPassphrase_changeGlobalOption()
{
  option_->put(PREF_RPC_SECRET, SECRET);
  auto& rgman = e_->getRequestGroupMan();
  const auto siteKey = std::string(PREF_SITE_CONCURRENT->k);
  const auto defaultKey = std::string(PREF_SITE_CONCURRENT_DEFAULT->k);
  assertRuns(passphraseCall(e_.get(), "aria2.changeGlobalOption",
                            optionParams({{siteKey, "a.test:2"},
                                          {defaultKey, "3"}})),
             "site keys");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:2"),
                       option_->get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("3"),
                       option_->get(PREF_SITE_CONCURRENT_DEFAULT));

  // Any other key rejects the whole call.
  assertUnauthorized(
      passphraseCall(e_.get(), "aria2.changeGlobalOption",
                     optionParams({{siteKey, "b.test:1"},
                                   {PREF_MAX_CONCURRENT_DOWNLOADS->k, "9"}})),
      "with max-concurrent-downloads");
  for (const auto& key :
       {PREF_SITE_CONCURRENT_FILE->k, PREF_MAX_OVERALL_DOWNLOAD_LIMIT->k,
        PREF_DIR->k, PREF_RPC_SECRET->k, PREF_STATUS_PAGE_PASSPHRASE->k,
        "no-such-option"}) {
    assertUnauthorized(passphraseCall(e_.get(), "aria2.changeGlobalOption",
                                      optionParams({{defaultKey, "4"},
                                                    {key, "x"}})),
                       key);
  }
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:2"),
                       option_->get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("3"),
                       option_->get(PREF_SITE_CONCURRENT_DEFAULT));
  CPPUNIT_ASSERT_EQUAL(1, rgman->getMaxConcurrentDownloads());
  CPPUNIT_ASSERT(!option_->defined(PREF_SITE_CONCURRENT_FILE));
  CPPUNIT_ASSERT_EQUAL(std::string(SECRET), option_->get(PREF_RPC_SECRET));

  // Without an options struct.
  assertUnauthorized(passphraseCall(e_.get(), "aria2.changeGlobalOption"),
                     "no params");
  assertUnauthorized(passphraseCall(e_.get(), "aria2.changeGlobalOption",
                                    paramsOf(String::g(siteKey + "=c.test:1"))),
                     "string param");
  // An empty struct changes nothing.
  assertRuns(passphraseCall(e_.get(), "aria2.changeGlobalOption",
                            optionParams({})),
             "empty struct");
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:2"),
                       option_->get(PREF_SITE_CONCURRENT));

  // The token path is unchanged.
  assertRuns(passphraseCall(
                 e_.get(), "aria2.changeGlobalOption",
                 withToken(optionParams(
                     {{PREF_MAX_CONCURRENT_DOWNLOADS->k, "9"}}))),
             "token");
  CPPUNIT_ASSERT_EQUAL(9, rgman->getMaxConcurrentDownloads());
}

void RpcMethodTest::testPassphrase_systemMethods()
{
  option_->put(PREF_RPC_SECRET, SECRET);
  const std::vector<std::string> names = {"system.listMethods",
                                          "system.listNotifications"};
  for (const auto& name : names) {
    // Both override execute() and never call authorize().
    assertUnauthorized(passphraseCall(e_.get(), name), name);
    // A wrong token does not take them off the passphrase path.
    for (const auto& token : {"wrong", ""}) {
      assertUnauthorized(
          passphraseCall(e_.get(), name, withToken(List::g(), token)),
          name + " with token " + token);
    }
    // A valid token takes the token path.
    {
      auto res = passphraseCall(e_.get(), name, withToken(List::g()));
      assertRuns(res, name + " with token");
      CPPUNIT_ASSERT(downcast<List>(res.param));
    }
    // Stock: they need no token.
    auto res =
        dispatch(RpcRequest(name, List::g(), Integer::g(1), true), e_.get());
    assertRuns(res, name);
    CPPUNIT_ASSERT(downcast<List>(res.param));
  }
  auto calls = List::g();
  for (const auto& name : names) {
    calls->append(createCall(name));
    calls->append(createCall(name, withToken(List::g(), "wrong")));
  }
  for (const auto& name : names) {
    calls->append(createCall(name, withToken(List::g())));
  }
  auto res =
      passphraseCall(e_.get(), "system.multicall", paramsOf(std::move(calls)));
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT(not_authorized(res));
  auto list = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL(names.size() * 3, list->size());
  for (size_t i = 0; i < names.size() * 2; ++i) {
    assertUnauthorizedFault(list->get(i), "multicall " + util::uitos(i));
  }
  for (size_t i = names.size() * 2; i < list->size(); ++i) {
    auto result = downcast<List>(list->get(i));
    CPPUNIT_ASSERT_MESSAGE(util::uitos(i), result);
    CPPUNIT_ASSERT_MESSAGE(util::uitos(i), downcast<List>(result->get(0)));
  }
}

void RpcMethodTest::testPassphrase_multicall()
{
  option_->put(PREF_RPC_SECRET, SECRET);
  auto& rgman = e_->getRequestGroupMan();
  auto calls = List::g();
  calls->append(createCall("aria2.getGlobalStat"));
  {
    auto uris = List::g();
    uris->append("http://host/added");
    calls->append(createCall("aria2.addUri", paramsOf(std::move(uris))));
  }
  calls->append(createCall(
      "aria2.changeGlobalOption",
      optionParams({{PREF_MAX_CONCURRENT_DOWNLOADS->k, "5"}})));
  calls->append(createCall(
      "aria2.changeGlobalOption",
      optionParams({{PREF_SITE_CONCURRENT_DEFAULT->k, "4"}})));
  calls->append(createCall("aria2.getGlobalOption", withToken(List::g())));
  calls->append(
      createCall("aria2.tellActive", withToken(List::g(), "wrong")));
  calls->append(createCall("aria2.getVersion"));
  auto res =
      passphraseCall(e_.get(), "system.multicall", paramsOf(std::move(calls)));
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  // Denied sub-calls make the response unauthorized, so it is delayed
  // like any unauthorized response.
  CPPUNIT_ASSERT(not_authorized(res));
  auto list = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)7, list->size());
  for (size_t i : {0, 3, 4}) {
    CPPUNIT_ASSERT_MESSAGE(util::uitos(i), downcast<List>(list->get(i)));
  }
  for (size_t i : {1, 2, 5, 6}) {
    assertUnauthorizedFault(list->get(i), util::uitos(i));
  }
  CPPUNIT_ASSERT_EQUAL((size_t)0, rgman->getReservedGroups().size());
  CPPUNIT_ASSERT_EQUAL(1, rgman->getMaxConcurrentDownloads());
  CPPUNIT_ASSERT_EQUAL(std::string("4"),
                       option_->get(PREF_SITE_CONCURRENT_DEFAULT));
  // The sub-call with a valid token ran on the token path.
  auto globalOptions = downcast<Dict>(downcast<List>(list->get(4))->get(0));
  CPPUNIT_ASSERT(globalOptions);
  CPPUNIT_ASSERT(globalOptions->containsKey(PREF_DIR->k));

  // Without the passphrase flag, each sub-call takes the token path.
  calls = List::g();
  calls->append(createCall("aria2.getGlobalStat"));
  calls->append(createCall("system.listMethods"));
  res = dispatch(RpcRequest("system.multicall", paramsOf(std::move(calls)),
                            Integer::g(1), true),
                 e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  list = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)2, list->size());
  assertUnauthorizedFault(list->get(0), "getGlobalStat without token");
  CPPUNIT_ASSERT(downcast<List>(list->get(1)));
}

namespace {
// entry is a tell* entry of a download with one file: files[].uris is
// present unless redacted, errorMessage only if stopped and not
// redacted.
void checkRedaction(const Dict* entry, bool redacted, bool stopped,
                    const std::string& what)
{
  CPPUNIT_ASSERT_MESSAGE(what, entry);
  CPPUNIT_ASSERT_EQUAL_MESSAGE(what, !redacted && stopped,
                               entry->containsKey("errorMessage"));
  CPPUNIT_ASSERT_MESSAGE(what, entry->containsKey("gid"));
  auto files = downcast<List>(entry->get("files"));
  CPPUNIT_ASSERT_MESSAGE(what, files);
  CPPUNIT_ASSERT_EQUAL_MESSAGE(what, (size_t)1, files->size());
  auto file = downcast<Dict>(files->get(0));
  CPPUNIT_ASSERT_MESSAGE(what, file);
  CPPUNIT_ASSERT_MESSAGE(what, file->containsKey("path"));
  CPPUNIT_ASSERT_EQUAL_MESSAGE(what, !redacted, file->containsKey("uris"));
  if (!redacted) {
    CPPUNIT_ASSERT_MESSAGE(what, downcast<List>(file->get("uris")));
  }
}

const Dict* onlyEntry(const RpcResponse& res, const std::string& what)
{
  auto list = downcast<List>(res.param);
  CPPUNIT_ASSERT_MESSAGE(what, list);
  CPPUNIT_ASSERT_EQUAL_MESSAGE(what, (size_t)1, list->size());
  return downcast<Dict>(list->get(0));
}
} // namespace

void RpcMethodTest::testPassphrase_redaction()
{
  option_->put(PREF_RPC_SECRET, SECRET);
  auto& rgman = e_->getRequestGroupMan();
  auto active = createRequestGroup(0, 0, "pp_active",
                                   "http://host/active?sig=1",
                                   util::copy(option_));
  auto waiting = createRequestGroup(0, 0, "pp_waiting",
                                    "http://host/waiting?sig=2",
                                    util::copy(option_));
  rgman->addReservedGroup(active);
  rgman->addReservedGroup(waiting);
  rgman->fillRequestGroupFromReserver(e_.get());
  CPPUNIT_ASSERT_EQUAL((int)RequestGroup::STATE_ACTIVE, active->getState());
  auto stopped = createDownloadResult(error_code::UNKNOWN_ERROR,
                                      "http://host/stopped?sig=3");
  stopped->resultMessage = "Redirect to https://cdn.test/stopped?sig=3 failed";
  rgman->addDownloadResult(stopped);
  const std::vector<std::string> keys = {"gid", "files", "errorMessage"};

  for (bool redacted : {true, false}) {
    // A token takes the token path: its responses are not redacted.
    auto run = [&](const std::string& name, std::unique_ptr<List> params,
                   const std::string& what) {
      auto res = passphraseCall(e_.get(), name,
                                redacted ? std::move(params)
                                         : withToken(std::move(params)));
      assertRuns(res, what);
      return res;
    };
    for (bool filtered : {false, true}) {
      auto what = std::string(redacted ? "passphrase " : "token ") +
                  (filtered ? "keys " : "");
      auto withKeys = [&](std::unique_ptr<List> params) {
        if (filtered) {
          params->append(createKeys(keys));
        }
        return params;
      };
      checkRedaction(onlyEntry(run("aria2.tellActive", withKeys(List::g()),
                                   what + "tellActive"),
                               what),
                     redacted, false, what + "tellActive");
      checkRedaction(onlyEntry(run("aria2.tellWaiting", withKeys(rangeParams()),
                                   what + "tellWaiting"),
                               what),
                     redacted, false, what + "tellWaiting");
      checkRedaction(onlyEntry(run("aria2.tellStopped", withKeys(rangeParams()),
                                   what + "tellStopped"),
                               what),
                     redacted, true, what + "tellStopped");
      for (const auto& g :
           {std::make_pair(GroupId::toHex(active->getGID()), false),
            std::make_pair(GroupId::toHex(waiting->getGID()), false),
            std::make_pair(stopped->gid->toHex(), true)}) {
        auto res = run("aria2.tellStatus", withKeys(gidParams(g.first)),
                       what + "tellStatus " + g.first);
        checkRedaction(downcast<Dict>(res.param), redacted, g.second,
                       what + "tellStatus " + g.first);
      }
    }
    {
      auto params = gidParams(stopped->gid->toHex());
      params->append(createKeys({"errorMessage"}));
      auto res = run("aria2.tellStatus", std::move(params), "errorMessage only");
      auto entry = downcast<Dict>(res.param);
      if (redacted) {
        CPPUNIT_ASSERT_EQUAL((size_t)0, entry->size());
      }
      else {
        CPPUNIT_ASSERT_EQUAL(stopped->resultMessage,
                             getStringValue(entry, "errorMessage"));
      }
    }
  }

  // In system.multicall each sub-call is redacted on its own: the
  // token of one does not un-redact the other.
  auto calls = List::g();
  calls->append(createCall("aria2.tellStopped", rangeParams()));
  calls->append(createCall("aria2.tellStopped", withToken(rangeParams())));
  auto res =
      passphraseCall(e_.get(), "system.multicall", paramsOf(std::move(calls)));
  assertRuns(res, "multicall");
  auto list = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)2, list->size());
  for (size_t i = 0; i < 2; ++i) {
    auto result = downcast<List>(list->get(i));
    CPPUNIT_ASSERT(result);
    auto entries = downcast<List>(result->get(0));
    CPPUNIT_ASSERT(entries);
    CPPUNIT_ASSERT_EQUAL((size_t)1, entries->size());
    checkRedaction(downcast<Dict>(entries->get(0)), i == 0, true,
                   "multicall " + util::uitos(i));
  }
}

} // namespace rpc

} // namespace aria2
