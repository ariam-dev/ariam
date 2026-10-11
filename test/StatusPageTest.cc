#include "StatusPage.h"

#include <cstdlib>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <cppunit/extensions/HelperMacros.h>

#include <aria2/aria2.h>
#include "HttpServer.h"
#include "HttpServerBodyCommand.h"
#include "HttpServerCommand.h"
#include "DownloadEngine.h"
#include "SelectEventPoll.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "SocketCore.h"
#include "Option.h"
#include "OptionParser.h"
#include "prefs.h"
#include "File.h"
#include "Exception.h"
#include "RecoverableException.h"
#include "error_code.h"
#include "RpcMethodImpl.h"
#include "RpcRequest.h"
#include "RpcResponse.h"
#include "ValueBase.h"
#include "base64.h"
#include "wallclock.h"
#include "a2functional.h"
#include "util.h"
#include "DownloadResult.h"
#include "ValueBaseJsonParser.h"
#include "TestUtil.h"
#ifdef HAVE_ZLIB
#  include "GZipDecoder.h"
#endif // HAVE_ZLIB
#ifdef ENABLE_WEBSOCKET
#  include "WebSocketSessionMan.h"
#endif // ENABLE_WEBSOCKET

namespace aria2 {

error_code::Value option_processing(Option& op, bool standalone,
                                    std::vector<std::string>& uris, int argc,
                                    char** argv, const KeyVals& options);

class StatusPageTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(StatusPageTest);
  CPPUNIT_TEST(testIsPageFileName);
  CPPUNIT_TEST(testMatchPagePath);
  CPPUNIT_TEST(testGetContentType);
  CPPUNIT_TEST(testParseBasicPassword);
  CPPUNIT_TEST(testValidateOptions);
  CPPUNIT_TEST(testOptionProcessing_guards);
  CPPUNIT_TEST(testGetGlobalOption_hidesPassphrase);
  CPPUNIT_TEST(testNoPassphrase_stock404);
  CPPUNIT_TEST(testServer_pageRoute);
  CPPUNIT_TEST(testStaticRoutes);
  CPPUNIT_TEST(testRejectedNames);
  CPPUNIT_TEST(testPageHeaders);
  CPPUNIT_TEST(testUnauthorized);
  CPPUNIT_TEST(testAnyUserName);
#ifdef HAVE_ZLIB
  CPPUNIT_TEST(testContentEncoding);
#endif // HAVE_ZLIB
  CPPUNIT_TEST(testCheckStatusPagePassphrase);
  CPPUNIT_TEST(testRateLimit_pageRoutes);
  CPPUNIT_TEST(testPassphraseRpc_accepted);
  CPPUNIT_TEST(testPassphraseRpc_refusals);
#ifdef ENABLE_WEBSOCKET
  CPPUNIT_TEST(testPassphraseRpc_webSocket);
#endif // ENABLE_WEBSOCKET
  CPPUNIT_TEST(testPassphraseRpc_batch);
  CPPUNIT_TEST(testPassphraseRpc_rateLimit);
  CPPUNIT_TEST(testAcceptsJsonpCallback);
  CPPUNIT_TEST(testJsonpCallback);
  CPPUNIT_TEST_SUITE_END();

  std::string dir_;
  std::string pageDir_;
  std::shared_ptr<Option> option_;
  std::unique_ptr<DownloadEngine> e_;
  std::unique_ptr<SocketCore> listen_;

public:
  void setUp();
  void tearDown();

  void testIsPageFileName();
  void testMatchPagePath();
  void testGetContentType();
  void testParseBasicPassword();
  void testValidateOptions();
  void testOptionProcessing_guards();
  void testGetGlobalOption_hidesPassphrase();
  void testNoPassphrase_stock404();
  void testServer_pageRoute();
  void testStaticRoutes();
  void testRejectedNames();
  void testPageHeaders();
  void testUnauthorized();
  void testAnyUserName();
  void testContentEncoding();
  void testCheckStatusPagePassphrase();
  void testRateLimit_pageRoutes();
  void testPassphraseRpc_accepted();
  void testPassphraseRpc_refusals();
  void testPassphraseRpc_webSocket();
  void testPassphraseRpc_batch();
  void testPassphraseRpc_rateLimit();
  void testAcceptsJsonpCallback();
  void testJsonpCallback();
};

CPPUNIT_TEST_SUITE_REGISTRATION(StatusPageTest);

namespace {
const std::string PASSPHRASE = "open sesame";
const std::string PEER = "127.0.0.1";
const std::string INDEX = "<!doctype html><title>ariam</title>\n";
const std::string PNG("\x89PNG\r\n\x1a\n\0\0", 10);
const std::string ICO("\0\0\1\0", 4);

const char CSP[] = "Content-Security-Policy: default-src 'none'; "
                   "script-src 'self'; style-src 'self'; connect-src 'self'; "
                   "img-src 'self'; base-uri 'none'; form-action 'none'; "
                   "frame-ancestors 'none'";
const char CHALLENGE_LINE[] =
    "WWW-Authenticate: Basic realm=\"ariam\", charset=\"UTF-8\"";

void writeFile(const std::string& path, const std::string& content)
{
  std::ofstream o(path.c_str(), std::ios::binary);
  o << content;
}

std::string basic(const std::string& userpass)
{
  return "Authorization: Basic " +
         base64::encode(std::begin(userpass), std::end(userpass)) + "\r\n";
}

std::string get(const std::string& path, const std::string& headers = "")
{
  return "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n" + headers + "\r\n";
}

std::string authorizedGet(const std::string& path,
                          const std::string& headers = "")
{
  return get(path, basic("user:" + PASSPHRASE) + headers);
}

struct Response {
  int status;
  // The status line and the header fields, each ending with "\r\n".
  std::string headers;
  std::string body;

  bool has(const std::string& line) const
  {
    return headers.find("\r\n" + line + "\r\n") != std::string::npos;
  }

  bool hasField(const std::string& name) const
  {
    return headers.find("\r\n" + name + ":") != std::string::npos;
  }

  bool isPageResponse() const { return has(CSP); }
};

bool parseResponse(const std::string& data, Response& res)
{
  auto end = data.find("\r\n\r\n");
  if (end == std::string::npos) {
    return false;
  }
  res.headers = data.substr(0, end + 2);
  size_t length = 0;
  auto cl = res.headers.find("\r\nContent-Length: ");
  if (cl != std::string::npos) {
    length = strtoul(res.headers.c_str() + cl + 18, nullptr, 10);
  }
  if (data.size() < end + 4 + length) {
    return false;
  }
  res.body = data.substr(end + 4, length);
  res.status = atoi(res.headers.c_str() + 9);
  return true;
}

// pump, if given, runs before each read.
Response readResponse(SocketCore& client, const std::function<void()>& pump)
{
  std::string data;
  Response res;
  for (int i = 0; i < 20; ++i) {
    if (pump) {
      pump();
    }
    if (client.isReadable(pump ? 0 : 1)) {
      char buf[16_k];
      size_t len = sizeof(buf);
      client.readData(buf, len);
      data.append(buf, len);
    }
    if (parseResponse(data, res)) {
      return res;
    }
  }
  CPPUNIT_FAIL("Incomplete response: " + data);
  return res;
}
} // namespace

void StatusPageTest::setUp()
{
  dir_ = A2_TEST_OUT_DIR "/aria2_StatusPageTest";
  pageDir_ = dir_ + "/www";
  File(pageDir_ + "/sub").mkdirs();
  writeFile(pageDir_ + "/index.html", INDEX);
  writeFile(pageDir_ + "/app.js", "export {};\n");
  writeFile(pageDir_ + "/style.css", "body{}\n");
  writeFile(pageDir_ + "/icon.svg", "<svg/>");
  writeFile(pageDir_ + "/logo.png", PNG);
  writeFile(pageDir_ + "/favicon.ico", ICO);
  writeFile(pageDir_ + "/notes.txt", "notes");
  writeFile(pageDir_ + "/sub/index.html", "sub");
  writeFile(dir_ + "/outside.html", "outside");
  File(pageDir_ + "/missing.html").remove();

  option_ = std::make_shared<Option>();
  OptionParser::getInstance()->parseDefaultValues(*option_);
  option_->put(PREF_ENABLE_RPC, A2_V_TRUE);
  option_->put(PREF_RPC_SECURE, A2_V_TRUE);
  option_->put(PREF_RPC_SECRET, "secret");
  option_->put(PREF_STATUS_PAGE_PASSPHRASE, PASSPHRASE);
  option_->put(PREF_STATUS_PAGE_DIR, pageDir_);
  e_ = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
  e_->setOption(option_.get());
  e_->setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get()));

  listen_ = make_unique<SocketCore>();
  listen_->bind(0);
  listen_->beginListen();
  listen_->setBlockingMode();

  global::wallclock().reset();
}

