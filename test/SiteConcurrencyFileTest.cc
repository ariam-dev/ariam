#include "SiteConcurrencyFile.h"

#include <cerrno>
#include <csignal>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef __MINGW32__
#  include <sys/resource.h>
#  include <unistd.h>
#endif // !__MINGW32__

#include <cppunit/extensions/HelperMacros.h>

#include <aria2/aria2.h>
#include "TestUtil.h"
#include "Option.h"
#include "OptionParser.h"
#include "OptionHandler.h"
#include "prefs.h"
#include "File.h"
#include "Exception.h"
#include "error_code.h"
#include "DownloadEngine.h"
#include "SelectEventPoll.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "RpcMethodImpl.h"
#include "RpcRequest.h"
#include "RpcResponse.h"
#include "ValueBase.h"
#include "LogFactory.h"
#include "Logger.h"
#include "a2functional.h"
#include "util.h"

namespace aria2 {

error_code::Value option_processing(Option& op, bool standalone,
                                    std::vector<std::string>& uris, int argc,
                                    char** argv, const KeyVals& options);

class SiteConcurrencyFileTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SiteConcurrencyFileTest);
  CPPUNIT_TEST(testSaveAndLoad);
  CPPUNIT_TEST(testLoad_missing);
  CPPUNIT_TEST(testLoad_commentsAndPartial);
  CPPUNIT_TEST(testLoad_invalid);
#ifndef __MINGW32__
  CPPUNIT_TEST(testLoad_statFails);
#endif // !__MINGW32__
  CPPUNIT_TEST(testChangeGlobalOption_persists);
  CPPUNIT_TEST(testChangeGlobalOption_noFile);
  CPPUNIT_TEST(testChangeGlobalOption_badValueNotPersisted);
  CPPUNIT_TEST(testChangeGlobalOption_openFails);
  CPPUNIT_TEST(testChangeGlobalOption_renameFails);
#ifndef __MINGW32__
  CPPUNIT_TEST(testChangeGlobalOption_flushFails);
#endif // !__MINGW32__
  CPPUNIT_TEST(testStartup_restoresPersistedValues);
  CPPUNIT_TEST(testStartup_invalidFile);
#ifndef __MINGW32__
  CPPUNIT_TEST(testStartup_statFails);
#endif // !__MINGW32__
  CPPUNIT_TEST_SUITE_END();

  std::string dir_;
  std::string path_;
  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;

public:
  void setUp()
  {
    dir_ = A2_TEST_OUT_DIR "/aria2_SiteConcurrencyFileTest";
    path_ = dir_ + "/sites.conf";
    File(dir_).mkdirs();
    // Leftovers of earlier runs, including empty directories some tests
    // create.
    for (const auto& p : {path_, path_ + "__temp", dir_ + "/dir.conf",
                          dir_ + "/dir.conf__temp", dir_ + "/aria2.conf",
                          std::string("__temp")}) {
      File(p).remove();
    }
    option_ = std::make_shared<Option>();
    OptionParser::getInstance()->parseDefaultValues(*option_);
    e_ = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
    e_->setOption(option_.get());
    e_->setRequestGroupMan(make_unique<RequestGroupMan>(
        std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get()));
  }

  void testSaveAndLoad();
  void testLoad_missing();
  void testLoad_commentsAndPartial();
  void testLoad_invalid();
  void testLoad_statFails();
  void testChangeGlobalOption_persists();
  void testChangeGlobalOption_noFile();
  void testChangeGlobalOption_badValueNotPersisted();
  void testChangeGlobalOption_openFails();
  void testChangeGlobalOption_renameFails();
  void testChangeGlobalOption_flushFails();
  void testStartup_restoresPersistedValues();
  void testStartup_invalidFile();
  void testStartup_statFails();
};

CPPUNIT_TEST_SUITE_REGISTRATION(SiteConcurrencyFileTest);

