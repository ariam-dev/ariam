#include "HttpRequest.h"

#include <sstream>

#include <cppunit/extensions/HelperMacros.h>

#include "prefs.h"
#include "AuthConfigFactory.h"
#include "PiecedSegment.h"
#include "Piece.h"
#include "Range.h"
#include "Request.h"
#include "Option.h"
#include "array_fun.h"
#include "CookieStorage.h"
#include "util.h"
#include "AuthConfig.h"
#include "TestUtil.h"
#include "MessageDigest.h"

namespace aria2 {

class HttpRequestTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(HttpRequestTest);
  CPPUNIT_TEST(testGetStartByte);
  CPPUNIT_TEST(testGetEndByte);
  CPPUNIT_TEST(testCreateRequest);
  CPPUNIT_TEST(testCreateRequest_ftp);
  CPPUNIT_TEST(testCreateRequest_with_cookie);
  CPPUNIT_TEST(testCreateRequest_query);
  CPPUNIT_TEST(testCreateRequest_head);
  CPPUNIT_TEST(testCreateRequest_ipv6LiteralAddr);
  CPPUNIT_TEST(testCreateRequest_endOffsetOverride);
  CPPUNIT_TEST(testCreateRequest_wantDigest);
  CPPUNIT_TEST(testCreateProxyRequest);
  CPPUNIT_TEST(testIsRangeSatisfied);
  CPPUNIT_TEST(testUserAgent);
  CPPUNIT_TEST(testAddHeader);
  CPPUNIT_TEST(testAcceptMetalink);
  CPPUNIT_TEST(testEnableAcceptEncoding);
  CPPUNIT_TEST(testConditionalRequest);
  CPPUNIT_TEST(testScopeCredentialHeaders_noRedirect);
  CPPUNIT_TEST(testScopeCredentialHeaders_requestOrigin);
  CPPUNIT_TEST(testScopeCredentialHeaders_origin);
  CPPUNIT_TEST(testScopeCredentialHeaders_scheme);
  CPPUNIT_TEST(testScopeCredentialHeaders_headerNames);
  CPPUNIT_TEST(testScopeCredentialHeaders_unparsableUri);
  CPPUNIT_TEST(testCreateRequest_credentialScope);
  CPPUNIT_TEST_SUITE_END();

private:
  std::unique_ptr<Option> option_;
  std::unique_ptr<AuthConfigFactory> authConfigFactory_;

public:
  void setUp()
  {
    option_.reset(new Option());
    option_->put(PREF_HTTP_AUTH_CHALLENGE, A2_V_TRUE);
    authConfigFactory_.reset(new AuthConfigFactory());
  }

  void testGetStartByte();
  void testGetEndByte();
  void testCreateRequest();
  void testCreateRequest_ftp();
  void testCreateRequest_with_cookie();
  void testCreateRequest_query();
  void testCreateRequest_head();
  void testCreateRequest_ipv6LiteralAddr();
  void testCreateRequest_endOffsetOverride();
  void testCreateRequest_wantDigest();
  void testCreateProxyRequest();
  void testIsRangeSatisfied();
  void testUserAgent();
  void testAddHeader();
  void testAcceptMetalink();
  void testEnableAcceptEncoding();
  void testConditionalRequest();
  void testScopeCredentialHeaders_noRedirect();
  void testScopeCredentialHeaders_requestOrigin();
  void testScopeCredentialHeaders_origin();
  void testScopeCredentialHeaders_scheme();
  void testScopeCredentialHeaders_headerNames();
  void testScopeCredentialHeaders_unparsableUri();
  void testCreateRequest_credentialScope();
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpRequestTest);

void HttpRequestTest::testGetStartByte()
{
  HttpRequest httpRequest;
  auto p = std::make_shared<Piece>(1, 1_k);
  auto segment = std::make_shared<PiecedSegment>(1_k, p);
  auto fileEntry = std::make_shared<FileEntry>("file", 10_k, 0);

  CPPUNIT_ASSERT_EQUAL((int64_t)0LL, httpRequest.getStartByte());

  httpRequest.setSegment(segment);
  httpRequest.setFileEntry(fileEntry);

  CPPUNIT_ASSERT_EQUAL((int64_t)1_k, httpRequest.getStartByte());
}

