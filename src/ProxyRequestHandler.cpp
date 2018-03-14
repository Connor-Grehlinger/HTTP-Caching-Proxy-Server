
#include "logging/aixlog.hpp"
#include "ProxyRequestHandler.h"

#include <iostream>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <cstdlib>
#include <sstream>
#include <utility>

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
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPServerSession.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/Exception.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/HTTPCookie.h>
#include<Poco/Timestamp.h>
#include<Poco/DateTimeFormatter.h>

using namespace Poco::Net;
using namespace Poco::Util;
using namespace std;

void ProxyRequestHandler::remoteReq(Poco::URI& uri, std::string path, HTTPServerRequest& req, HTTPServerResponse& resp) {
  HTTPClientSession session(uri.getHost(), uri.getPort());
  HTTPRequest proxy_req(req.getMethod(), path, HTTPMessage::HTTP_1_1);
  string key = this->staticCache.makeKey(uri);

  // send request normally
  LOG(INFO) << req.get("unique_id") << ": " << "Requesting \"" << uri.getHost() << path << " " << HTTPMessage::HTTP_1_1
  << "\" from " << uri.getHost() << std::endl;

  proxy_req.setContentType("text/html");
  session.sendRequest(proxy_req);

  // get response
  HTTPResponse proxy_resp;
  // create istream for session response
  istream &is = session.receiveResponse(proxy_resp);

  LOG(INFO) << req.get("unique_id") << ": " << "Received \"" << proxy_resp.getVersion() << " " << proxy_resp.getStatus() << " " << proxy_resp.getReason()
  << "\" from " << uri.getHost() << std::endl;

  // copies cookies, headers, etc
  ProxyServerCache::copyResponseObj(proxy_resp, resp);

  ostringstream oss;
  oss << is.rdbuf();  // extract stream contents for caching
  if ((req.getMethod() == "GET") && (proxy_resp.getStatus() == 200)){
    // add if 200-OK resp to GET request

    if (ProxyServerCache::isCacheableResp(proxy_resp, req.get("unique_id"))){
      // filter out 'no-store' and 'private' responses
      LOG(TRACE) << "The response is cachable " << endl;

      this->staticCache.add(key, CacheResponse(proxy_resp, oss.str(), req.get("unique_id")));
      //LOG(INFO) << req.get("unique_id") << ": NOTE ETag: " << etag << std::endl;
    }
  }

  LOG(TRACE) << "Proxy resp: " << proxy_resp.getStatus() << " - " << proxy_resp.getReason()
  << std::endl;

  LOG(TRACE) << "Requesting url=" << uri.getHost() << std::endl
  << "port=" << uri.getPort() << endl
  << "path=" << path << endl;

  LOG(INFO) << req.get("unique_id") << ": " << "Responding \"" << resp.getVersion() << " " << resp.getStatus() << " " << resp.getReason() << "\"" << endl;
  std::ostream& out = resp.send();
  // Copy HTTP stream to app server response stream
  out << oss.str(); //trying the below

}