namespace {
const char PREVIOUS[] = "site-concurrent=old.test:1\n"
                        "site-concurrent-default=1\n";

void writeFile(const std::string& path, const std::string& content)
{
  std::ofstream o(path.c_str(), std::ios::binary);
  o << content;
}

void parse(Option& option, PrefPtr pref, const std::string& value)
{
  OptionParser::getInstance()->find(pref)->parse(option, value);
}

rpc::RpcResponse
changeGlobalOption(DownloadEngine* e,
                   const std::vector<std::pair<std::string, std::string>>& kvs)
{
  rpc::ChangeGlobalOptionRpcMethod m;
  rpc::RpcRequest req(rpc::ChangeGlobalOptionRpcMethod::getMethodName(),
                      List::g());
  auto opt = Dict::g();
  for (const auto& kv : kvs) {
    opt->put(kv.first, kv.second);
  }
  req.params->append(std::move(opt));
  return m.execute(std::move(req), e);
}

std::string faultString(const rpc::RpcResponse& res)
{
  return downcast<String>(downcast<Dict>(res.param)->get("faultString"))->s();
}

bool contains(const std::string& s, const std::string& sub)
{
  return s.find(sub) != std::string::npos;
}
} // namespace

void SiteConcurrencyFileTest::testSaveAndLoad()
{
  Option values;
  parse(values, PREF_SITE_CONCURRENT, "a.test:1, b.test:3");
  parse(values, PREF_SITE_CONCURRENT_DEFAULT, "2");
  writeFile(path_, PREVIOUS);
  saveSiteConcurrencyFile(path_, values);
  CPPUNIT_ASSERT_EQUAL(std::string("site-concurrent=a.test:1,b.test:3\n"
                                   "site-concurrent-default=2\n"),
                       readFile(path_));
  CPPUNIT_ASSERT(!File(path_ + "__temp").exists());

  Option loaded;
  CPPUNIT_ASSERT(loadSiteConcurrencyFile(loaded, path_));
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1,b.test:3"),
                       loaded.get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("2"),
                       loaded.get(PREF_SITE_CONCURRENT_DEFAULT));

  // An empty list round-trips.
  parse(values, PREF_SITE_CONCURRENT, "");
  saveSiteConcurrencyFile(path_, values);
  CPPUNIT_ASSERT(loadSiteConcurrencyFile(loaded, path_));
  CPPUNIT_ASSERT(loaded.defined(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string(), loaded.get(PREF_SITE_CONCURRENT));
}

void SiteConcurrencyFileTest::testLoad_missing()
{
  Option option;
  option.put(PREF_SITE_CONCURRENT, "a.test:1");
  CPPUNIT_ASSERT(!loadSiteConcurrencyFile(option, path_));
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1"),
                       option.get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT(!option.defined(PREF_SITE_CONCURRENT_DEFAULT));
}

void SiteConcurrencyFileTest::testLoad_commentsAndPartial()
{
  writeFile(path_, "# site limits\n"
                   "\n"
                   " site-concurrent-default = 3 \n");
  Option option;
  option.put(PREF_SITE_CONCURRENT, "a.test:1");
  CPPUNIT_ASSERT(loadSiteConcurrencyFile(option, path_));
  CPPUNIT_ASSERT_EQUAL(std::string("3"),
                       option.get(PREF_SITE_CONCURRENT_DEFAULT));
  // A value the file does not hold is not overridden.
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1"),
                       option.get(PREF_SITE_CONCURRENT));
}