void HttpRequestTest::testGetEndByte()
{
  size_t index = 1;
  size_t length = 1_m - 1_k;
  size_t segmentLength = 1_m;

  HttpRequest httpRequest;
  auto piece = std::make_shared<Piece>(index, length);
  auto segment = std::make_shared<PiecedSegment>(segmentLength, piece);
  auto fileEntry = std::make_shared<FileEntry>("file", segmentLength * 10, 0);

  CPPUNIT_ASSERT_EQUAL((int64_t)0LL, httpRequest.getEndByte());

  httpRequest.setSegment(segment);

  CPPUNIT_ASSERT_EQUAL((int64_t)0LL, httpRequest.getEndByte());

  auto request = std::make_shared<Request>();
  request->supportsPersistentConnection(true);
  request->setPipeliningHint(true);

  httpRequest.setRequest(request);
  httpRequest.setFileEntry(fileEntry);

  CPPUNIT_ASSERT_EQUAL((int64_t)(segmentLength * index + length - 1),
                       httpRequest.getEndByte());

  // The end byte of FileEntry are placed inside segment
  fileEntry->setLength(segmentLength + 100);

  CPPUNIT_ASSERT_EQUAL((int64_t)(segmentLength * index + 100 - 1),
                       httpRequest.getEndByte());

  request->setPipeliningHint(false);

  CPPUNIT_ASSERT_EQUAL((int64_t)0LL, httpRequest.getEndByte());
}

void HttpRequestTest::testCreateRequest()
{
  auto request = std::make_shared<Request>();
  request->supportsPersistentConnection(true);
  request->setUri("http://localhost:8080/archives/aria2-1.0.0.tar.bz2");

  auto p = std::make_shared<Piece>(0, 1_k);
  auto segment = std::make_shared<PiecedSegment>(1_k, p);
  auto fileEntry = std::make_shared<FileEntry>("file", 10_m, 0);

  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setSegment(segment);
  httpRequest.setFileEntry(fileEntry);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());
  httpRequest.setNoWantDigest(true);

  // remove "Connection: close" and add end byte range
  request->setPipeliningHint(true);

  std::string expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                             "User-Agent: aria2\r\n"
                             "Accept: */*\r\n"
                             "Host: localhost:8080\r\n"
                             "Pragma: no-cache\r\n"
                             "Cache-Control: no-cache\r\n"
                             "Range: bytes=0-1023\r\n"
                             "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setPipeliningHint(false);

  expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: localhost:8080\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Connection: close\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  p.reset(new Piece(1, 1_m));
  segment.reset(new PiecedSegment(1_m, p));
  httpRequest.setSegment(segment);

  expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: localhost:8080\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Connection: close\r\n"
                 "Range: bytes=1048576-\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setPipeliningHint(true);

  expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: localhost:8080\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Range: bytes=1048576-2097151\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  // redirection set persistent connection flag to true
  request->redirectUri(
      "http://localhost:8080/archives/download/aria2-1.0.0.tar.bz2");

  expectedText = "GET /archives/download/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: localhost:8080\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Range: bytes=1048576-2097151\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->supportsPersistentConnection(true);
  request->setPipeliningHint(false);

  // this only removes "Connection: close".
  request->setKeepAliveHint(true);

  expectedText = "GET /archives/download/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: localhost:8080\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Range: bytes=1048576-\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setKeepAliveHint(false);

  request->resetUri();

  p.reset(new Piece(0, 1_m));
  segment.reset(new PiecedSegment(1_m, p));
  httpRequest.setSegment(segment);

  // enable http auth
  option_->put(PREF_HTTP_USER, "aria2user");
  option_->put(PREF_HTTP_PASSWD, "aria2passwd");

  CPPUNIT_ASSERT(authConfigFactory_->activateBasicCred("localhost", 8080, "/",
                                                       option_.get()));

  expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: localhost:8080\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Connection: close\r\n"
                 "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  // enable http proxy auth
  auto proxyRequest = std::make_shared<Request>();
  CPPUNIT_ASSERT(proxyRequest->setUri(
      "http://aria2proxyuser:aria2proxypasswd@localhost:9000"));
  httpRequest.setProxyRequest(proxyRequest);

  expectedText =
      "GET http://localhost:8080/archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
      "User-Agent: aria2\r\n"
      "Accept: */*\r\n"
      "Host: localhost:8080\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "Connection: close\r\n"
      "Proxy-Authorization: Basic "
      "YXJpYTJwcm94eXVzZXI6YXJpYTJwcm94eXBhc3N3ZA==\r\n"
      "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
      "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setPipeliningHint(true);

  expectedText =
      "GET http://localhost:8080/archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
      "User-Agent: aria2\r\n"
      "Accept: */*\r\n"
      "Host: localhost:8080\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "Range: bytes=0-1048575\r\n"
      "Connection: Keep-Alive\r\n"
      "Proxy-Authorization: Basic "
      "YXJpYTJwcm94eXVzZXI6YXJpYTJwcm94eXBhc3N3ZA==\r\n"
      "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
      "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setPipeliningHint(false);

  // turn off proxy auth
  CPPUNIT_ASSERT(proxyRequest->setUri("http://localhost:9000"));

  expectedText =
      "GET http://localhost:8080/archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
      "User-Agent: aria2\r\n"
      "Accept: */*\r\n"
      "Host: localhost:8080\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "Connection: close\r\n"
      "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
      "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testCreateRequest_ftp()
{
  option_->put(PREF_FTP_USER, "aria2user");
  option_->put(PREF_FTP_PASSWD, "aria2passwd");

  auto request = std::make_shared<Request>();
  request->setUri("ftp://localhost:8080/archives/aria2-1.0.0.tar.bz2");

  auto proxyRequest = std::make_shared<Request>();
  CPPUNIT_ASSERT(proxyRequest->setUri("http://localhost:9000"));

  HttpRequest httpRequest;
  auto p = std::make_shared<Piece>(0, 1_m);
  auto segment = std::make_shared<PiecedSegment>(1_m, p);
  auto fileEntry = std::make_shared<FileEntry>("file", 10_m, 0);

  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setSegment(segment);
  httpRequest.setFileEntry(fileEntry);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());
  httpRequest.setProxyRequest(proxyRequest);
  httpRequest.setNoWantDigest(true);

  std::string expectedText =
      "GET ftp://aria2user@localhost:8080/archives/aria2-1.0.0.tar.bz2"
      " HTTP/1.1\r\n"
      "User-Agent: aria2\r\n"
      "Accept: */*\r\n"
      "Host: localhost:8080\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "Connection: close\r\n"
      "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
      "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  // test proxy authorization
  CPPUNIT_ASSERT(proxyRequest->setUri(
      "http://aria2proxyuser:aria2proxypasswd@localhost:9000"));

  expectedText =
      "GET ftp://aria2user@localhost:8080/archives/aria2-1.0.0.tar.bz2"
      " HTTP/1.1\r\n"
      "User-Agent: aria2\r\n"
      "Accept: */*\r\n"
      "Host: localhost:8080\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "Connection: close\r\n"
      "Proxy-Authorization: Basic "
      "YXJpYTJwcm94eXVzZXI6YXJpYTJwcm94eXBhc3N3ZA==\r\n"
      "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
      "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