void StatusPageTest::tearDown()
{
  e_.reset();
  listen_.reset();
  global::wallclock().reset();
}

namespace {
// Answers request with HttpServerBodyCommand on a new HttpServer,
// without HttpServerCommand: statusPage stands in for
// PREF_STATUS_PAGE_PASSPHRASE. The engine loop does not run, so
// global::wallclock() stays where the test puts it.
Response pageRequest(SocketCore& listen, DownloadEngine* e,
                     const std::string& request, bool statusPage = true)
{
  auto endpoint = listen.getAddrInfo();
  SocketCore client;
  client.establishConnection("localhost", endpoint.port);
  while (!client.isWritable(0)) {
  }
  std::shared_ptr<SocketCore> inbound = listen.acceptConnection();
  inbound->setBlockingMode();
  auto httpServer = std::make_shared<HttpServer>(inbound);
  if (statusPage) {
    httpServer->enableStatusPage();
  }
#ifdef HAVE_ZLIB
  httpServer->enableGZip();
#endif // HAVE_ZLIB
  client.writeData(request);
  while (!httpServer->receiveRequest()) {
  }
  auto command = make_unique<HttpServerBodyCommand>(e->newCUID(), httpServer,
                                                    e, inbound);
  if (!command->execute()) {
    // The command queued itself in e, which owns it now.
    command.release();
    CPPUNIT_FAIL("The request was not answered");
  }
  command.reset();
  while (!httpServer->sendBufferIsEmpty()) {
    httpServer->sendResponse();
  }
  return readResponse(client, nullptr);
}

// Sends request on a connection served by HttpServerCommand in the
// engine loop of e, the path every RPC port connection takes.
Response serverRequest(SocketCore& listen, DownloadEngine* e,
                       const std::string& request)
{
  auto endpoint = listen.getAddrInfo();
  SocketCore client;
  client.establishConnection("localhost", endpoint.port);
  while (!client.isWritable(0)) {
  }
  std::shared_ptr<SocketCore> inbound = listen.acceptConnection();
  e->addCommand(
      make_unique<HttpServerCommand>(e->newCUID(), e, inbound, false));
  client.writeData(request);
  return readResponse(client, [e]() { e->run(true); });
}
} // namespace

void StatusPageTest::testIsPageFileName()
{
  for (auto name : {"index.html", "app.js", "style.css", "icon.svg",
                    "logo.png", "favicon.ico", "queue-view.js", "a_b.c.css",
                    "0.html"}) {
    CPPUNIT_ASSERT_MESSAGE(name, status_page::isPageFileName(name));
  }
  for (auto name :
       {"", ".html", "..html", "a..html", "a..b.js", "-a.js", "_a.js",
        "Index.html", "index.HTML", "index.txt", "index", "index.", "a/b.js",
        "%2e%2e.html", "a%2fb.js", "a b.js", "index.html.bak", "index.htm",
        "a.js\n", "a.jsx"}) {
    CPPUNIT_ASSERT_MESSAGE(name, !status_page::isPageFileName(name));
  }
}

void StatusPageTest::testMatchPagePath()
{
  std::string fileName;
  CPPUNIT_ASSERT(status_page::matchPagePath("/", fileName));
  CPPUNIT_ASSERT_EQUAL(std::string("index.html"), fileName);
  CPPUNIT_ASSERT(status_page::matchPagePath("/app.js", fileName));
  CPPUNIT_ASSERT_EQUAL(std::string("app.js"), fileName);
  for (auto path :
       {"", "//", "app.js", "/../index.html", "/%2e%2e/index.html",
        "/%2E%2E/index.html", "/..%2findex.html", "/sub/index.html",
        "//index.html", "/index.html/", "/Index.html", "/index.txt",
        "/jsonrpc", "/rpc", "/.", "/.."}) {
    fileName = "unchanged";
    CPPUNIT_ASSERT_MESSAGE(path, !status_page::matchPagePath(path, fileName));
    CPPUNIT_ASSERT_EQUAL(std::string("unchanged"), fileName);
  }
}

void StatusPageTest::testGetContentType()
{
  CPPUNIT_ASSERT_EQUAL(std::string("text/html; charset=utf-8"),
                       std::string(status_page::getContentType("index.html")));
  CPPUNIT_ASSERT_EQUAL(std::string("text/javascript"),
                       std::string(status_page::getContentType("app.js")));
  CPPUNIT_ASSERT_EQUAL(std::string("text/css"),
                       std::string(status_page::getContentType("style.css")));
  CPPUNIT_ASSERT_EQUAL(std::string("image/svg+xml"),
                       std::string(status_page::getContentType("icon.svg")));
  CPPUNIT_ASSERT_EQUAL(std::string("image/png"),
                       std::string(status_page::getContentType("logo.png")));
  CPPUNIT_ASSERT_EQUAL(std::string("image/x-icon"),
                       std::string(status_page::getContentType("favicon.ico")));
}

void StatusPageTest::testParseBasicPassword()
{
  std::string password = "unchanged";
  CPPUNIT_ASSERT(!status_page::parseBasicPassword("", password));
  CPPUNIT_ASSERT(!status_page::parseBasicPassword("Bearer abc", password));
  CPPUNIT_ASSERT(
      !status_page::parseBasicPassword("Digest username=\"u\"", password));
  CPPUNIT_ASSERT_EQUAL(std::string("unchanged"), password);

  // "user:pass"
  CPPUNIT_ASSERT(status_page::parseBasicPassword("Basic dXNlcjpwYXNz", password));
  CPPUNIT_ASSERT_EQUAL(std::string("pass"), password);
  // The scheme is case-insensitive (RFC 7617).
  CPPUNIT_ASSERT(status_page::parseBasicPassword("basic dXNlcjpwYXNz", password));
  CPPUNIT_ASSERT_EQUAL(std::string("pass"), password);
  // ":pass" (empty user name)
  CPPUNIT_ASSERT(status_page::parseBasicPassword("Basic OnBhc3M=", password));
  CPPUNIT_ASSERT_EQUAL(std::string("pass"), password);
  // "user:pa:ss" (the password may hold ':')
  CPPUNIT_ASSERT(
      status_page::parseBasicPassword("Basic dXNlcjpwYTpzcw==", password));
  CPPUNIT_ASSERT_EQUAL(std::string("pa:ss"), password);
  // "user" (no ':'): Basic credentials with an empty password.
  CPPUNIT_ASSERT(status_page::parseBasicPassword("Basic dXNlcg==", password));
  CPPUNIT_ASSERT_EQUAL(std::string(""), password);
  // Not base64: Basic credentials with an empty password.
  password = "unchanged";
  CPPUNIT_ASSERT(status_page::parseBasicPassword("Basic !!!", password));
  CPPUNIT_ASSERT_EQUAL(std::string(""), password);
}

void StatusPageTest::testValidateOptions()
{
  auto good = []() {
    Option op;
    op.put(PREF_STATUS_PAGE_PASSPHRASE, PASSPHRASE);
    op.put(PREF_RPC_SECURE, A2_V_TRUE);
    op.put(PREF_RPC_ALLOW_ORIGIN_ALL, A2_V_FALSE);
    op.put(PREF_RPC_SECRET, "secret");
    return op;
  };
  auto rejects = [](const Option& op, const std::string& name) {
    try {
      status_page::validateOptions(op);
      CPPUNIT_FAIL("exception must be thrown");
    }
    catch (RecoverableException& e) {
      CPPUNIT_ASSERT_MESSAGE(e.what(),
                             std::string(e.what()).find(name) !=
                                 std::string::npos);
    }
  };

  status_page::validateOptions(good());
  {
    auto op = good();
    op.put(PREF_RPC_SECURE, A2_V_FALSE);
    rejects(op, "--rpc-secure");
  }
  {
    auto op = good();
    op.remove(PREF_RPC_SECURE);
    rejects(op, "--rpc-secure");
  }
  {
    auto op = good();
    op.put(PREF_RPC_ALLOW_ORIGIN_ALL, A2_V_TRUE);
    rejects(op, "--rpc-allow-origin-all");
  }
  {
    auto op = good();
    op.remove(PREF_RPC_SECRET);
    rejects(op, "--rpc-secret");
  }
  {
    auto op = good();
    op.put(PREF_RPC_USER, "user");
    rejects(op, "--rpc-user");
  }
  {
    // Without a passphrase nothing is checked.
    Option op;
    op.put(PREF_RPC_ALLOW_ORIGIN_ALL, A2_V_TRUE);
    op.put(PREF_RPC_USER, "user");
    status_page::validateOptions(op);
  }
}