void SiteConcurrencyFileTest::testLoad_invalid()
{
  const std::pair<const char*, const char*> bad[] = {
      {"site-concurrent=a.test:1,a.test:2\n", "listed twice"},
      {"site-concurrent=a.test:0\n", "greater than or equal to 1"},
      {"site-concurrent-default=-1\n", "site-concurrent-default"},
      {"site-concurrent-default=2\ndir=/tmp\n",
       "option 'dir' is not allowed"},
      {"no-such-option=1\n", "option 'no-such-option' is not allowed"},
  };
  for (const auto& b : bad) {
    writeFile(path_, b.first);
    Option option;
    option.put(PREF_SITE_CONCURRENT, "a.test:1");
    option.put(PREF_SITE_CONCURRENT_DEFAULT, "2");
    try {
      loadSiteConcurrencyFile(option, path_);
      CPPUNIT_FAIL(std::string("exception must be thrown: ") + b.first);
    }
    catch (Exception& e) {
      auto trace = e.stackTrace();
      CPPUNIT_ASSERT_MESSAGE(trace, contains(trace, path_));
      CPPUNIT_ASSERT_MESSAGE(trace, contains(trace, b.second));
    }
    // An invalid file changes no value, not even the valid lines.
    CPPUNIT_ASSERT_EQUAL(std::string("a.test:1"),
                         option.get(PREF_SITE_CONCURRENT));
    CPPUNIT_ASSERT_EQUAL(std::string("2"),
                         option.get(PREF_SITE_CONCURRENT_DEFAULT));
    CPPUNIT_ASSERT(!option.defined(PREF_DIR));
  }

  // Not a regular file.
  auto dirPath = dir_ + "/dir.conf";
  CPPUNIT_ASSERT(File(dirPath).mkdirs());
  Option option;
  try {
    loadSiteConcurrencyFile(option, dirPath);
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    auto trace = e.stackTrace();
    CPPUNIT_ASSERT_MESSAGE(trace, contains(trace, dirPath));
    CPPUNIT_ASSERT_MESSAGE(trace, contains(trace, "not a regular file"));
  }
  File(dirPath).remove();
}

#ifndef __MINGW32__
namespace {
// stat of either link fails with ELOOP, which running as root does not
// bypass.
std::string makeSymlinkLoop(const std::string& dir)
{
  auto a = dir + "/loop-a.conf";
  auto b = dir + "/loop-b.conf";
  // Leftovers of earlier runs. File::remove() cannot remove them: it
  // stats the link target.
  unlink(a.c_str());
  unlink(b.c_str());
  CPPUNIT_ASSERT_EQUAL(0, symlink("loop-b.conf", a.c_str()));
  CPPUNIT_ASSERT_EQUAL(0, symlink("loop-a.conf", b.c_str()));
  return a;
}
} // namespace

void SiteConcurrencyFileTest::testLoad_statFails()
{
  auto loopPath = makeSymlinkLoop(dir_);
  Option option;
  option.put(PREF_SITE_CONCURRENT, "a.test:1");
  option.put(PREF_SITE_CONCURRENT_DEFAULT, "2");
  try {
    loadSiteConcurrencyFile(option, loopPath);
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    auto trace = e.stackTrace();
    CPPUNIT_ASSERT_MESSAGE(trace, contains(trace, loopPath));
    CPPUNIT_ASSERT_MESSAGE(trace,
                           contains(trace, util::safeStrerror(ELOOP)));
  }
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1"),
                       option.get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("2"),
                       option.get(PREF_SITE_CONCURRENT_DEFAULT));
}
#endif // !__MINGW32__

void SiteConcurrencyFileTest::testChangeGlobalOption_persists()
{
  parse(*option_, PREF_SITE_CONCURRENT_DEFAULT, "2");
  parse(*option_, PREF_SITE_CONCURRENT_FILE, path_);
  writeFile(path_, PREVIOUS);

  e_->getRequestGroupMan()->clearQueueCheck();
  auto res = changeGlobalOption(
      e_.get(), {{PREF_SITE_CONCURRENT->k, "a.test:1,b.test:3"}});
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1,b.test:3"),
                       option_->get(PREF_SITE_CONCURRENT));
  // The file holds the new list and the running default.
  CPPUNIT_ASSERT_EQUAL(std::string("site-concurrent=a.test:1,b.test:3\n"
                                   "site-concurrent-default=2\n"),
                       readFile(path_));
  CPPUNIT_ASSERT(e_->getRequestGroupMan()->queueCheckRequested());

  res = changeGlobalOption(e_.get(), {{PREF_SITE_CONCURRENT_DEFAULT->k, "4"}});
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(std::string("site-concurrent=a.test:1,b.test:3\n"
                                   "site-concurrent-default=4\n"),
                       readFile(path_));
  CPPUNIT_ASSERT(!File(path_ + "__temp").exists());

  // A call without a site key leaves the file alone.
  File(path_).remove();
  res = changeGlobalOption(e_.get(),
                           {{PREF_MAX_OVERALL_DOWNLOAD_LIMIT->k, "100K"}});
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT(!File(path_).exists());
}