template <typename InputIterator>
void foo(CookieStorage& st, InputIterator first, InputIterator last, time_t t)
{
  for (; first != last; ++first) {
    st.store(*first, t);
  }
}
void HttpRequestTest::testCreateRequest_with_cookie()
{
  auto request = std::make_shared<Request>();
  request->setUri("http://localhost/archives/aria2-1.0.0.tar.bz2");
  auto p = std::make_shared<Piece>(0, 1_m);
  auto segment = std::make_shared<PiecedSegment>(1_m, p);
  auto fileEntry = std::make_shared<FileEntry>("file", 10_m, 0);

  auto st = CookieStorage{};
  CPPUNIT_ASSERT(st.store(
      createCookie("name1", "value1", "localhost", true, "/archives", false),
      0));
  CPPUNIT_ASSERT(st.store(createCookie("name2", "value2", "localhost", true,
                                       "/archives/download", false),
                          0));
  CPPUNIT_ASSERT(st.store(createCookie("name3", "value3", "aria2.org", false,
                                       "/archives/download", false),
                          0));
  CPPUNIT_ASSERT(st.store(
      createCookie("name4", "value4", "aria2.org", false, "/archives/", true),
      0));
  CPPUNIT_ASSERT(st.store(
      createCookie("name5", "value5", "example.org", false, "/", false), 0));

  HttpRequest httpRequest;

  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setSegment(segment);
  httpRequest.setFileEntry(fileEntry);
  httpRequest.setCookieStorage(&st);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());
  httpRequest.setNoWantDigest(true);

  std::string expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                             "User-Agent: aria2\r\n"
                             "Accept: */*\r\n"
                             "Host: localhost\r\n"
                             "Pragma: no-cache\r\n"
                             "Cache-Control: no-cache\r\n"
                             "Connection: close\r\n"
                             "Cookie: name1=value1;\r\n"
                             "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setUri("http://localhost/archives/download/aria2-1.0.0.tar.bz2");

  expectedText = "GET /archives/download/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: localhost\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Connection: close\r\n"
                 "Cookie: name2=value2;name1=value1;\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setUri("http://www.aria2.org/archives/download/aria2-1.0.0.tar.bz2");

  expectedText = "GET /archives/download/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: www.aria2.org\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Connection: close\r\n"
                 "Cookie: name3=value3;\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setUri("https://www.aria2.org/archives/download/"
                  "aria2-1.0.0.tar.bz2");

  expectedText = "GET /archives/download/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: www.aria2.org\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Connection: close\r\n"
                 "Cookie: name3=value3;name4=value4;\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  // The path of cookie4 ends with '/'
  request->setUri("https://www.aria2.org/archives/aria2-1.0.0.tar.bz2");

  expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: www.aria2.org\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Connection: close\r\n"
                 "Cookie: name4=value4;\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setUri("http://example.org/aria2-1.0.0.tar.bz2");

  expectedText = "GET /aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: example.org\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Connection: close\r\n"
                 "Cookie: name5=value5;\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testCreateRequest_query()
{
  auto request = std::make_shared<Request>();
  request->setUri(
      "http://localhost/wiki?id=9ad5109a-b8a5-4edf-9373-56a1c34ae138");
  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());
  httpRequest.setNoWantDigest(true);

  std::string expectedText =
      "GET /wiki?id=9ad5109a-b8a5-4edf-9373-56a1c34ae138 HTTP/1.1\r\n"
      "User-Agent: aria2\r\n"
      "Accept: */*\r\n"
      "Host: localhost\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "Connection: close\r\n"
      "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testCreateRequest_head()
{
  auto request = std::make_shared<Request>();
  request->setMethod(Request::METHOD_HEAD);
  request->setUri("http://localhost/aria2-1.0.0.tar.bz2");

  HttpRequest httpRequest;
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());

  std::stringstream result(httpRequest.createRequest());
  std::string line;
  CPPUNIT_ASSERT(getline(result, line));
  line = util::strip(line);
  CPPUNIT_ASSERT_EQUAL(std::string("HEAD /aria2-1.0.0.tar.bz2 HTTP/1.1"), line);
}

