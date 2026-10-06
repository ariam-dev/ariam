#include "HttpServer.h"

#include <cppunit/extensions/HelperMacros.h>

#include "SocketCore.h"
#include "HttpHeader.h"
#include "StatusPage.h"
#include "LogFactory.h"
#include "Logger.h"
#include "File.h"
#include "TestUtil.h"
#include "a2functional.h"

namespace aria2 {

class HttpServerTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(HttpServerTest);
  CPPUNIT_TEST(testHttpBasicAuth);
  CPPUNIT_TEST(testStatusPageRequestHeaders);
  CPPUNIT_TEST(testAuthorizationRedactedInLog);
  CPPUNIT_TEST(testStatusPageRequestType);
  CPPUNIT_TEST(testAcceptsPassphraseRpc);
  CPPUNIT_TEST_SUITE_END();

public:
  void testHttpBasicAuth();
  void testStatusPageRequestHeaders();
  void testAuthorizationRedactedInLog();
  void testStatusPageRequestType();
  void testAcceptsPassphraseRpc();
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpServerTest);

namespace {
std::unique_ptr<HttpServer> performHttpRequest(SocketCore& server,
                                               std::string request)
{
  auto endpoint = server.getAddrInfo();

  SocketCore client;
  client.establishConnection("localhost", endpoint.port);
  while (!client.isWritable(0)) {
  }

  auto inbound = server.acceptConnection();
  inbound->setBlockingMode();
  auto rv = make_unique<HttpServer>(inbound);

  client.writeData(request);
  while (!rv->receiveRequest()) {
  }
  return rv;
}
} // namespace