void StatusPageTest::testOptionProcessing_guards()
{
  const KeyVals good{{PREF_NO_CONF->k, A2_V_TRUE},
                     {PREF_STATUS_PAGE_PASSPHRASE->k, PASSPHRASE},
                     {PREF_RPC_SECURE->k, A2_V_TRUE},
                     {PREF_RPC_SECRET->k, "secret"}};
  std::vector<std::string> uris;
  {
    Option op;
    CPPUNIT_ASSERT_EQUAL(error_code::FINISHED,
                         option_processing(op, false, uris, 0, nullptr, good));
    CPPUNIT_ASSERT_EQUAL(PASSPHRASE, op.get(PREF_STATUS_PAGE_PASSPHRASE));
    CPPUNIT_ASSERT_EQUAL(std::string(STATUS_PAGE_DIR),
                         op.get(PREF_STATUS_PAGE_DIR));
  }
  const KeyVals conflicts[] = {
      {{PREF_RPC_SECURE->k, A2_V_FALSE}},
      {{PREF_RPC_ALLOW_ORIGIN_ALL->k, A2_V_TRUE}},
      {{PREF_RPC_USER->k, "user"}},
  };
  for (const auto& conflict : conflicts) {
    auto args = good;
    args.insert(std::end(args), std::begin(conflict), std::end(conflict));
    Option op;
    CPPUNIT_ASSERT(option_processing(op, false, uris, 0, nullptr, args) !=
                   error_code::FINISHED);
  }
  {
    // rpc-secret unset.
    KeyVals args;
    for (const auto& kv : good) {
      if (kv.first != PREF_RPC_SECRET->k) {
        args.push_back(kv);
      }
    }
    Option op;
    CPPUNIT_ASSERT(option_processing(op, false, uris, 0, nullptr, args) !=
                   error_code::FINISHED);
  }
}

void StatusPageTest::testGetGlobalOption_hidesPassphrase()
{
  rpc::GetGlobalOptionRpcMethod m;
  rpc::RpcRequest req(rpc::GetGlobalOptionRpcMethod::getMethodName(),
                      List::g());
  req.params->append("token:secret");
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const Dict* result = downcast<Dict>(res.param);
  CPPUNIT_ASSERT(result);
  CPPUNIT_ASSERT(!result->get(PREF_STATUS_PAGE_PASSPHRASE->k));
  CPPUNIT_ASSERT(!result->get(PREF_RPC_SECRET->k));
  const String* dir = downcast<String>(result->get(PREF_STATUS_PAGE_DIR->k));
  CPPUNIT_ASSERT(dir);
  CPPUNIT_ASSERT_EQUAL(pageDir_, dir->s());
}

void StatusPageTest::testNoPassphrase_stock404()
{
  option_->remove(PREF_STATUS_PAGE_PASSPHRASE);
  for (auto path : {"/", "/index.html", "/app.js"}) {
    for (const auto& request : {get(path), authorizedGet(path)}) {
      auto res = serverRequest(*listen_, e_.get(), request);
      CPPUNIT_ASSERT_EQUAL_MESSAGE(path, 404, res.status);
      CPPUNIT_ASSERT(!res.isPageResponse());
      CPPUNIT_ASSERT(!res.hasField("WWW-Authenticate"));
      CPPUNIT_ASSERT(res.has("Cache-Control: no-cache"));
      CPPUNIT_ASSERT(res.body.empty());
    }
  }
  CPPUNIT_ASSERT_EQUAL((size_t)0, e_->getPassphraseLimiter().size());
}

void StatusPageTest::testServer_pageRoute()
{
  {
    auto res = serverRequest(*listen_, e_.get(), get("/"));
    CPPUNIT_ASSERT_EQUAL(401, res.status);
    CPPUNIT_ASSERT(res.has(CHALLENGE_LINE));
    CPPUNIT_ASSERT(res.isPageResponse());
  }
  {
    auto res = serverRequest(*listen_, e_.get(), authorizedGet("/"));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    CPPUNIT_ASSERT_EQUAL(INDEX, res.body);
  }
  {
    // Other paths keep the stock behavior.
    auto res = serverRequest(*listen_, e_.get(), authorizedGet("/sub/index.html"));
    CPPUNIT_ASSERT_EQUAL(404, res.status);
    CPPUNIT_ASSERT(!res.isPageResponse());
  }
}

void StatusPageTest::testStaticRoutes()
{
  struct {
    const char* path;
    const char* contentType;
    std::string body;
  } routes[] = {
      {"/", "text/html; charset=utf-8", INDEX},
      {"/?v=1", "text/html; charset=utf-8", INDEX},
      {"/index.html", "text/html; charset=utf-8", INDEX},
      {"/app.js", "text/javascript", "export {};\n"},
      {"/app.js?v=2#x", "text/javascript", "export {};\n"},
      {"/style.css", "text/css", "body{}\n"},
      {"/icon.svg", "image/svg+xml", "<svg/>"},
      {"/logo.png", "image/png", PNG},
      {"/favicon.ico", "image/x-icon", ICO},
  };
  for (const auto& route : routes) {
    auto res = pageRequest(*listen_, e_.get(), authorizedGet(route.path));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(route.path, 200, res.status);
    CPPUNIT_ASSERT_MESSAGE(route.path, res.has(std::string("Content-Type: ") +
                                               route.contentType));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(route.path, route.body, res.body);
    CPPUNIT_ASSERT(!res.hasField("Content-Encoding"));
  }
  {
    auto res = pageRequest(*listen_, e_.get(), authorizedGet("/missing.html"));
    CPPUNIT_ASSERT_EQUAL(404, res.status);
    CPPUNIT_ASSERT(res.isPageResponse());
  }
}

void StatusPageTest::testRejectedNames()
{
  // Each gets the stock 404 with no page headers and no challenge,
  // whatever the credentials, before any passphrase check.
  for (auto path :
       {"/../outside.html", "/%2e%2e/outside.html", "/%2E%2E/outside.html",
        "/..%2foutside.html", "/%2e%2e%2foutside.html", "/sub/index.html",
        "//index.html", "/index.html/", "/Index.html", "/INDEX.HTML",
        "/index.HTML", "/notes.txt", "/app.mjs", "/index.htm", "/a..b.html",
        "/.index.html", "/-x.js", "/%69ndex.html", "/index.html%00"}) {
    for (const auto& request :
         {get(path), authorizedGet(path), get(path, basic("u:wrong"))}) {
      auto res = pageRequest(*listen_, e_.get(), request);
      CPPUNIT_ASSERT_EQUAL_MESSAGE(path, 404, res.status);
      CPPUNIT_ASSERT_MESSAGE(path, !res.isPageResponse());
      CPPUNIT_ASSERT_MESSAGE(path, !res.hasField("WWW-Authenticate"));
      CPPUNIT_ASSERT(res.body.empty());
    }
  }
  for (const auto& userpass : {"u:" + PASSPHRASE, std::string("u:wrong")}) {
    for (auto request :
         {"HEAD / HTTP/1.1\r\n" + basic(userpass) + "\r\n",
          "POST / HTTP/1.1\r\nContent-Length: 0\r\n" + basic(userpass) +
              "\r\n"}) {
      auto res = pageRequest(*listen_, e_.get(), request);
      CPPUNIT_ASSERT_EQUAL(404, res.status);
      CPPUNIT_ASSERT(!res.isPageResponse());
    }
  }
  // The wrong passphrases were never checked: nothing was counted.
  CPPUNIT_ASSERT_EQUAL((size_t)0, e_->getPassphraseLimiter().size());
}