void HttpRequestTest::testCreateRequest_endOffsetOverride()
{
  auto request = std::make_shared<Request>();
  request->setUri("http://localhost/myfile");
  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());
  httpRequest.setNoWantDigest(true);
  auto p = std::make_shared<Piece>(0, 1_m);
  auto segment = std::make_shared<PiecedSegment>(1_m, p);
  httpRequest.setSegment(segment);
  httpRequest.setEndOffsetOverride(1_g);
  auto fileEntry = std::make_shared<FileEntry>("file", 10_g, 0);
  httpRequest.setFileEntry(fileEntry);
  // End byte is passed if it is not 0
  std::string expectedText = "GET /myfile HTTP/1.1\r\n"
                             "User-Agent: aria2\r\n"
                             "Accept: */*\r\n"
                             "Host: localhost\r\n"
                             "Pragma: no-cache\r\n"
                             "Cache-Control: no-cache\r\n"
                             "Connection: close\r\n"
                             "Range: bytes=0-1073741823\r\n"
                             "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  segment->updateWrittenLength(1);

  expectedText = "GET /myfile HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Accept: */*\r\n"
                 "Host: localhost\r\n"
                 "Pragma: no-cache\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Connection: close\r\n"
                 "Range: bytes=1-1073741823\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testCreateRequest_wantDigest()
{
  auto request = std::make_shared<Request>();
  request->setUri("http://localhost/");
  HttpRequest httpRequest;
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());

  std::string wantDigest;
  if (MessageDigest::supports("sha-512")) {
    wantDigest += "SHA-512;q=1, ";
  }
  if (MessageDigest::supports("sha-256")) {
    wantDigest += "SHA-256;q=1, ";
  }
  if (MessageDigest::supports("sha-1")) {
    wantDigest += "SHA;q=0.1, ";
  }
  if (!wantDigest.empty()) {
    wantDigest.erase(wantDigest.size() - 2);
  }

  std::string expectedText = "GET / HTTP/1.1\r\n"
                             "User-Agent: aria2\r\n"
                             "Accept: */*\r\n"
                             "Host: localhost\r\n"
                             "Pragma: no-cache\r\n"
                             "Cache-Control: no-cache\r\n"
                             "Connection: close\r\n"
                             "Want-Digest: " +
                             wantDigest +
                             "\r\n"
                             "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testCreateProxyRequest()
{
  auto request = std::make_shared<Request>();
  request->setUri("http://localhost/archives/aria2-1.0.0.tar.bz2");
  auto p = std::make_shared<Piece>(0, 1_m);
  auto segment = std::make_shared<PiecedSegment>(1_m, p);

  auto proxyRequest = std::make_shared<Request>();
  CPPUNIT_ASSERT(proxyRequest->setUri("http://localhost:9000"));

  HttpRequest httpRequest;

  httpRequest.setRequest(request);
  httpRequest.setSegment(segment);
  httpRequest.setProxyRequest(proxyRequest);

  request->supportsPersistentConnection(true);

  std::string expectedText = "CONNECT localhost:80 HTTP/1.1\r\n"
                             "User-Agent: aria2\r\n"
                             "Host: localhost:80\r\n"
                             //"Proxy-Connection: close\r\n"
                             "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createProxyRequest());

  // adds Keep-Alive header.
  request->setKeepAliveHint(true);

  expectedText = "CONNECT localhost:80 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Host: localhost:80\r\n"
                 //"Proxy-Connection: Keep-Alive\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createProxyRequest());

  request->setKeepAliveHint(false);
  // pipelining also adds Keep-Alive header.
  request->setPipeliningHint(true);

  expectedText = "CONNECT localhost:80 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Host: localhost:80\r\n"
                 //"Proxy-Connection: Keep-Alive\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createProxyRequest());

  // test proxy authorization
  CPPUNIT_ASSERT(proxyRequest->setUri(
      "http://aria2proxyuser:aria2proxypasswd@localhost:9000"));

  expectedText = "CONNECT localhost:80 HTTP/1.1\r\n"
                 "User-Agent: aria2\r\n"
                 "Host: localhost:80\r\n"
                 //"Proxy-Connection: Keep-Alive\r\n"
                 "Proxy-Authorization: Basic "
                 "YXJpYTJwcm94eXVzZXI6YXJpYTJwcm94eXBhc3N3ZA==\r\n"
                 "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createProxyRequest());
}