void SiteConcurrencyFileTest::testChangeGlobalOption_noFile()
{
  CPPUNIT_ASSERT(option_->blank(PREF_SITE_CONCURRENT_FILE));
  e_->getRequestGroupMan()->clearQueueCheck();
  auto res = changeGlobalOption(e_.get(),
                                {{PREF_SITE_CONCURRENT->k, "a.test:1"},
                                 {PREF_SITE_CONCURRENT_DEFAULT->k, "2"}});
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:1"),
                       option_->get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("2"),
                       option_->get(PREF_SITE_CONCURRENT_DEFAULT));
  CPPUNIT_ASSERT(e_->getRequestGroupMan()->queueCheckRequested());
  // Nothing is written: an empty path would give the temporary file
  // "__temp" in the working directory.
  CPPUNIT_ASSERT(!File("__temp").exists());
  CPPUNIT_ASSERT(!File(path_).exists());
}

void SiteConcurrencyFileTest::testChangeGlobalOption_badValueNotPersisted()
{
  parse(*option_, PREF_SITE_CONCURRENT, "old.test:1");
  parse(*option_, PREF_SITE_CONCURRENT_DEFAULT, "1");
  parse(*option_, PREF_SITE_CONCURRENT_FILE, path_);
  writeFile(path_, PREVIOUS);
  auto res = changeGlobalOption(e_.get(),
                                {{PREF_SITE_CONCURRENT->k, "a.test:1,a.test:2"},
                                 {PREF_SITE_CONCURRENT_DEFAULT->k, "5"}});
  CPPUNIT_ASSERT_EQUAL(1, res.code);
  CPPUNIT_ASSERT_EQUAL(std::string(PREVIOUS), readFile(path_));
  CPPUNIT_ASSERT_EQUAL(std::string("old.test:1"),
                       option_->get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("1"),
                       option_->get(PREF_SITE_CONCURRENT_DEFAULT));
}

void SiteConcurrencyFileTest::testChangeGlobalOption_openFails()
{
  parse(*option_, PREF_SITE_CONCURRENT, "old.test:1");
  parse(*option_, PREF_SITE_CONCURRENT_DEFAULT, "1");
  auto rgman = e_->getRequestGroupMan().get();
  const std::vector<std::pair<std::string, std::string>> change{
      {PREF_SITE_CONCURRENT->k, "a.test:1"},
      {PREF_SITE_CONCURRENT_DEFAULT->k, "2"},
      {PREF_MAX_OVERALL_DOWNLOAD_LIMIT->k, "100K"}};

  // The temporary file cannot be created: its path is a directory.
  // Running as root does not bypass this.
  parse(*option_, PREF_SITE_CONCURRENT_FILE, path_);
  writeFile(path_, PREVIOUS);
  CPPUNIT_ASSERT(File(path_ + "__temp").mkdirs());
  rgman->clearQueueCheck();
  auto res = changeGlobalOption(e_.get(), change);
  CPPUNIT_ASSERT_EQUAL(1, res.code);
  CPPUNIT_ASSERT_MESSAGE(faultString(res), contains(faultString(res), path_));
  CPPUNIT_ASSERT_MESSAGE(faultString(res),
                         contains(faultString(res), "open"));
  // The previous file and the running values are unchanged, including
  // the other option of the same call.
  CPPUNIT_ASSERT_EQUAL(std::string(PREVIOUS), readFile(path_));
  CPPUNIT_ASSERT_EQUAL(std::string("old.test:1"),
                       option_->get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("1"),
                       option_->get(PREF_SITE_CONCURRENT_DEFAULT));
  CPPUNIT_ASSERT_EQUAL(0, rgman->getMaxOverallDownloadSpeedLimit());
  CPPUNIT_ASSERT(!rgman->queueCheckRequested());
  // What was in the way of the temporary file is not removed.
  CPPUNIT_ASSERT(File(path_ + "__temp").isDir());
  File(path_ + "__temp").remove();

  // The directory of the file does not exist.
  auto missing = dir_ + "/no-such-dir/sites.conf";
  parse(*option_, PREF_SITE_CONCURRENT_FILE, missing);
  res = changeGlobalOption(e_.get(), change);
  CPPUNIT_ASSERT_EQUAL(1, res.code);
  CPPUNIT_ASSERT_MESSAGE(faultString(res),
                         contains(faultString(res), missing));
  CPPUNIT_ASSERT_EQUAL(std::string("old.test:1"),
                       option_->get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("1"),
                       option_->get(PREF_SITE_CONCURRENT_DEFAULT));
  CPPUNIT_ASSERT_EQUAL(0, rgman->getMaxOverallDownloadSpeedLimit());
  CPPUNIT_ASSERT(!File(dir_ + "/no-such-dir").exists());
}