void StatusPageTest::testPageHeaders()
{
  auto check = [](const Response& res) {
    CPPUNIT_ASSERT(res.has(CSP));
    CPPUNIT_ASSERT(res.has("X-Content-Type-Options: nosniff"));
    CPPUNIT_ASSERT(res.has("Referrer-Policy: no-referrer"));
    CPPUNIT_ASSERT(res.has("Cache-Control: no-store"));
    CPPUNIT_ASSERT(!res.has("Cache-Control: no-cache"));
    CPPUNIT_ASSERT(!res.hasField("Access-Control-Allow-Origin"));
  };
  {
    auto res = pageRequest(*listen_, e_.get(), authorizedGet("/"));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    check(res);
  }
  {
    auto res = pageRequest(*listen_, e_.get(), get("/app.js"));
    CPPUNIT_ASSERT_EQUAL(401, res.status);
    check(res);
  }
  {
    auto res = pageRequest(*listen_, e_.get(), authorizedGet("/missing.html"));
    CPPUNIT_ASSERT_EQUAL(404, res.status);
    check(res);
  }
  for (int i = 0; i < PassphraseLimiter::LOCKOUT_THRESHOLD; ++i) {
    pageRequest(*listen_, e_.get(), get("/", basic("u:wrong")));
  }
  {
    auto res = pageRequest(*listen_, e_.get(), authorizedGet("/"));
    CPPUNIT_ASSERT_EQUAL(429, res.status);
    CPPUNIT_ASSERT(res.headers.find("HTTP/1.1 429 Too Many Requests\r\n") ==
                   0);
    check(res);
  }
}

void StatusPageTest::testUnauthorized()
{
  for (const auto& request :
       {get("/"),
        get("/", basic("user:wrong")),
        get("/", basic("user:" + PASSPHRASE + " ")),
        get("/", basic("user:" + PASSPHRASE.substr(1))),
        get("/", basic(PASSPHRASE)),
        get("/", "Authorization: Bearer " + PASSPHRASE + "\r\n"),
        get("/missing.html"),
        get("/missing.html", basic("user:wrong"))}) {
    auto res = pageRequest(*listen_, e_.get(), request);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(request, 401, res.status);
    CPPUNIT_ASSERT_MESSAGE(request, res.has(CHALLENGE_LINE));
    CPPUNIT_ASSERT(res.body.empty());
  }
  // Missing or non-Basic credentials are not counted; the 5 wrong
  // passphrases are (the last one starts a lockout).
  CPPUNIT_ASSERT_EQUAL(
      5, e_->getPassphraseLimiter().countFailures(PEER, global::wallclock()));
}