void HttpRequestTest::testIsRangeSatisfied()
{
  auto request = std::make_shared<Request>();
  request->supportsPersistentConnection(true);
  request->setUri("http://localhost:8080/archives/aria2-1.0.0.tar.bz2");
  request->setPipeliningHint(false); // default: false
  auto p = std::make_shared<Piece>(0, 1_m);
  auto segment = std::make_shared<PiecedSegment>(1_m, p);
  auto fileEntry = std::make_shared<FileEntry>("file", 0, 0);

  HttpRequest httpRequest;

  httpRequest.setRequest(request);
  httpRequest.setSegment(segment);
  httpRequest.setFileEntry(fileEntry);

  Range range;

  CPPUNIT_ASSERT(httpRequest.isRangeSatisfied(range));

  p.reset(new Piece(1, 1_m));
  segment.reset(new PiecedSegment(1_m, p));
  httpRequest.setSegment(segment);

  CPPUNIT_ASSERT(!httpRequest.isRangeSatisfied(range));

  uint64_t entityLength = segment->getSegmentLength() * 10;

  range = Range(segment->getPosition(), 0, entityLength);

  CPPUNIT_ASSERT(httpRequest.isRangeSatisfied(range));

  fileEntry->setLength(entityLength - 1);

  CPPUNIT_ASSERT(!httpRequest.isRangeSatisfied(range));

  fileEntry->setLength(entityLength);

  CPPUNIT_ASSERT(httpRequest.isRangeSatisfied(range));

  request->setPipeliningHint(true);

  CPPUNIT_ASSERT(!httpRequest.isRangeSatisfied(range));

  range =
      Range(segment->getPosition(),
            segment->getPosition() + segment->getLength() - 1, entityLength);

  CPPUNIT_ASSERT(httpRequest.isRangeSatisfied(range));

  range = Range(segment->getPosition(),
                segment->getPosition() + segment->getLength() - 1, 0);

  CPPUNIT_ASSERT(!httpRequest.isRangeSatisfied(range));

  range =
      Range(0, segment->getPosition() + segment->getLength() - 1, entityLength);

  CPPUNIT_ASSERT(!httpRequest.isRangeSatisfied(range));
}

void HttpRequestTest::testUserAgent()
{
  auto request = std::make_shared<Request>();
  request->setUri("http://localhost:8080/archives/aria2-1.0.0.tar.bz2");

  // std::shared_ptr<Piece> p(new Piece(0, 1_k));
  // std::shared_ptr<Segment> segment(new PiecedSegment(1_k, p));

  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  // httpRequest.setSegment(segment);
  httpRequest.setUserAgent("aria2 (Linux)");
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());
  httpRequest.setNoWantDigest(true);

  std::string expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                             "User-Agent: aria2 (Linux)\r\n"
                             "Accept: */*\r\n"
                             "Host: localhost:8080\r\n"
                             "Pragma: no-cache\r\n"
                             "Cache-Control: no-cache\r\n"
                             "Connection: close\r\n"
                             "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  auto proxyRequest = std::make_shared<Request>();
  CPPUNIT_ASSERT(proxyRequest->setUri("http://localhost:9000"));

  httpRequest.setProxyRequest(proxyRequest);

  std::string expectedTextForProxy = "CONNECT localhost:8080 HTTP/1.1\r\n"
                                     "User-Agent: aria2 (Linux)\r\n"
                                     "Host: localhost:8080\r\n"
                                     //"Proxy-Connection: close\r\n"
                                     "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedTextForProxy, httpRequest.createProxyRequest());
}