void ProxyRequestHandler::handleRequest(HTTPServerRequest &req, HTTPServerResponse &resp)
{
  /************************************************/
  // Only using HTTP requests (no danger of HTTPS) //
  /************************************************/

  // Strong guarantee -- Either request is handled and expected response is returned to user for this particular HTTP request
  //                      or objects are unmodified and memory is not leaked, while solely this request fails silently
  try
  {
    // prepare uri
    Poco::URI uri(req.getURI());

    // prepare path
    string path(uri.getPathAndQuery());
    if (path.empty()) path = "/";

    string key = this->staticCache.makeKey(uri); // construct cache key from uri

    // log send request
    LOG(TRACE) << "Creating proxy session to " << uri.getHost() << std::endl;

    // POST only
    if(req.getMethod() == "POST") {
      // prepare session
      HTTPClientSession session(uri.getHost(), uri.getPort());
      HTTPRequest proxy_req(req.getMethod(), path, HTTPMessage::HTTP_1_1);

      LOG(TRACE) << "POST request to=" << uri.getHost() << std::endl;

      proxy_req.setContentType("application/x-www-form-urlencoded");
      proxy_req.setKeepAlive(true);
      proxy_req.setContentLength(req.getContentLength());

      LOG(TRACE) << "Post content length=" << proxy_req.getContentLength() << std::endl;

      // Copy over cookies from client's request to proxy request
      Poco::Net::NameValueCollection postCookies;
      req.getCookies(postCookies);
      proxy_req.setCookies(postCookies);

      LOG(INFO) << req.get("unique_id") << ": " << "Requesting \"" << uri.getHost() << path << " " << HTTPMessage::HTTP_1_1
      << "\" from " << uri.getHost() << std::endl;

      std::ostream& opost = session.sendRequest(proxy_req);

      //HTMLForm htmlBody(req, req.stream());
      Poco::StreamCopier::copyStream(req.stream(), opost);
      //htmlBody.write(opost);

      // get response
      HTTPResponse proxy_resp;
      // create istream for session response
      istream &is = session.receiveResponse(proxy_resp);
      LOG(INFO) << req.get("unique_id") << ": " << "Received \"" << proxy_resp.getVersion() << " " << proxy_resp.getStatus() << " " << proxy_resp.getReason()
      << "\" from " << uri.getHost() << std::endl;

      ProxyServerCache::copyResponseObj(proxy_resp, resp);

      LOG(INFO) << req.get("unique_id") << ": " << "Responding \"" << resp.getVersion() << " " << resp.getStatus() << " " << resp.getReason() << "\"" << endl;

      std::ostream& out = resp.send();
      // Copy HTTP stream to app server response stream
      Poco::StreamCopier::copyStream(is, out);

    }
    else {
      /* GET or other non-POST request method */

      // This is a GET request, so we may be interested in checking the cache
      if (req.getMethod() == "GET"){

        // check if the request has been cached
        Poco::SharedPtr<CacheResponse> checkResponse = this->staticCache.get(key);

        if (!checkResponse.isNull()){

          // returns whether item needs to be revalidated to validItem
	        bool validItem = (*checkResponse).isValidResponse(req.get("unique_id"));

          if (!validItem) {
            // update cacheitem depending on result of If-None-Match request

            bool itemInvalid = updateCacheItem(uri, path, req, resp, checkResponse);
            if (itemInvalid) {
              // return because a remote request was required (item was invalid after If-None-Match)
              return;
            }

            // otherwise, continue and retrieve from cache...

          }

          // writes to resp
          LOG(DEBUG) << "Copying back cached headers" << endl;
          (*checkResponse).getResponse(resp);
          std::ostream& out = resp.send();
          out << (*checkResponse).getResponseData().str();

          LOG(INFO) << req.get("unique_id") << ": " << "Responding \"" << resp.getVersion() << " " << resp.getStatus() << " " << resp.getReason() << "\"" << endl;

          LOG(DEBUG) << "Response is cached, responding with cached data" << endl;
          return;
        }
        else{

          LOG(INFO) << req.get("unique_id") << ": " << "not in cache" << endl;
          remoteReq(uri, path, req, resp);

        }
      } else {
          // Not a GET or POST request, simply attempt to contact the remote server with the user's request
          // May fail, but we make a strong exception guarantee
          // and hence we'll either return a valid response to the user or we'll return to the original state unaffected
          remoteReq(uri, path, req, resp);
      }

    }
  }
  catch (Poco::Exception &ex){

    LOG(DEBUG) << "Failed to get response from url=" << req.getURI() << std::endl
    << "method=" << req.getMethod() << std::endl
    << ex.what() << ": " << ex.message() << endl;

    resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
    LOG(INFO) << req.get("unique_id") << ": " << "Responding \"" << resp.getVersion() << " " << resp.getStatus() << " " << resp.getReason() << "\"" << endl;
  }

}

// returns true if the cache item requires a remote request
bool ProxyRequestHandler::updateCacheItem(Poco::URI uri, std::string path, HTTPServerRequest& req, HTTPServerResponse &resp, Poco::SharedPtr<CacheResponse> item) {

  HTTPResponse cacheResponseObj;
  (*item).getResponse(cacheResponseObj);
  string etag = "";
  string lm = "";

  if(cacheResponseObj.has("Etag")) {
    etag = cacheResponseObj.get("Etag");
    LOG(DEBUG) << "cached response has etag=" << etag << std::endl;
    HTTPClientSession session(uri.getHost(), uri.getPort());
    HTTPRequest ping_req(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
    ping_req.add("If-None-Match", etag);
    // send request normally
    session.sendRequest(ping_req);

    // get response
    HTTPResponse ping_resp;
    session.receiveResponse(ping_resp);

    LOG(INFO) << req.get("unique_id") << ": " << "NOTE sent If-None-Match request, received status " << ping_resp.getStatus() << std::endl;

    if (ping_resp.getStatus() == 304) {
      LOG(INFO) << req.get("unique_id") << ": " <<  "NOTE cached response validated" << std::endl;

      // update expiration timestamp
      (*item).validated(req.get("unique_id"));

      return false;
    }
  } else if (cacheResponseObj.has("Last-Modified")) {
    lm = cacheResponseObj.get("Last-Modified");
    HTTPClientSession session(uri.getHost(), uri.getPort());
    HTTPRequest ping_req(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
    ping_req.add("If-Modified-Since", lm);
    // send request normally
    session.sendRequest(ping_req);

    // get response
    HTTPResponse ping_resp;
    session.receiveResponse(ping_resp);

    LOG(INFO) << req.get("unique_id") << ": " << "NOTE sent If-Modified-Since request, received status " << ping_resp.getStatus() << std::endl;

    if (ping_resp.getStatus() == 304) {
      LOG(INFO) << req.get("unique_id") << ": " <<  "NOTE cached response validated" << std::endl;

      // update expiration timestamp
      (*item).validated(req.get("unique_id"));

      return false;
    }
  }

  remoteReq(uri, path, req, resp);
  return true;


}


ProxyRequestHandler::ProxyRequestHandler(){
  //LOG(TRACE) << "------ Created Proxy Request Handler ------" << endl;
}


ProxyRequestHandler::~ProxyRequestHandler() {

}

// Cache is a static member shared by all ProxyRequestHandler objects
ProxyServerCache ProxyRequestHandler::staticCache;