void SiteConcurrencyFileTest::testChangeGlobalOption_renameFails()
{
  parse(*option_, PREF_SITE_CONCURRENT, "old.test:1");
  parse(*option_, PREF_SITE_CONCURRENT_DEFAULT, "1");
  // The file path is a directory, so the temporary file is written but
  // cannot replace it.
  auto dirPath = dir_ + "/dir.conf";
  CPPUNIT_ASSERT(File(dirPath).mkdirs());
  parse(*option_, PREF_SITE_CONCURRENT_FILE, dirPath);
  auto res = changeGlobalOption(e_.get(), {{PREF_SITE_CONCURRENT->k, "a.test:1"}});
  CPPUNIT_ASSERT_EQUAL(1, res.code);
  CPPUNIT_ASSERT_MESSAGE(faultString(res),
                         contains(faultString(res), "rename failed"));
  CPPUNIT_ASSERT_EQUAL(std::string("old.test:1"),
                       option_->get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT(File(dirPath).isDir());
  CPPUNIT_ASSERT(!File(dirPath + "__temp").exists());
  File(dirPath).remove();
}

#ifndef __MINGW32__
namespace {
// While alive, writing data to a regular file fails with EFBIG instead
// of raising SIGXFSZ. The console log is quieted because stdout can be
// a regular file (the test log).
class ZeroFileSizeLimit {
public:
  ZeroFileSizeLimit() : ok_(false)
  {
    LogFactory::setConsoleLogLevel(Logger::A2_ERROR);
    LogFactory::reconfigure();
    handler_ = signal(SIGXFSZ, SIG_IGN);
    if (getrlimit(RLIMIT_FSIZE, &saved_) == 0) {
      struct rlimit lim = saved_;
      lim.rlim_cur = 0;
      ok_ = setrlimit(RLIMIT_FSIZE, &lim) == 0;
    }
  }

  ~ZeroFileSizeLimit()
  {
    if (ok_) {
      setrlimit(RLIMIT_FSIZE, &saved_);
    }
    signal(SIGXFSZ, handler_);
    LogFactory::setConsoleLogLevel(V_DEBUG);
    LogFactory::reconfigure();
  }

  bool ok() const { return ok_; }

private:
  struct rlimit saved_;
  void (*handler_)(int);
  bool ok_;
};
} // namespace

void SiteConcurrencyFileTest::testChangeGlobalOption_flushFails()
{
  parse(*option_, PREF_SITE_CONCURRENT, "old.test:1");
  parse(*option_, PREF_SITE_CONCURRENT_DEFAULT, "1");
  parse(*option_, PREF_SITE_CONCURRENT_FILE, path_);
  writeFile(path_, PREVIOUS);
  rpc::RpcResponse res(0, rpc::RpcResponse::AUTHORIZED, nullptr, nullptr);
  bool limited;
  {
    ZeroFileSizeLimit limit;
    limited = limit.ok();
    if (limited) {
      // The content fits in the stdio buffer, so the first write(2) is
      // made by fflush.
      res = changeGlobalOption(e_.get(),
                               {{PREF_SITE_CONCURRENT->k, "a.test:1"},
                                {PREF_SITE_CONCURRENT_DEFAULT->k, "2"}});
    }
  }
  CPPUNIT_ASSERT(limited);
  CPPUNIT_ASSERT_EQUAL(1, res.code);
  CPPUNIT_ASSERT_MESSAGE(faultString(res),
                         contains(faultString(res), "flush failed"));
  CPPUNIT_ASSERT_EQUAL(std::string(PREVIOUS), readFile(path_));
  CPPUNIT_ASSERT(!File(path_ + "__temp").exists());
  CPPUNIT_ASSERT_EQUAL(std::string("old.test:1"),
                       option_->get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("1"),
                       option_->get(PREF_SITE_CONCURRENT_DEFAULT));
}
#endif // !__MINGW32__

void SiteConcurrencyFileTest::testStartup_restoresPersistedValues()
{
  auto confPath = dir_ + "/aria2.conf";
  writeFile(confPath, "site-concurrent=a.test:1\n"
                      "site-concurrent-default=2\n"
                      "site-concurrent-file=" +
                          path_ + "\n");
  // KeyVals stand in for the command line.
  const KeyVals args{{PREF_CONF_PATH->k, confPath},
                     {PREF_SITE_CONCURRENT->k, "a.test:9"}};
  std::vector<std::string> uris;

  // No file yet: the command line beats the configuration file.
  Option op;
  CPPUNIT_ASSERT_EQUAL(error_code::FINISHED,
                       option_processing(op, false, uris, 0, nullptr, args));
  CPPUNIT_ASSERT_EQUAL(std::string("a.test:9"), op.get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("2"), op.get(PREF_SITE_CONCURRENT_DEFAULT));
  CPPUNIT_ASSERT_EQUAL(path_, op.get(PREF_SITE_CONCURRENT_FILE));

  // A live change is persisted.
  {
    auto e = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
    e->setOption(&op);
    e->setRequestGroupMan(make_unique<RequestGroupMan>(
        std::vector<std::shared_ptr<RequestGroup>>{}, 1, &op));
    auto res = changeGlobalOption(e.get(),
                                  {{PREF_SITE_CONCURRENT->k, "b.test:3"},
                                   {PREF_SITE_CONCURRENT_DEFAULT->k, "4"}});
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }

  // Restart: the persisted values override the configuration file and
  // the command line.
  Option restarted;
  CPPUNIT_ASSERT_EQUAL(
      error_code::FINISHED,
      option_processing(restarted, false, uris, 0, nullptr, args));
  CPPUNIT_ASSERT_EQUAL(std::string("b.test:3"),
                       restarted.get(PREF_SITE_CONCURRENT));
  CPPUNIT_ASSERT_EQUAL(std::string("4"),
                       restarted.get(PREF_SITE_CONCURRENT_DEFAULT));
}

void SiteConcurrencyFileTest::testStartup_invalidFile()
{
  auto confPath = dir_ + "/aria2.conf";
  writeFile(confPath, "site-concurrent-file=" + path_ + "\n");
  writeFile(path_, "site-concurrent=a.test:1,a.test:2\n");
  const KeyVals args{{PREF_CONF_PATH->k, confPath}};
  std::vector<std::string> uris;
  Option op;
  auto rv = option_processing(op, false, uris, 0, nullptr, args);
  CPPUNIT_ASSERT(rv != error_code::FINISHED);
}

#ifndef __MINGW32__
void SiteConcurrencyFileTest::testStartup_statFails()
{
  auto loopPath = makeSymlinkLoop(dir_);
  auto confPath = dir_ + "/aria2.conf";
  writeFile(confPath, "site-concurrent-file=" + loopPath + "\n");
  const KeyVals args{{PREF_CONF_PATH->k, confPath}};
  std::vector<std::string> uris;
  Option op;
  auto rv = option_processing(op, false, uris, 0, nullptr, args);
  CPPUNIT_ASSERT(rv != error_code::FINISHED);
}
#endif // !__MINGW32__

} // namespace aria2