void HttpRequestTest::testAddHeader()
{
  auto request = std::make_shared<Request>();
  request->setUri("http://localhost/archives/aria2-1.0.0.tar.bz2");

  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());
  httpRequest.addHeader("X-ARIA2: v0.13\nX-ARIA2-DISTRIBUTE: enabled\n");
  httpRequest.addHeader("Accept: text/html");
  httpRequest.setNoWantDigest(true);
  std::string expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
                             "User-Agent: aria2\r\n"
                             "Host: localhost\r\n"
                             "Pragma: no-cache\r\n"
                             "Cache-Control: no-cache\r\n"
                             "Connection: close\r\n"
                             "X-ARIA2: v0.13\r\n"
                             "X-ARIA2-DISTRIBUTE: enabled\r\n"
                             "Accept: text/html\r\n"
                             "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testAcceptMetalink()
{
  auto request = std::make_shared<Request>();
  request->setUri("http://localhost/archives/aria2-1.0.0.tar.bz2");

  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());
  httpRequest.setAcceptMetalink(true);
  httpRequest.setNoWantDigest(true);

  std::string expectedText =
      "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
      "User-Agent: aria2\r\n"
      "Accept: */*,application/metalink4+xml,application/metalink+xml\r\n"
      "Host: localhost\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "Connection: close\r\n"
      "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testEnableAcceptEncoding()
{
  auto request = std::make_shared<Request>();
  request->setUri("http://localhost/archives/aria2-1.0.0.tar.bz2");

  HttpRequest httpRequest;
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());
  httpRequest.setNoWantDigest(true);

  std::string acceptEncodings;
#ifdef HAVE_ZLIB
  acceptEncodings += "deflate, gzip";
#endif // HAVE_ZLIB

  std::string expectedTextHead =
      "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
      "User-Agent: aria2\r\n"
      "Accept: */*\r\n";
  std::string expectedTextTail = "Host: localhost\r\n"
                                 "Pragma: no-cache\r\n"
                                 "Cache-Control: no-cache\r\n"
                                 "Connection: close\r\n"
                                 "\r\n";

  std::string expectedText = expectedTextHead;
  expectedText += expectedTextTail;
  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  expectedText = expectedTextHead;
  if (!acceptEncodings.empty()) {
    expectedText += "Accept-Encoding: ";
    expectedText += acceptEncodings;
    expectedText += "\r\n";
  }
  expectedText += expectedTextTail;

  httpRequest.enableAcceptGZip();
  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testCreateRequest_ipv6LiteralAddr()
{
  auto request = std::make_shared<Request>();
  request->setUri("http://[::1]/path");
  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());

  CPPUNIT_ASSERT(httpRequest.createRequest().find("Host: [::1]") !=
                 std::string::npos);

  auto proxy = std::make_shared<Request>();
  proxy->setUri("http://proxy");
  httpRequest.setProxyRequest(proxy);
  std::string proxyRequest = httpRequest.createProxyRequest();
  CPPUNIT_ASSERT(proxyRequest.find("Host: [::1]:80") != std::string::npos);
  CPPUNIT_ASSERT(proxyRequest.find("CONNECT [::1]:80 ") != std::string::npos);
}

void HttpRequestTest::testConditionalRequest()
{
  HttpRequest httpRequest;
  CPPUNIT_ASSERT(!httpRequest.conditionalRequest());
  httpRequest.setIfModifiedSinceHeader("dummy");
  CPPUNIT_ASSERT(httpRequest.conditionalRequest());
  httpRequest.setIfModifiedSinceHeader("");
  CPPUNIT_ASSERT(!httpRequest.conditionalRequest());
  httpRequest.addHeader("If-None-Match: *");
  CPPUNIT_ASSERT(httpRequest.conditionalRequest());
  httpRequest.clearHeader();
  httpRequest.addHeader("If-Modified-Since: dummy");
  CPPUNIT_ASSERT(httpRequest.conditionalRequest());
}

