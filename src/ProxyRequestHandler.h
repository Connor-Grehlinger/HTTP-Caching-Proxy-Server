#include <Poco/Net/HTTPServerConnectionFactory.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/Net/SSLException.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/AcceptCertificateHandler.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/StreamCopier.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/Exception.h>
#include "ProxyServerCache.h"
#include <Poco/URI.h>

using namespace Poco::Net;


class ProxyRequestHandler : public HTTPRequestHandler
{
public:
  using HTTPRequestHandler::HTTPRequestHandler;

  //ProxyRequestHandler(ProxyServerCache* cache); // now using static member

  ProxyRequestHandler();

  ~ProxyRequestHandler();

  void remoteReq(Poco::URI& uri, std::string path, HTTPServerRequest& req, HTTPServerResponse& resp);
  bool updateCacheItem(Poco::URI uri, std::string path, HTTPServerRequest& req, HTTPServerResponse &resp, Poco::SharedPtr<CacheResponse> item);

  virtual void handleRequest(HTTPServerRequest &req, HTTPServerResponse &resp);


private:
  ProxyRequestHandler(const ProxyRequestHandler&);
  ProxyRequestHandler(const HTTPRequestHandler&);
  ProxyRequestHandler& operator = (const ProxyRequestHandler&);

  // Cache for the request handler (may have to move up hierarchy to
  // have caching span multiple request or even connections)

  //ProxyServerCache * requestCache; // defunct, no longer passing in from factory

  static ProxyServerCache staticCache;

  static int count;
};