void StatusPageTest::testAnyUserName()
{
  for (const auto& userpass :
       {"user:" + PASSPHRASE, ":" + PASSPHRASE, "admin:" + PASSPHRASE,
        std::string("\xc3\xa9:") + PASSPHRASE}) {
    auto res = pageRequest(*listen_, e_.get(), get("/", basic(userpass)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(userpass, 200, res.status);
    CPPUNIT_ASSERT_EQUAL(INDEX, res.body);
  }
  {
    // A passphrase holding ':' and non-ASCII bytes (charset="UTF-8").
    const std::string passphrase = "p\xc3\xa4ss:w\xc3\xb6rd";
    option_->put(PREF_STATUS_PAGE_PASSPHRASE, passphrase);
    auto e = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
    e->setOption(option_.get());
    e->setRequestGroupMan(make_unique<RequestGroupMan>(
        std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get()));
    auto res =
        pageRequest(*listen_, e.get(), get("/", basic("u:" + passphrase)));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
  }
}

#ifdef HAVE_ZLIB
void StatusPageTest::testContentEncoding()
{
  {
    auto res = pageRequest(*listen_, e_.get(),
                           authorizedGet("/", "Accept-Encoding: gzip\r\n"));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    CPPUNIT_ASSERT(res.has("Content-Encoding: gzip"));
    GZipDecoder decoder;
    decoder.init();
    auto body = decoder.decode(
        reinterpret_cast<const unsigned char*>(res.body.data()),
        res.body.size());
    CPPUNIT_ASSERT(decoder.finished());
    CPPUNIT_ASSERT_EQUAL(INDEX, body);
  }
  {
    auto res = pageRequest(*listen_, e_.get(),
                           authorizedGet("/logo.png", "Accept-Encoding: gzip\r\n"));
    CPPUNIT_ASSERT(res.has("Content-Encoding: gzip"));
    GZipDecoder decoder;
    decoder.init();
    CPPUNIT_ASSERT_EQUAL(
        PNG, decoder.decode(
                 reinterpret_cast<const unsigned char*>(res.body.data()),
                 res.body.size()));
  }
  {
    // No body, no Content-Encoding.
    auto res =
        pageRequest(*listen_, e_.get(), get("/", "Accept-Encoding: gzip\r\n"));
    CPPUNIT_ASSERT_EQUAL(401, res.status);
    CPPUNIT_ASSERT(!res.hasField("Content-Encoding"));
    CPPUNIT_ASSERT(res.has("Content-Length: 0"));
  }
  {
    auto res = pageRequest(*listen_, e_.get(), authorizedGet("/"));
    CPPUNIT_ASSERT(!res.hasField("Content-Encoding"));
    CPPUNIT_ASSERT_EQUAL(INDEX, res.body);
  }
}
#endif // HAVE_ZLIB

void StatusPageTest::testCheckStatusPagePassphrase()
{
  const auto& limiter = e_->getPassphraseLimiter();
  auto& now = global::wallclock();
  for (int i = 0; i < 4; ++i) {
    CPPUNIT_ASSERT_EQUAL(PASSPHRASE_WRONG,
                         e_->checkStatusPagePassphrase(PEER, "wrong"));
    CPPUNIT_ASSERT(!limiter.locked(PEER, now));
  }
  CPPUNIT_ASSERT_EQUAL(PASSPHRASE_WRONG,
                       e_->checkStatusPagePassphrase(PEER, "wrong"));
  CPPUNIT_ASSERT(limiter.locked(PEER, now));
  // Locked: the right passphrase is not checked, and nothing is counted.
  CPPUNIT_ASSERT_EQUAL(PASSPHRASE_LOCKED,
                       e_->checkStatusPagePassphrase(PEER, PASSPHRASE));
  CPPUNIT_ASSERT_EQUAL(PASSPHRASE_LOCKED,
                       e_->checkStatusPagePassphrase(PEER, "wrong"));
  CPPUNIT_ASSERT_EQUAL(5, limiter.countFailures(PEER, now));
  // Another client is not locked.
  CPPUNIT_ASSERT_EQUAL(PASSPHRASE_OK,
                       e_->checkStatusPagePassphrase("192.0.2.1", PASSPHRASE));
  // The 1 s lockout ends; the 6th failure locks for 2 s.
  now.advance(1_s);
  CPPUNIT_ASSERT_EQUAL(PASSPHRASE_WRONG,
                       e_->checkStatusPagePassphrase(PEER, "wrong"));
  now.advance(1_s);
  CPPUNIT_ASSERT_EQUAL(PASSPHRASE_LOCKED,
                       e_->checkStatusPagePassphrase(PEER, PASSPHRASE));
  now.advance(1_s);
  // A success resets the count.
  CPPUNIT_ASSERT_EQUAL(PASSPHRASE_OK,
                       e_->checkStatusPagePassphrase(PEER, PASSPHRASE));
  CPPUNIT_ASSERT_EQUAL(0, limiter.countFailures(PEER, now));
  CPPUNIT_ASSERT_EQUAL((size_t)0, limiter.size());

  // Without a passphrase nothing matches and nothing is counted.
  option_->remove(PREF_STATUS_PAGE_PASSPHRASE);
  CPPUNIT_ASSERT_EQUAL(PASSPHRASE_WRONG,
                       e_->checkStatusPagePassphrase(PEER, ""));
  CPPUNIT_ASSERT_EQUAL((size_t)0, limiter.size());
}

void StatusPageTest::testRateLimit_pageRoutes()
{
  const auto& limiter = e_->getPassphraseLimiter();
  auto& now = global::wallclock();
  // Credential-less requests, which a browser sends before it has
  // credentials, are never counted.
  for (int i = 0; i < 10; ++i) {
    auto res = pageRequest(*listen_, e_.get(), get("/"));
    CPPUNIT_ASSERT_EQUAL(401, res.status);
  }
  CPPUNIT_ASSERT_EQUAL((size_t)0, limiter.size());
  for (int i = 0; i < 5; ++i) {
    auto res = pageRequest(*listen_, e_.get(), get("/", basic("u:wrong")));
    CPPUNIT_ASSERT_EQUAL(401, res.status);
    CPPUNIT_ASSERT(res.has(CHALLENGE_LINE));
  }
  {
    // Locked out: 429 without checking the right passphrase.
    auto res = pageRequest(*listen_, e_.get(), authorizedGet("/app.js"));
    CPPUNIT_ASSERT_EQUAL(429, res.status);
    CPPUNIT_ASSERT(!res.hasField("WWW-Authenticate"));
    CPPUNIT_ASSERT(res.body.empty());
  }
  now.advance(999_ms);
  CPPUNIT_ASSERT_EQUAL(
      429, pageRequest(*listen_, e_.get(), authorizedGet("/")).status);
  CPPUNIT_ASSERT_EQUAL(5, limiter.countFailures(PEER, now));
  now.advance(1_ms);
  {
    auto res = pageRequest(*listen_, e_.get(), authorizedGet("/"));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    CPPUNIT_ASSERT_EQUAL(INDEX, res.body);
  }
  // The success reset the count: 4 more failures do not lock.
  for (int i = 0; i < 4; ++i) {
    pageRequest(*listen_, e_.get(), get("/", basic("u:wrong")));
  }
  CPPUNIT_ASSERT_EQUAL(
      200, pageRequest(*listen_, e_.get(), authorizedGet("/")).status);
}

namespace {
const char UNAUTHORIZED[] =
    "\"error\":{\"code\":1,\"message\":\"Unauthorized\"}";

// The header fields the status page sends with each RPC request,
// besides Host and its credentials.
const char PAGE_RPC_HEADERS[] = "Content-Type: application/json\r\n"
                                "X-Ariam-Page: 1\r\n"
                                "Origin: https://localhost\r\n"
                                "Sec-Fetch-Site: same-origin\r\n";

std::string post(const std::string& body, const std::string& headers)
{
  return "POST /jsonrpc HTTP/1.1\r\nHost: localhost\r\nContent-Length: " +
         util::uitos(body.size()) + "\r\n" + headers + "\r\n" + body;
}

// A status page RPC request; credentials is its Authorization field.
std::string pagePost(const std::string& body,
                     const std::string& credentials = basic("user:" +
                                                            PASSPHRASE))
{
  return post(body, PAGE_RPC_HEADERS + credentials);
}

std::string rpcCall(const std::string& method,
                    const std::string& params = "[]", int id = 1)
{
  return "{\"jsonrpc\":\"2.0\",\"id\":" + util::itos(id) + ",\"method\":\"" +
         method + "\",\"params\":" + params + "}";
}

bool contains(const std::string& s, const std::string& part)
{
  return s.find(part) != std::string::npos;
}

std::unique_ptr<ValueBase> parseJson(const std::string& s)
{
  ssize_t error = 0;
  auto v = json::ValueBaseJsonParser().parseFinal(s.c_str(), s.size(), error);
  CPPUNIT_ASSERT_MESSAGE(s, error >= 0 && v);
  return v;
}

const Dict* member(const ValueBase* batch, size_t i, size_t size)
{
  auto list = downcast<List>(batch);
  CPPUNIT_ASSERT(list);
  CPPUNIT_ASSERT_EQUAL(size, list->size());
  auto res = downcast<Dict>(list->get(i));
  CPPUNIT_ASSERT(res);
  return res;
}

bool isUnauthorized(const ValueBase* response)
{
  auto res = downcast<Dict>(response);
  auto error = res ? downcast<Dict>(res->get("error")) : nullptr;
  auto message = error ? downcast<String>(error->get("message")) : nullptr;
  return message && message->s() == "Unauthorized";
}

const Dict* onlyEntry(const Dict* response)
{
  auto result = downcast<List>(response->get("result"));
  CPPUNIT_ASSERT(result);
  CPPUNIT_ASSERT_EQUAL((size_t)1, result->size());
  auto entry = downcast<Dict>(result->get(0));
  CPPUNIT_ASSERT(entry);
  return entry;
}

bool hasUris(const Dict* entry)
{
  auto files = downcast<List>(entry->get("files"));
  CPPUNIT_ASSERT(files);
  auto file = downcast<Dict>(files->get(0));
  CPPUNIT_ASSERT(file);
  CPPUNIT_ASSERT(file->containsKey("path"));
  return file->containsKey("uris");
}
} // namespace

void StatusPageTest::testPassphraseRpc_accepted()
{
  {
    auto res =
        pageRequest(*listen_, e_.get(), pagePost(rpcCall("aria2.getGlobalStat")));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    CPPUNIT_ASSERT(contains(res.body, "\"result\":{"));
    CPPUNIT_ASSERT(res.has("Content-Type: application/json-rpc"));
    CPPUNIT_ASSERT(!res.isPageResponse());
  }
  // The media type decides; Origin and Sec-Fetch-Site are checked only
  // if present.
  for (const auto& headers :
       {std::string("Content-Type: application/json; charset=UTF-8\r\n"
                    "X-Ariam-Page: 1\r\n"
                    "Origin: https://localhost\r\n"),
        std::string("Content-Type: application/json\r\nX-Ariam-Page: 1\r\n")}) {
    auto res = pageRequest(
        *listen_, e_.get(),
        post(rpcCall("ariam.getSiteSummary"),
             headers + basic("user:" + PASSPHRASE)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(headers, 200, res.status);
    CPPUNIT_ASSERT(contains(res.body, "\"result\":{"));
  }
  // A denied method answers as without the passphrase, and does not
  // run.
  {
    auto res = pageRequest(
        *listen_, e_.get(),
        pagePost(rpcCall("aria2.addUri", "[[\"http://host/added\"]]")));
    CPPUNIT_ASSERT_EQUAL(400, res.status);
    CPPUNIT_ASSERT(contains(res.body, UNAUTHORIZED));
    CPPUNIT_ASSERT_EQUAL((size_t)0,
                         e_->getRequestGroupMan()->getReservedGroups().size());
  }
  for (auto method : {"system.listMethods", "system.listNotifications",
                      "aria2.getGlobalOption", "aria2.getVersion"}) {
    auto res = pageRequest(*listen_, e_.get(), pagePost(rpcCall(method)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(method, 400, res.status);
    CPPUNIT_ASSERT_MESSAGE(method, contains(res.body, UNAUTHORIZED));
  }
  {
    // A wrong token does not take a sub-call off the passphrase path.
    auto res = pageRequest(
        *listen_, e_.get(),
        pagePost(rpcCall("system.multicall",
                         "[[{\"methodName\":\"aria2.getGlobalStat\","
                         "\"params\":[]},"
                         "{\"methodName\":\"system.listMethods\","
                         "\"params\":[]},"
                         "{\"methodName\":\"system.listMethods\","
                         "\"params\":[\"token:wrong\"]},"
                         "{\"methodName\":\"system.listNotifications\","
                         "\"params\":[\"token:wrong\"]},"
                         "{\"methodName\":\"aria2.getGlobalStat\","
                         "\"params\":[\"token:wrong\"]}]]")));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    auto json = parseJson(res.body);
    auto result = downcast<List>(downcast<Dict>(json)->get("result"));
    CPPUNIT_ASSERT(result);
    CPPUNIT_ASSERT_EQUAL((size_t)5, result->size());
    CPPUNIT_ASSERT(downcast<List>(result->get(0)));
    for (size_t i = 1; i < 5; ++i) {
      auto fault = downcast<Dict>(result->get(i));
      CPPUNIT_ASSERT_MESSAGE(util::uitos(i), fault);
      CPPUNIT_ASSERT_EQUAL_MESSAGE(util::uitos(i), std::string("Unauthorized"),
                                   downcast<String>(fault->get("message"))->s());
    }
  }
  // Successes are not counted.
  CPPUNIT_ASSERT_EQUAL((size_t)0, e_->getPassphraseLimiter().size());
}

void StatusPageTest::testPassphraseRpc_refusals()
{
  const std::string ct = "Content-Type: application/json\r\n";
  const std::string xp = "X-Ariam-Page: 1\r\n";
  const std::string og = "Origin: https://localhost\r\n";
  const std::string sf = "Sec-Fetch-Site: same-origin\r\n";
  const auto body = rpcCall("aria2.getGlobalStat");
  const auto right = basic("u:" + PASSPHRASE);
  const auto wrong = basic("u:wrong");
  // Each misses one precondition of the passphrase path.
  auto refused = [&](const std::string& credentials) {
    return std::vector<std::string>{
        post(body, ct + og + sf + credentials),
        post(body, ct + "X-Ariam-Page: 0\r\n" + og + sf + credentials),
        post(body, "Content-Type: text/plain;charset=UTF-8\r\n" + xp + og +
                       sf + credentials),
        post(body, "Content-Type: application/x-www-form-urlencoded\r\n" + xp +
                       og + sf + credentials),
        post(body, xp + og + sf + credentials),
        post(body, ct + xp + "Origin: https://evil.test\r\n" + sf + credentials),
        post(body, ct + xp + "Origin: http://localhost\r\n" + sf + credentials),
        post(body, ct + xp + "Origin: null\r\n" + sf + credentials),
        post(body, ct + xp + og + "Sec-Fetch-Site: cross-site\r\n" + credentials),
        post(body, ct + xp + og + "Sec-Fetch-Site: same-site\r\n" + credentials),
        // JSONP
        get("/jsonrpc?method=aria2.getGlobalStat&id=1&jsoncallback=cb",
            ct + xp + og + sf + credentials),
    };
  };
  // A stock request without a token: unauthorized, with no challenge
  // and no 429.
  auto checkStock = [](const Response& res, const std::string& request) {
    CPPUNIT_ASSERT_EQUAL_MESSAGE(request, 400, res.status);
    CPPUNIT_ASSERT_MESSAGE(request, contains(res.body, UNAUTHORIZED));
    CPPUNIT_ASSERT_MESSAGE(request, !res.hasField("WWW-Authenticate"));
    CPPUNIT_ASSERT_MESSAGE(request, !res.isPageResponse());
  };
  // The same request with every precondition met is accepted.
  CPPUNIT_ASSERT_EQUAL(
      200,
      pageRequest(*listen_, e_.get(), post(body, ct + xp + og + sf + right))
          .status);
  for (const auto& request : refused(right)) {
    checkStock(pageRequest(*listen_, e_.get(), request), request);
  }
  // The passphrase of a refused request is never checked: a wrong one
  // is not counted ...
  for (const auto& request : refused(wrong)) {
    checkStock(pageRequest(*listen_, e_.get(), request), request);
  }
  CPPUNIT_ASSERT_EQUAL((size_t)0, e_->getPassphraseLimiter().size());
  // ... and a lockout does not answer it with 429.
  for (int i = 0; i < PassphraseLimiter::LOCKOUT_THRESHOLD; ++i) {
    pageRequest(*listen_, e_.get(), post(body, ct + xp + og + sf + wrong));
  }
  CPPUNIT_ASSERT_EQUAL(
      429,
      pageRequest(*listen_, e_.get(), post(body, ct + xp + og + sf + right))
          .status);
  for (const auto& credentials : {right, wrong}) {
    for (const auto& request : refused(credentials)) {
      checkStock(pageRequest(*listen_, e_.get(), request), request);
    }
  }
  CPPUNIT_ASSERT_EQUAL(PassphraseLimiter::LOCKOUT_THRESHOLD,
                       e_->getPassphraseLimiter().countFailures(
                           PEER, global::wallclock()));
}

#ifdef ENABLE_WEBSOCKET
namespace {
// A masked client text frame carrying payload (RFC 6455 section 5.2).
std::string wsTextFrame(const std::string& payload)
{
  const char mask[] = {0x12, 0x34, 0x56, 0x78};
  std::string frame = "\x81";
  if (payload.size() < 126) {
    frame += static_cast<char>(0x80 | payload.size());
  }
  else {
    frame += static_cast<char>(0x80 | 126);
    frame += static_cast<char>((payload.size() >> 8) & 0xff);
    frame += static_cast<char>(payload.size() & 0xff);
  }
  frame.append(mask, sizeof(mask));
  for (size_t i = 0; i < payload.size(); ++i) {
    frame += static_cast<char>(payload[i] ^ mask[i % 4]);
  }
  return frame;
}

void readUntil(SocketCore& client, DownloadEngine* e, std::string& data,
               const std::string& part)
{
  for (int i = 0; i < 20 && !contains(data, part); ++i) {
    e->run(true);
    while (client.isReadable(0)) {
      char buf[16_k];
      size_t len = sizeof(buf);
      client.readData(buf, len);
      if (len == 0) {
        break;
      }
      data.append(buf, len);
    }
  }
  CPPUNIT_ASSERT_MESSAGE("No " + part + " in " + data, contains(data, part));
}
} // namespace

void StatusPageTest::testPassphraseRpc_webSocket()
{
  e_->setWebSocketSessionMan(make_unique<rpc::WebSocketSessionMan>());
  auto endpoint = listen_->getAddrInfo();
  SocketCore client;
  client.establishConnection("localhost", endpoint.port);
  while (!client.isWritable(0)) {
  }
  std::shared_ptr<SocketCore> inbound = listen_->acceptConnection();
  e_->addCommand(
      make_unique<HttpServerCommand>(e_->newCUID(), e_.get(), inbound, false));
  // The handshake carries every status page header field and the
  // passphrase.
  client.writeData(std::string("GET /jsonrpc HTTP/1.1\r\n"
                               "Host: localhost\r\n"
                               "Upgrade: websocket\r\n"
                               "Connection: Upgrade\r\n"
                               "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                               "Sec-WebSocket-Version: 13\r\n") +
                   PAGE_RPC_HEADERS + basic("user:" + PASSPHRASE) + "\r\n");
  std::string data;
  readUntil(client, e_.get(), data, "\r\n\r\n");
  CPPUNIT_ASSERT(data.find("HTTP/1.1 101 Switching Protocols\r\n") == 0);
  data.clear();
  // The session serves token calls ...
  client.writeData(
      wsTextFrame(rpcCall("aria2.getGlobalStat", "[\"token:secret\"]", 1)));
  readUntil(client, e_.get(), data, "\"result\":{");
  data.clear();
  // ... but never the passphrase path: a call without a token is
  // unauthorized, and the passphrase is not consulted.
  client.writeData(wsTextFrame(rpcCall("aria2.getGlobalStat", "[]", 2)));
  readUntil(client, e_.get(), data, UNAUTHORIZED);
  CPPUNIT_ASSERT_EQUAL((size_t)0, e_->getPassphraseLimiter().size());
}
#endif // ENABLE_WEBSOCKET

void StatusPageTest::testPassphraseRpc_batch()
{
  auto& rgman = e_->getRequestGroupMan();
  auto stopped = createDownloadResult(error_code::UNKNOWN_ERROR,
                                      "http://host/stopped?sig=1");
  stopped->resultMessage = "Redirect to https://cdn.test/stopped?sig=1 failed";
  rgman->addDownloadResult(stopped);
  const auto& limiter = e_->getPassphraseLimiter();
  auto& now = global::wallclock();
  auto batchOf = [](const std::vector<std::string>& calls) {
    std::string batch = "[";
    for (const auto& call : calls) {
      batch += (batch.size() > 1 ? "," : "") + call;
    }
    return batch + "]";
  };
  {
    // A valid token on one call neither authorizes nor un-redacts
    // another.
    auto res = pageRequest(
        *listen_, e_.get(),
        pagePost(batchOf(
            {rpcCall("aria2.tellStopped", "[\"token:secret\",0,10]", 1),
             rpcCall("aria2.tellStopped", "[0,10]", 2),
             rpcCall("aria2.addUri", "[[\"http://host/added\"]]", 3),
             rpcCall("aria2.getGlobalOption", "[]", 4),
             rpcCall("aria2.getGlobalStat", "[]", 5)})));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    auto json = parseJson(res.body);
    auto token = onlyEntry(member(json.get(), 0, 5));
    CPPUNIT_ASSERT_EQUAL(stopped->resultMessage,
                         downcast<String>(token->get("errorMessage"))->s());
    CPPUNIT_ASSERT(hasUris(token));
    auto passphrase = onlyEntry(member(json.get(), 1, 5));
    CPPUNIT_ASSERT(!passphrase->containsKey("errorMessage"));
    CPPUNIT_ASSERT(!hasUris(passphrase));
    CPPUNIT_ASSERT(isUnauthorized(member(json.get(), 2, 5)));
    CPPUNIT_ASSERT(isUnauthorized(member(json.get(), 3, 5)));
    CPPUNIT_ASSERT(member(json.get(), 4, 5)->containsKey("result"));
    CPPUNIT_ASSERT_EQUAL((size_t)0, rgman->getReservedGroups().size());
  }
  {
    // Without credentials, only the call with a token runs; nothing is
    // counted.
    auto res = pageRequest(
        *listen_, e_.get(),
        post(batchOf({rpcCall("aria2.getGlobalOption", "[\"token:secret\"]", 1),
                      rpcCall("aria2.getGlobalStat", "[]", 2)}),
             PAGE_RPC_HEADERS));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    auto json = parseJson(res.body);
    CPPUNIT_ASSERT(member(json.get(), 0, 2)->containsKey("result"));
    CPPUNIT_ASSERT(isUnauthorized(member(json.get(), 1, 2)));
    CPPUNIT_ASSERT_EQUAL((size_t)0, limiter.size());
  }
  {
    // Forbidden methods stay denied in a passphrase-only batch.
    auto res = pageRequest(
        *listen_, e_.get(),
        pagePost(batchOf(
            {rpcCall("aria2.getGlobalStat", "[]", 1),
             rpcCall("aria2.addUri", "[[\"http://host/added\"]]", 2),
             rpcCall("system.listMethods", "[]", 3),
             rpcCall("system.listNotifications", "[]", 4),
             rpcCall("aria2.changeGlobalOption",
                     "[{\"max-concurrent-downloads\":\"5\"}]", 5),
             rpcCall("aria2.changeGlobalOption",
                     "[{\"site-concurrent-default\":\"3\"}]", 6),
             rpcCall("aria2.shutdown", "[]", 7)})));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    auto json = parseJson(res.body);
    CPPUNIT_ASSERT(member(json.get(), 0, 7)->containsKey("result"));
    for (size_t i : {1, 2, 3, 4, 6}) {
      CPPUNIT_ASSERT_MESSAGE(util::uitos(i),
                             isUnauthorized(member(json.get(), i, 7)));
    }
    CPPUNIT_ASSERT_EQUAL(std::string("OK"),
                         downcast<String>(member(json.get(), 5, 7)->get("result"))
                             ->s());
    CPPUNIT_ASSERT_EQUAL((size_t)0, rgman->getReservedGroups().size());
    CPPUNIT_ASSERT_EQUAL(1, rgman->getMaxConcurrentDownloads());
    CPPUNIT_ASSERT_EQUAL(std::string("3"),
                         option_->get(PREF_SITE_CONCURRENT_DEFAULT));
  }
  {
    // The passphrase is checked once per request: a wrong one counts
    // once, and the call with a token still runs.
    auto res = pageRequest(
        *listen_, e_.get(),
        pagePost(batchOf({rpcCall("aria2.getGlobalStat", "[]", 1),
                          rpcCall("aria2.tellActive", "[]", 2),
                          rpcCall("ariam.getSiteSummary", "[]", 3),
                          rpcCall("aria2.getGlobalStat",
                                  "[\"token:secret\"]", 4)}),
                 basic("u:wrong")));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    auto json = parseJson(res.body);
    for (size_t i = 0; i < 3; ++i) {
      CPPUNIT_ASSERT(isUnauthorized(member(json.get(), i, 4)));
    }
    CPPUNIT_ASSERT(member(json.get(), 3, 4)->containsKey("result"));
    CPPUNIT_ASSERT_EQUAL(1, limiter.countFailures(PEER, now));
  }
  {
    // When every call carries a token, the credentials are ignored.
    auto res = pageRequest(
        *listen_, e_.get(),
        pagePost(batchOf({rpcCall("aria2.getGlobalStat", "[\"token:secret\"]",
                                  1),
                          rpcCall("aria2.tellActive", "[\"token:secret\"]",
                                  2)}),
                 basic("u:wrong")));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    auto json = parseJson(res.body);
    CPPUNIT_ASSERT(member(json.get(), 0, 2)->containsKey("result"));
    CPPUNIT_ASSERT(member(json.get(), 1, 2)->containsKey("result"));
    CPPUNIT_ASSERT_EQUAL(1, limiter.countFailures(PEER, now));
  }
  {
    // So they are for system.multicall whose sub-calls all carry a
    // valid token.
    auto res = pageRequest(
        *listen_, e_.get(),
        pagePost(rpcCall("system.multicall",
                         "[[{\"methodName\":\"aria2.getGlobalStat\","
                         "\"params\":[\"token:secret\"]},"
                         "{\"methodName\":\"aria2.tellStopped\","
                         "\"params\":[\"token:secret\",0,10]}]]"),
                 basic("u:wrong")));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    auto json = parseJson(res.body);
    auto result = downcast<List>(downcast<Dict>(json)->get("result"));
    CPPUNIT_ASSERT(result);
    CPPUNIT_ASSERT_EQUAL((size_t)2, result->size());
    CPPUNIT_ASSERT(downcast<List>(result->get(0)));
    // The token path: unredacted.
    auto entries = downcast<List>(downcast<List>(result->get(1))->get(0));
    CPPUNIT_ASSERT(entries);
    CPPUNIT_ASSERT_EQUAL((size_t)1, entries->size());
    auto entry = downcast<Dict>(entries->get(0));
    CPPUNIT_ASSERT(entry);
    CPPUNIT_ASSERT(entry->containsKey("errorMessage"));
    CPPUNIT_ASSERT(hasUris(entry));
    CPPUNIT_ASSERT_EQUAL(1, limiter.countFailures(PEER, now));
  }
  {
    // A sub-call without a token needs the passphrase: a wrong one
    // counts, and only the sub-call with a token runs.
    auto res = pageRequest(
        *listen_, e_.get(),
        pagePost(rpcCall("system.multicall",
                         "[[{\"methodName\":\"aria2.getGlobalStat\","
                         "\"params\":[\"token:secret\"]},"
                         "{\"methodName\":\"aria2.getGlobalStat\","
                         "\"params\":[]}]]"),
                 basic("u:wrong")));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    auto json = parseJson(res.body);
    auto result = downcast<List>(downcast<Dict>(json)->get("result"));
    CPPUNIT_ASSERT(result);
    CPPUNIT_ASSERT_EQUAL((size_t)2, result->size());
    CPPUNIT_ASSERT(downcast<List>(result->get(0)));
    auto fault = downcast<Dict>(result->get(1));
    CPPUNIT_ASSERT(fault);
    CPPUNIT_ASSERT_EQUAL(std::string("Unauthorized"),
                         downcast<String>(fault->get("message"))->s());
    CPPUNIT_ASSERT_EQUAL(2, limiter.countFailures(PEER, now));
  }
}

void StatusPageTest::testPassphraseRpc_rateLimit()
{
  const auto& limiter = e_->getPassphraseLimiter();
  auto& now = global::wallclock();
  const auto body = rpcCall("aria2.getGlobalStat");
  const auto wrong = basic("u:wrong");
  // Failed checks on the RPC path count with those of the page routes.
  for (int i = 0; i < 4; ++i) {
    auto res = pageRequest(*listen_, e_.get(), pagePost(body, wrong));
    CPPUNIT_ASSERT_EQUAL(400, res.status);
    CPPUNIT_ASSERT(contains(res.body, UNAUTHORIZED));
  }
  CPPUNIT_ASSERT_EQUAL(4, limiter.countFailures(PEER, now));
  CPPUNIT_ASSERT(!limiter.locked(PEER, now));
  CPPUNIT_ASSERT_EQUAL(401,
                       pageRequest(*listen_, e_.get(), get("/", wrong)).status);
  CPPUNIT_ASSERT(limiter.locked(PEER, now));
  // Locked out: 429 without checking the right passphrase, for a call,
  // a batch, and system.multicall.
  for (const auto& b :
       {body, "[" + body + "]",
        rpcCall("system.multicall",
                "[[{\"methodName\":\"aria2.getGlobalStat\"}]]")}) {
    auto res = pageRequest(*listen_, e_.get(), pagePost(b));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(b, 429, res.status);
    CPPUNIT_ASSERT(res.headers.find("HTTP/1.1 429 Too Many Requests\r\n") == 0);
    CPPUNIT_ASSERT(res.body.empty());
    CPPUNIT_ASSERT(!res.hasField("WWW-Authenticate"));
  }
  CPPUNIT_ASSERT_EQUAL(
      429, pageRequest(*listen_, e_.get(), authorizedGet("/")).status);
  CPPUNIT_ASSERT_EQUAL(5, limiter.countFailures(PEER, now));
  // A token request from the locked-out address proceeds: a stock
  // client, and the page's headers whose credentials are then ignored.
  const auto tokenBody = rpcCall("aria2.getGlobalStat", "[\"token:secret\"]");
  for (const auto& request :
       {post(tokenBody, "Content-Type: application/json\r\n"),
        pagePost(tokenBody, wrong), pagePost(tokenBody)}) {
    auto res = pageRequest(*listen_, e_.get(), request);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(request, 200, res.status);
    CPPUNIT_ASSERT_MESSAGE(request, contains(res.body, "\"result\":{"));
  }
  // A request holding a call with a valid token runs on the token path:
  // that call runs, and a call without a token is unauthorized.
  const auto stat = std::string("{\"methodName\":\"aria2.getGlobalStat\","
                                "\"params\":[]}");
  const auto tokenStat =
      std::string("{\"methodName\":\"aria2.getGlobalStat\","
                  "\"params\":[\"token:secret\"]}");
  for (const auto& credentials : {basic("u:" + PASSPHRASE), wrong}) {
    {
      const auto batch =
          "[" + tokenBody + "," + rpcCall("aria2.getGlobalStat", "[]", 2) + "]";
      auto res = pageRequest(*listen_, e_.get(), pagePost(batch, credentials));
      CPPUNIT_ASSERT_EQUAL(200, res.status);
      auto json = parseJson(res.body);
      CPPUNIT_ASSERT(member(json.get(), 0, 2)->containsKey("result"));
      CPPUNIT_ASSERT(isUnauthorized(member(json.get(), 1, 2)));
    }
    // system.multicall, the page's request shape: sub-calls that all
    // carry a valid token, and a mix.
    for (const auto& calls :
         {"[" + tokenStat + "," + tokenStat + "]",
          "[" + tokenStat + "," + stat + "]"}) {
      auto res = pageRequest(
          *listen_, e_.get(),
          pagePost(rpcCall("system.multicall", "[" + calls + "]"),
                   credentials));
      CPPUNIT_ASSERT_EQUAL_MESSAGE(calls, 200, res.status);
      auto json = parseJson(res.body);
      auto result = downcast<List>(downcast<Dict>(json)->get("result"));
      CPPUNIT_ASSERT_MESSAGE(calls, result);
      CPPUNIT_ASSERT_EQUAL((size_t)2, result->size());
      CPPUNIT_ASSERT_MESSAGE(calls, downcast<List>(result->get(0)));
      if (calls.find(stat) == std::string::npos) {
        CPPUNIT_ASSERT_MESSAGE(calls, downcast<List>(result->get(1)));
      }
      else {
        auto fault = downcast<Dict>(result->get(1));
        CPPUNIT_ASSERT_MESSAGE(calls, fault);
        CPPUNIT_ASSERT_EQUAL(std::string("Unauthorized"),
                             downcast<String>(fault->get("message"))->s());
      }
    }
  }
  // A wrong token is no valid token: 429.
  for (const auto& b :
       {"[" + rpcCall("aria2.getGlobalStat", "[\"token:wrong\"]") + "," + body +
            "]",
        rpcCall("system.multicall",
                "[[{\"methodName\":\"aria2.getGlobalStat\","
                "\"params\":[\"token:wrong\"]}," +
                    stat + "]]")}) {
    auto res = pageRequest(*listen_, e_.get(), pagePost(b));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(b, 429, res.status);
  }
  CPPUNIT_ASSERT_EQUAL(5, limiter.countFailures(PEER, now));
  CPPUNIT_ASSERT(limiter.locked(PEER, now));
  {
    // Without credentials nothing is checked: the stock response.
    auto res = pageRequest(*listen_, e_.get(), post(body, PAGE_RPC_HEADERS));
    CPPUNIT_ASSERT_EQUAL(400, res.status);
    CPPUNIT_ASSERT(contains(res.body, UNAUTHORIZED));
  }
  CPPUNIT_ASSERT_EQUAL(5, limiter.countFailures(PEER, now));
  // The 1 s lockout ends; the right passphrase resets the count.
  now.advance(1_s);
  CPPUNIT_ASSERT_EQUAL(200,
                       pageRequest(*listen_, e_.get(), pagePost(body)).status);
  CPPUNIT_ASSERT_EQUAL((size_t)0, limiter.size());
  // On the RPC path alone, 5 failures lock out.
  for (int i = 0; i < PassphraseLimiter::LOCKOUT_THRESHOLD; ++i) {
    pageRequest(*listen_, e_.get(), pagePost(body, wrong));
  }
  CPPUNIT_ASSERT_EQUAL(429,
                       pageRequest(*listen_, e_.get(), pagePost(body)).status);
}

void StatusPageTest::testAcceptsJsonpCallback()
{
  const std::string longest(status_page::MAX_JSONP_CALLBACK_LENGTH, 'a');
  for (const auto& callback :
       {std::string(), std::string("cb"), std::string("a.b_c$"),
        std::string("_"), std::string("$"), std::string("Z9"),
        std::string("jQuery3510_1600000000000"),
        std::string("angular.callbacks._0"), longest}) {
    CPPUNIT_ASSERT_MESSAGE(callback,
                           status_page::acceptsJsonpCallback(callback));
  }
  for (const auto& callback :
       {std::string("alert(1)//"), std::string("a;b"), std::string("a(b)"),
        std::string("<x>"), std::string("\xc3\xa9"), std::string("a\xc3\xa9"),
        std::string("%61"), std::string("1a"), std::string("0"),
        std::string(".a"), std::string("a-b"), std::string("a b"),
        std::string("a\nb"), std::string("a\0b", 3), std::string("a=b"),
        longest + "a"}) {
    CPPUNIT_ASSERT_MESSAGE(callback,
                           !status_page::acceptsJsonpCallback(callback));
  }
}

namespace {
// callback is sent as is, without percent-encoding.
std::string jsonpGet(const std::string& method, const std::string& params,
                     const std::string& callback)
{
  return get("/jsonrpc?method=" + method + "&id=1&params=" +
             base64::encode(std::begin(params), std::end(params)) +
             "&jsoncallback=" + callback);
}
} // namespace

void StatusPageTest::testJsonpCallback()
{
  auto& rgman = e_->getRequestGroupMan();
  const std::string addUri = "[\"token:secret\",[\"http://host/added\"]]";
  const std::string longest(status_page::MAX_JSONP_CALLBACK_LENGTH, 'a');
  // Status page enabled: a non-identifier callback is refused before
  // any method runs, also a parse error, with no script body.
  for (const auto& callback :
       {std::string("alert(1)//"), std::string("a;b"), std::string("a(b)"),
        std::string("<x>"), std::string("\xc3\xa9"), std::string("%C3%A9"),
        longest + "a", std::string("1a")}) {
    for (const auto& request :
         {jsonpGet("aria2.addUri", addUri, callback),
          jsonpGet("system.listMethods", "[]", callback),
          // params is the Base64 of "{".
          get("/jsonrpc?params=ew==&jsoncallback=" + callback)}) {
      auto res = pageRequest(*listen_, e_.get(), request);
      CPPUNIT_ASSERT_EQUAL_MESSAGE(request, 400, res.status);
      CPPUNIT_ASSERT_MESSAGE(request,
                             res.has("Content-Type: application/json-rpc"));
      CPPUNIT_ASSERT_MESSAGE(request,
                             !contains(res.headers, "text/javascript"));
      auto json = parseJson(res.body);
      auto error = downcast<Dict>(downcast<Dict>(json)->get("error"));
      CPPUNIT_ASSERT_MESSAGE(request, error);
      CPPUNIT_ASSERT_EQUAL_MESSAGE(
          request, (int64_t)-32600, downcast<Integer>(error->get("code"))->i());
    }
  }
  CPPUNIT_ASSERT_EQUAL((size_t)0, rgman->getReservedGroups().size());
  // An identifier callback is served as stock.
  for (const auto& callback :
       {std::string("cb"), std::string("a.b_c$"), longest}) {
    auto res = pageRequest(*listen_, e_.get(),
                           jsonpGet("system.listMethods", "[]", callback));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(callback, 200, res.status);
    CPPUNIT_ASSERT(res.has("Content-Type: text/javascript"));
    CPPUNIT_ASSERT_MESSAGE(res.body, res.body.find(callback + "({") == 0);
  }
  {
    auto res =
        pageRequest(*listen_, e_.get(), jsonpGet("aria2.addUri", addUri, "cb"));
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    CPPUNIT_ASSERT(res.has("Content-Type: text/javascript"));
    CPPUNIT_ASSERT_EQUAL((size_t)1, rgman->getReservedGroups().size());
  }
  // Status page disabled: stock, which copies any callback.
  {
    auto res = pageRequest(*listen_, e_.get(),
                           jsonpGet("system.listMethods", "[]", "alert(1)//"),
                           false);
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    CPPUNIT_ASSERT(res.has("Content-Type: text/javascript"));
    CPPUNIT_ASSERT_MESSAGE(res.body, res.body.find("alert(1)//({") == 0);
  }
  {
    auto res = pageRequest(*listen_, e_.get(),
                           jsonpGet("aria2.addUri", addUri, "a;b"), false);
    CPPUNIT_ASSERT_EQUAL(200, res.status);
    CPPUNIT_ASSERT(res.has("Content-Type: text/javascript"));
    CPPUNIT_ASSERT_MESSAGE(res.body, res.body.find("a;b({") == 0);
    CPPUNIT_ASSERT_EQUAL((size_t)2, rgman->getReservedGroups().size());
  }
}

} // namespace aria2