void HttpServerTest::testHttpBasicAuth()
{
  SocketCore server;
  server.bind(0);
  server.beginListen();
  server.setBlockingMode();

  {
    // Default is no auth
    auto req = performHttpRequest(
        server, "GET / HTTP/1.1\r\nUser-Agent: aria2-test\r\n\r\n");
    CPPUNIT_ASSERT(req->authenticate());
  }

  {
    // Empty user-name and password should come out as no auth.
    auto req = performHttpRequest(
        server, "GET / HTTP/1.1\r\nUser-Agent: aria2-test\r\n\r\n");
    req->setUsernamePassword("", "");
    CPPUNIT_ASSERT(req->authenticate());
  }

  {
    // Empty user-name but set password should also come out as no auth.
    auto req = performHttpRequest(
        server, "GET / HTTP/1.1\r\nUser-Agent: aria2-test\r\n\r\n");
    req->setUsernamePassword("", "pass");
    CPPUNIT_ASSERT(req->authenticate());
  }

  {
    // Client provided credentials should be ignored when there is no auth.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword("", "pass");
    CPPUNIT_ASSERT(req->authenticate());
  }

  {
    // Correct client provided credentials should match.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword("user", "pass");
    CPPUNIT_ASSERT(req->authenticate());
  }

  {
    // Correct client provided credentials should match (2).
    // Embedded nulls
    auto req = performHttpRequest(
        server, "GET / HTTP/1.1\r\nUser-Agent: aria2-test\r\nAuthorization: "
                "Basic dXNlcgBudWxsOnBhc3MAbnVsbA==\r\n\r\n");
    req->setUsernamePassword(std::string("user\0null", 9),
                             std::string("pass\0null", 9));
    CPPUNIT_ASSERT(req->authenticate());
  }

  {
    // Correct client provided credentials should match (3).
    // Embedded, leading nulls
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "AHVzZXI6AHBhc3M=\r\n\r\n");
    req->setUsernamePassword(std::string("\0user", 5),
                             std::string("\0pass", 5));
    CPPUNIT_ASSERT(req->authenticate());
  }

  {
    // Correct client provided credentials should match (3).
    // Whitespace
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "IHVzZXIJOgpwYXNzDQ==\r\n\r\n");
    req->setUsernamePassword(" user\t", "\npass\r");
    CPPUNIT_ASSERT(req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword("user", "pass2");
    CPPUNIT_ASSERT(!req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match (2).
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword("user2", "pass");
    CPPUNIT_ASSERT(!req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match (3).
    // Embedded null in pass config.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword("user", std::string("pass\0three", 10));
    CPPUNIT_ASSERT(!req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match (4).
    // Embedded null in user config.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword(std::string("user\0four", 9), "pass");
    CPPUNIT_ASSERT(!req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match (5).
    // Embedded null in http auth.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNzAHRocmVl\r\n\r\n");
    req->setUsernamePassword("user", "pass");
    CPPUNIT_ASSERT(!req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match (6).
    // Embedded null in http auth.
    // Embedded, leading nulls
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "AHVzZXI6AHBhc3M=\r\n\r\n");
    req->setUsernamePassword(std::string("\0user5", 6),
                             std::string("\0pass", 5));
    CPPUNIT_ASSERT(!req->authenticate());
  }

  {
    // When there is a user and password, the client must actually provide auth.
    auto req = performHttpRequest(
        server, "GET / HTTP/1.1\r\nUser-Agent: aria2-test\r\n\r\n");
    req->setUsernamePassword("user", "pass");
    CPPUNIT_ASSERT(!req->authenticate());
  }
}

void HttpServerTest::testStatusPageRequestHeaders()
{
  SocketCore server;
  server.bind(0);
  server.beginListen();
  server.setBlockingMode();

  {
    // Field names are case-insensitive.
    auto req = performHttpRequest(
        server, "POST /jsonrpc HTTP/1.1\r\n"
                "Host: ariam.example:6800\r\n"
                "x-ariam-page: 1\r\n"
                "SEC-FETCH-SITE: same-origin\r\n"
                "Origin: https://ariam.example:6800\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 0\r\n"
                "\r\n");
    const auto& header = req->getRequestHeader();
    CPPUNIT_ASSERT_EQUAL(std::string("ariam.example:6800"),
                         header->find(HttpHeader::HTTP_HOST));
    CPPUNIT_ASSERT_EQUAL(std::string("1"),
                         header->find(HttpHeader::X_ARIAM_PAGE));
    CPPUNIT_ASSERT_EQUAL(std::string("same-origin"),
                         header->find(HttpHeader::SEC_FETCH_SITE));
    CPPUNIT_ASSERT_EQUAL(std::string("https://ariam.example:6800"),
                         header->find(HttpHeader::ORIGIN));
    CPPUNIT_ASSERT_EQUAL(std::string("application/json"),
                         header->find(HttpHeader::CONTENT_TYPE));
  }
  {
    auto req = performHttpRequest(
        server, "POST /jsonrpc HTTP/1.1\r\nContent-Length: 0\r\n\r\n");
    const auto& header = req->getRequestHeader();
    CPPUNIT_ASSERT(!header->defined(HttpHeader::HTTP_HOST));
    CPPUNIT_ASSERT(!header->defined(HttpHeader::X_ARIAM_PAGE));
    CPPUNIT_ASSERT(!header->defined(HttpHeader::SEC_FETCH_SITE));
  }
}

void HttpServerTest::testAuthorizationRedactedInLog()
{
  SocketCore server;
  server.bind(0);
  server.beginListen();
  server.setBlockingMode();

  const std::string logFile =
      A2_TEST_OUT_DIR "/aria2_HttpServerTest_testAuthorizationRedactedInLog.log";
  File(logFile).remove();
  LogFactory::setLogFile(logFile);
  LogFactory::setLogLevel(Logger::A2_INFO);
  LogFactory::reconfigure();

  // "secret:passphrase"
  performHttpRequest(server, "GET / HTTP/1.1\r\n"
                             "User-Agent: aria2-test\r\n"
                             "Authorization: Basic c2VjcmV0OnBhc3NwaHJhc2U=\r\n"
                             "\r\n");
  // Lowercase name, value folded onto a continuation line.
  performHttpRequest(server, "GET / HTTP/1.1\r\n"
                             "authorization: Basic\r\n"
                             " c2VjcmV0OnBhc3NwaHJhc2U=\r\n"
                             "X-Ariam-Page: 1\r\n"
                             "\r\n");

  LogFactory::setLogFile("");
  LogFactory::setLogLevel(Logger::A2_DEBUG);
  LogFactory::reconfigure();

  auto log = readFile(logFile);
  CPPUNIT_ASSERT(log.find("HTTP Server received request") != std::string::npos);
  CPPUNIT_ASSERT(log.find("Authorization: <redacted>\r\n") != std::string::npos);
  CPPUNIT_ASSERT(log.find("authorization: <redacted>\r\n") != std::string::npos);
  CPPUNIT_ASSERT(log.find("c2VjcmV0") == std::string::npos);
  CPPUNIT_ASSERT(log.find("Basic") == std::string::npos);
  // Other fields are logged as received.
  CPPUNIT_ASSERT(log.find("User-Agent: aria2-test\r\n") != std::string::npos);
  CPPUNIT_ASSERT(log.find("X-Ariam-Page: 1\r\n") != std::string::npos);
}

void HttpServerTest::testStatusPageRequestType()
{
  SocketCore server;
  server.bind(0);
  server.beginListen();
  server.setBlockingMode();

  {
    // Status page routes do not exist unless enabled.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\n\r\n");
    CPPUNIT_ASSERT_EQUAL(RPC_TYPE_NONE, req->getRequestType());
    CPPUNIT_ASSERT_EQUAL(-1, req->setupResponseRecv());
  }
  {
    auto req = performHttpRequest(server, "GET /app.js?v=1 HTTP/1.1\r\n\r\n");
    req->enableStatusPage();
    CPPUNIT_ASSERT_EQUAL(0, req->setupResponseRecv());
    CPPUNIT_ASSERT_EQUAL(STATUS_PAGE_TYPE, req->getRequestType());
    CPPUNIT_ASSERT_EQUAL(std::string("app.js"), req->getStatusPageFile());
  }
  {
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\n\r\n");
    req->enableStatusPage();
    CPPUNIT_ASSERT_EQUAL(0, req->setupResponseRecv());
    CPPUNIT_ASSERT_EQUAL(STATUS_PAGE_TYPE, req->getRequestType());
    CPPUNIT_ASSERT_EQUAL(std::string("index.html"), req->getStatusPageFile());
  }
  {
    // The raw path decides: %2e%2e is not decoded.
    auto req =
        performHttpRequest(server, "GET /%2e%2e/index.html HTTP/1.1\r\n\r\n");
    req->enableStatusPage();
    CPPUNIT_ASSERT_EQUAL(-1, req->setupResponseRecv());
    CPPUNIT_ASSERT_EQUAL(RPC_TYPE_NONE, req->getRequestType());
  }
  {
    // JSONP keeps its route.
    auto req = performHttpRequest(server, "GET /jsonrpc HTTP/1.1\r\n\r\n");
    req->enableStatusPage();
    CPPUNIT_ASSERT_EQUAL(0, req->setupResponseRecv());
    CPPUNIT_ASSERT_EQUAL(RPC_TYPE_JSONP, req->getRequestType());
  }
}

void HttpServerTest::testAcceptsPassphraseRpc()
{
  SocketCore server;
  server.bind(0);
  server.beginListen();
  server.setBlockingMode();

  const std::string host = "Host: ariam.example:6800\r\n";
  const std::string json = "Content-Type: application/json\r\n";
  const std::string page = "X-Ariam-Page: 1\r\n";
  const std::string origin = "Origin: https://ariam.example:6800\r\n";
  const std::string site = "Sec-Fetch-Site: same-origin\r\n";
  auto acceptsRoute = [&server](const std::string& requestLine,
                                const std::string& headers) {
    auto req = performHttpRequest(server, requestLine + headers +
                                              "Content-Length: 0\r\n\r\n");
    req->enableStatusPage();
    return status_page::acceptsPassphraseRpc(*req);
  };
  auto accepts = [&acceptsRoute](const std::string& headers) {
    return acceptsRoute("POST /jsonrpc HTTP/1.1\r\n", headers);
  };

  const std::string good = host + json + page + origin + site;
  CPPUNIT_ASSERT(accepts(good));
  // Origin and Sec-Fetch-Site are checked only if present.
  CPPUNIT_ASSERT(accepts(host + json + page));
  CPPUNIT_ASSERT(accepts(host + json + page + origin));
  CPPUNIT_ASSERT(accepts(host + json + page + site));
  // The media type decides; parameters are ignored.
  for (auto contentType :
       {"application/json; charset=utf-8", "application/json;charset=UTF-8",
        "Application/JSON", "application/json ; charset=utf-8"}) {
    CPPUNIT_ASSERT_MESSAGE(
        contentType,
        accepts(host + "Content-Type: " + contentType + "\r\n" + page +
                origin + site));
  }
  // A query does not change the route.
  CPPUNIT_ASSERT(acceptsRoute("POST /jsonrpc?x=1 HTTP/1.1\r\n", good));

  for (const auto& headers : std::vector<std::string>{
           // X-Ariam-Page
           host + json + origin + site,
           host + json + "X-Ariam-Page: 0\r\n" + origin + site,
           host + json + "X-Ariam-Page: true\r\n" + origin + site,
           host + json + "X-Ariam-Page:\r\n" + origin + site,
           host + json + page + page + origin + site,
           // Content-Type
           host + page + origin + site,
           host + "Content-Type: text/plain;charset=UTF-8\r\n" + page +
               origin + site,
           host + "Content-Type: application/x-www-form-urlencoded\r\n" +
               page + origin + site,
           host + "Content-Type: multipart/form-data; boundary=x\r\n" + page +
               origin + site,
           host + "Content-Type: application/json-rpc\r\n" + page + origin +
               site,
           host + "Content-Type: application/jsonp\r\n" + page + origin + site,
           host + "Content-Type: text/json\r\n" + page + origin + site,
           host + json + json + page + origin + site,
           // Origin
           host + json + page + "Origin: https://evil.test\r\n" + site,
           host + json + page + "Origin: http://ariam.example:6800\r\n" +
               site,
           host + json + page + "Origin: https://ariam.example\r\n" +
               site,
           host + json + page +
               "Origin: https://ariam.example:6800.evil.test\r\n" + site,
           host + json + page + "Origin: null\r\n" + site,
           host + json + page + "Origin:\r\n" + site,
           host + json + page + origin + origin + site,
           // Sec-Fetch-Site
           host + json + page + origin + "Sec-Fetch-Site: cross-site\r\n",
           host + json + page + origin + "Sec-Fetch-Site: same-site\r\n",
           host + json + page + origin + "Sec-Fetch-Site: none\r\n",
           host + json + page + origin + site + site,
           // Host
           json + page,
           json + page + origin + site,
           "Host:\r\n" + json + page,
           host + host + json + page,
       }) {
    CPPUNIT_ASSERT_MESSAGE(headers, !accepts(headers));
  }
  // Other routes and methods: JSONP, the status page files, XML-RPC.
  for (auto requestLine :
       {"GET /jsonrpc?method=aria2.getGlobalStat&id=1 HTTP/1.1\r\n",
        "GET /jsonrpc HTTP/1.1\r\n", "POST / HTTP/1.1\r\n",
        "POST /rpc HTTP/1.1\r\n", "PUT /jsonrpc HTTP/1.1\r\n",
        "GET / HTTP/1.1\r\n"}) {
    CPPUNIT_ASSERT_MESSAGE(requestLine, !acceptsRoute(requestLine, good));
  }
  {
    // Status page routes do not exist.
    auto req = performHttpRequest(server, "POST /jsonrpc HTTP/1.1\r\n" + good +
                                              "Content-Length: 0\r\n\r\n");
    CPPUNIT_ASSERT(!status_page::acceptsPassphraseRpc(*req));
  }
}

} // namespace aria2
