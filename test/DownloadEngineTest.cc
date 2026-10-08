#include "DownloadEngine.h"

#include <memory>
#include <vector>

#include <cppunit/extensions/HelperMacros.h>

#include "SelectEventPoll.h"
#include "SocketCore.h"
#include "Request.h"
#include "fmt.h"
#include "a2functional.h"

namespace aria2 {

class DownloadEngineTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DownloadEngineTest);
  CPPUNIT_TEST(testPopPooledSocket_sameOrigin);
  CPPUNIT_TEST(testPopPooledSocket_otherHost);
  CPPUNIT_TEST(testPopPooledSocket_otherScheme);
  CPPUNIT_TEST(testPopPooledSocket_normalizedHost);
  CPPUNIT_TEST(testPopPooledSocket_ftpUnchanged);
  CPPUNIT_TEST_SUITE_END();

  std::unique_ptr<DownloadEngine> e_;
  std::unique_ptr<SocketCore> server_;
  uint16_t port_;
  // Server ends, kept open so a pooled client socket does not read EOF.
  std::vector<std::shared_ptr<SocketCore>> accepted_;

public:
  void setUp()
  {
    e_ = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
    server_ = make_unique<SocketCore>();
    server_->bind(0);
    server_->beginListen();
    server_->setBlockingMode();
    port_ = server_->getAddrInfo().port;
    accepted_.clear();
  }

  void tearDown()
  {
    accepted_.clear();
    server_.reset();
    e_.reset();
  }

  void testPopPooledSocket_sameOrigin();
  void testPopPooledSocket_otherHost();
  void testPopPooledSocket_otherScheme();
  void testPopPooledSocket_normalizedHost();
  void testPopPooledSocket_ftpUnchanged();

private:
  std::shared_ptr<SocketCore> connect()
  {
    auto client = std::make_shared<SocketCore>();
    client->establishConnection("localhost", port_);
    while (!client->isWritable(0))
      ;
    accepted_.push_back(server_->acceptConnection());
    return client;
  }

  std::shared_ptr<Request> createRequest(const std::string& scheme,
                                         const std::string& host)
  {
    auto req = std::make_shared<Request>();
    CPPUNIT_ASSERT(
        req->setUri(fmt("%s://%s:%u/file", scheme.c_str(), host.c_str(),
                        static_cast<unsigned int>(port_))));
    return req;
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DownloadEngineTest);

void DownloadEngineTest::testPopPooledSocket_sameOrigin()
{
  auto sock = connect();
  auto ipaddrs = std::vector<std::string>{sock->getPeerInfo().addr};
  auto reqB = createRequest("https", "b.example.com");

  e_->poolSocket(reqB, nullptr, sock);

  CPPUNIT_ASSERT(sock == e_->popPooledSocket(reqB, ipaddrs, port_));
  CPPUNIT_ASSERT(!e_->popPooledSocket(reqB, ipaddrs, port_));
}

void DownloadEngineTest::testPopPooledSocket_otherHost()
{
  auto sock = connect();
  auto ipaddrs = std::vector<std::string>{sock->getPeerInfo().addr};
  auto reqA = createRequest("https", "a.example.com");
  auto reqB = createRequest("https", "b.example.com");

  e_->poolSocket(reqB, nullptr, sock);

  CPPUNIT_ASSERT(!e_->popPooledSocket(reqA, ipaddrs, port_));
  CPPUNIT_ASSERT(sock == e_->popPooledSocket(reqB, ipaddrs, port_));
}

void DownloadEngineTest::testPopPooledSocket_otherScheme()
{
  auto sock = connect();
  auto ipaddrs = std::vector<std::string>{sock->getPeerInfo().addr};
  auto httpReq = createRequest("http", "b.example.com");
  auto httpsReq = createRequest("https", "b.example.com");

  e_->poolSocket(httpReq, nullptr, sock);

  CPPUNIT_ASSERT(!e_->popPooledSocket(httpsReq, ipaddrs, port_));
  CPPUNIT_ASSERT(sock == e_->popPooledSocket(httpReq, ipaddrs, port_));

  e_->poolSocket(httpsReq, nullptr, sock);

  CPPUNIT_ASSERT(!e_->popPooledSocket(httpReq, ipaddrs, port_));
  CPPUNIT_ASSERT(sock == e_->popPooledSocket(httpsReq, ipaddrs, port_));
}

void DownloadEngineTest::testPopPooledSocket_normalizedHost()
{
  auto sock = connect();
  auto ipaddrs = std::vector<std::string>{sock->getPeerInfo().addr};

  e_->poolSocket(createRequest("https", "B.Example.COM."), nullptr, sock);

  CPPUNIT_ASSERT(sock == e_->popPooledSocket(
                             createRequest("https", "b.example.com"), ipaddrs,
                             port_));
}

void DownloadEngineTest::testPopPooledSocket_ftpUnchanged()
{
  auto sock = connect();
  auto ipaddrs = std::vector<std::string>{sock->getPeerInfo().addr};
  auto ftpReq = createRequest("ftp", "b.example.com");
  auto httpReq = createRequest("http", "b.example.com");

  // FTP connections stay keyed by user and IP:port and never serve HTTP.
  e_->poolSocket(ftpReq, "anonymous", nullptr, sock, "/dir");

  CPPUNIT_ASSERT(!e_->popPooledSocket(httpReq, ipaddrs, port_));

  std::string options;
  CPPUNIT_ASSERT(sock == e_->popPooledSocket(options, ipaddrs, port_,
                                              "anonymous"));
  CPPUNIT_ASSERT_EQUAL(std::string("/dir"), options);
}

} // namespace aria2