namespace {
const std::string HEADERS = "Referer: https://www.example.com/page\n"
                            "Cookie: session=s3cr3t\n"
                            "X-Foo: bar\n"
                            "Authorization: Bearer t0ken\n";

const std::string HEADERS_WITHOUT_CREDENTIALS =
    "Referer: https://www.example.com/page\n"
    "X-Foo: bar\n";

std::string scopeRedirect(const std::string& uri, const std::string& target,
                          const std::string& headers = HEADERS)
{
  Request req;
  CPPUNIT_ASSERT(req.setUri(uri));
  CPPUNIT_ASSERT(req.redirectUri(target));
  return scopeCredentialHeaders(headers, req);
}
} // namespace

void HttpRequestTest::testScopeCredentialHeaders_noRedirect()
{
  for (const auto& uri :
       {"https://dl.example.com/file.zip", "http://dl.example.com/file.zip",
        "https://mirror.other.net/file.zip", "http://10.0.0.1/file.zip",
        "https://[2001:db8::1]/file.zip", "http://localhost/file.zip"}) {
    Request req;
    CPPUNIT_ASSERT(req.setUri(uri));
    CPPUNIT_ASSERT_EQUAL(HEADERS, scopeCredentialHeaders(HEADERS, req));
  }
}

void HttpRequestTest::testScopeCredentialHeaders_requestOrigin()
{
  // One download's URIs on two origins: each Request keeps them on its URI.
  Request first;
  CPPUNIT_ASSERT(first.setUri("https://www.example.com/file.zip"));
  Request second;
  CPPUNIT_ASSERT(second.setUri("https://mirror.other.net/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS, scopeCredentialHeaders(HEADERS, first));
  CPPUNIT_ASSERT_EQUAL(HEADERS, scopeCredentialHeaders(HEADERS, second));
  // Bound by the Request's own URI, not by another URI of the download.
  CPPUNIT_ASSERT(second.redirectUri("https://www.example.com/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeCredentialHeaders(HEADERS, second));
  CPPUNIT_ASSERT(first.redirectUri("https://www.example.com/files/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS, scopeCredentialHeaders(HEADERS, first));

  // Checked on every request: a later segment or pooled reuse of the
  // redirected Request drops them again; resetUri() (a retry) returns to
  // the Request's URI and keeps them.
  Request req;
  CPPUNIT_ASSERT(req.setUri("https://dl.example.com/file.zip"));
  CPPUNIT_ASSERT(req.redirectUri("https://attacker.net/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeCredentialHeaders(HEADERS, req));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeCredentialHeaders(HEADERS, req));
  CPPUNIT_ASSERT(req.resetUri());
  CPPUNIT_ASSERT_EQUAL(HEADERS, scopeCredentialHeaders(HEADERS, req));
  CPPUNIT_ASSERT(req.redirectUri("https://attacker.net/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeCredentialHeaders(HEADERS, req));
  CPPUNIT_ASSERT(req.redirectUri("https://dl.example.com/files/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS, scopeCredentialHeaders(HEADERS, req));
}

void HttpRequestTest::testScopeCredentialHeaders_origin()
{
  CPPUNIT_ASSERT_EQUAL(HEADERS,
                       scopeRedirect("https://dl.example.com/file.zip",
                                     "https://dl.example.com/files/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS,
                       scopeRedirect("https://DL.Example.COM/file.zip",
                                     "https://dl.example.com./file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS,
                       scopeRedirect("HTTPS://dl.example.com:443/file.zip",
                                     "https://dl.example.com/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS,
                       scopeRedirect("https://dl.example.com:443/file.zip",
                                     "https://dl.example.com/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS,
                       scopeRedirect("https://dl.example.com/file.zip",
                                     "https://dl.example.com:443/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS,
                       scopeRedirect("http://dl.example.com/file.zip",
                                     "http://dl.example.com:80/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS, scopeRedirect("http://10.0.0.1/file.zip",
                                              "http://10.0.0.1/f/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS,
                       scopeRedirect("https://[2001:db8::1]/file.zip",
                                     "https://[2001:db8::1]/f/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://www.example.com/file.zip",
                                     "https://dl.example.com/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://example.com/file.zip",
                                     "https://cdn.dl.example.com/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://members.example.co.uk/file.zip",
                                     "https://dl.example.co.uk/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://customer.co.uk/file.zip",
                                     "https://attacker.co.uk/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://a.github.io/file.zip",
                                     "https://b.github.io/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://example.com/file.zip",
                                     "https://other.net/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://dl.example.com/file.zip",
                                     "https://dl.example.com.other.net/f"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("http://10.0.0.1/file.zip",
                                     "http://10.0.0.2/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://example.com/file.zip",
                                     "https://[2001:db8::1]/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("http://localhost/file.zip",
                                     "http://otherhost/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://dl.example.com/file.zip",
                                     "https://dl.example.com:8443/file.zip"));
}

void HttpRequestTest::testScopeCredentialHeaders_scheme()
{
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://dl.example.com/file.zip",
                                     "http://dl.example.com/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://www.example.com/file.zip",
                                     "http://dl.example.com/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("http://dl.example.com/file.zip",
                                     "https://dl.example.com/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://dl.example.com/file.zip",
                                     "https://dl.example.com:8443/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("http://dl.example.com:8080/file.zip",
                                     "https://dl.example.com:8080/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://dl.example.com:8080/file.zip",
                                     "http://dl.example.com:8080/file.zip"));
  // Each request is checked against the Request's URI, so https after an
  // http hop keeps them.
  Request req;
  CPPUNIT_ASSERT(req.setUri("https://dl.example.com/file.zip"));
  CPPUNIT_ASSERT(req.redirectUri("http://dl.example.com/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeCredentialHeaders(HEADERS, req));
  CPPUNIT_ASSERT(req.redirectUri("https://dl.example.com/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS, scopeCredentialHeaders(HEADERS, req));
}

void HttpRequestTest::testScopeCredentialHeaders_headerNames()
{
  std::string headers = "cookie: a=1\n"
                        "AUTHORIZATION: Basic dXNlcjpwYXNz\n"
                        "CoOkIe : b=2\n"
                        "  Authorization: Bearer t0ken\r\n"
                        "Cookie2: $Version=1\n"
                        "X-Cookie: c=3\n"
                        "X-Authorization-Hint: none\n"
                        "Proxy-Authorization: Basic cHJveHk6cGFzcw==\n"
                        "referer: https://www.example.com/page\n"
                        "X-Foo: bar\n";
  std::string kept = "Cookie2: $Version=1\n"
                     "X-Cookie: c=3\n"
                     "X-Authorization-Hint: none\n"
                     "Proxy-Authorization: Basic cHJveHk6cGFzcw==\n"
                     "referer: https://www.example.com/page\n"
                     "X-Foo: bar\n";
  CPPUNIT_ASSERT_EQUAL(kept,
                       scopeRedirect("https://dl.example.com/file.zip",
                                     "https://other.net/file.zip", headers));
  CPPUNIT_ASSERT_EQUAL(headers,
                       scopeRedirect("https://dl.example.com/file.zip",
                                     "https://dl.example.com/files/file.zip",
                                     headers));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeRedirect("https://dl.example.com/file.zip",
                                     "http://other.net/file.zip",
                                     HEADERS_WITHOUT_CREDENTIALS));
  CPPUNIT_ASSERT_EQUAL(std::string(),
                       scopeRedirect("https://dl.example.com/file.zip",
                                     "http://other.net/file.zip",
                                     std::string()));
}

void HttpRequestTest::testScopeCredentialHeaders_unparsableUri()
{
  Request empty;
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeCredentialHeaders(HEADERS, empty));
  Request req;
  CPPUNIT_ASSERT(!req.setUri("dl.example.com"));
  CPPUNIT_ASSERT(req.redirectUri("https://dl.example.com/file.zip"));
  CPPUNIT_ASSERT_EQUAL(HEADERS_WITHOUT_CREDENTIALS,
                       scopeCredentialHeaders(HEADERS, req));
}

void HttpRequestTest::testCreateRequest_credentialScope()
{
  for (const auto& target : {"https://www.example.com/files/file.zip",
                             "https://dl.example.com/file.zip",
                             "https://cdn.other.net/file.zip"}) {
    auto request = std::make_shared<Request>();
    CPPUNIT_ASSERT(request->setUri("https://www.example.com/file.zip"));
    CPPUNIT_ASSERT(request->redirectUri(target));

    HttpRequest httpRequest;
    httpRequest.disableContentEncoding();
    httpRequest.setRequest(request);
    httpRequest.setAuthConfigFactory(authConfigFactory_.get());
    httpRequest.setOption(option_.get());
    httpRequest.setNoWantDigest(true);
    httpRequest.addHeader(scopeCredentialHeaders(HEADERS, *request));

    bool sameOrigin = request->getHost() == "www.example.com";
    auto text = httpRequest.createRequest();
    auto has = [&text](const std::string& line) {
      return text.find("\r\n" + line + "\r\n") != std::string::npos;
    };
    CPPUNIT_ASSERT_EQUAL(sameOrigin, has("Cookie: session=s3cr3t"));
    CPPUNIT_ASSERT_EQUAL(sameOrigin, has("Authorization: Bearer t0ken"));
    CPPUNIT_ASSERT(has("Referer: https://www.example.com/page"));
    CPPUNIT_ASSERT(has("X-Foo: bar"));
  }
}

} // namespace aria2
