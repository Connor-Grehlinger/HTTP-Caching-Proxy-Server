#include <Poco/UniqueExpireCache.h>
#include <Poco/ExpirationDecorator.h>
#include <Poco/Exception.h>
#include <Poco/LRUCache.h>
#include <ctime>
#include "logging/aixlog.hpp"
#include <Poco/URI.h>
#include <Poco/Clock.h>
#include <Poco/Timestamp.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/Timespan.h>

// Reponse object that will be cached with an expiration timer (defuct, not useing ExpireCache)
typedef Poco::ExpirationDecorator<std::istream> ExpRespStream;


class CacheResponse{
private:
  //std::string responseData;
  std::ostringstream responseData;

  // initial response timestamp
  Poco::Timestamp responseTimestamp;
  Poco::Timestamp expireTimestamp;

  Poco::Net::HTTPResponse responseObj;
  Poco::Timespan maxFreshness;

  bool expired;
  bool isNoCache;
  bool mustRevalidate;
  std::string expiresStr;
  std::string Etag;
  std::string last_modified;

public:

  //CacheResponse(std::string respData, double maxFresh, bool exp);
  CacheResponse(const CacheResponse& rhs);

  CacheResponse(const Poco::Net::HTTPResponse& response, std::string respData, std::string unique_id);

  std::string getEtag();
  std::string getLastModified();
  bool getIsNoCache();

  bool isValidResponse(std::string unique_id);

  ~CacheResponse();

  std::ostringstream& getResponseData();
  void getResponse(Poco::Net::HTTPResponse& writeToResponse);

  std::string getResponseDataStr();

  bool isExpired();

  void setExpired(bool exp);

  double getTimeLeft();

  void setMaxFreshness(double newFreshness);

  Poco::Timespan getMaxFreshness();

  void validated(std::string unique_id);

};


class ProxyServerCache : public Poco::LRUCache<std::string, CacheResponse>{
private:


public:
  static void copyResponseObj(const Poco::Net::HTTPResponse &fromResp, Poco::Net::HTTPResponse &toResp);
  static void copyResponseObj(const Poco::Net::HTTPResponse &fromResp, Poco::Net::HTTPServerResponse &toResp);
  static std::map<std::string, std::string> getCacheControlHeaders(const Poco::Net::HTTPResponse& resp);

  static long getMaxAge(const Poco::Net::HTTPResponse& resp);

  static std::string getExpires(const Poco::Net::HTTPResponse& resp);

  static std::string getEtag(const Poco::Net::HTTPResponse& resp);

  static std::string getLastModified(const Poco::Net::HTTPResponse& resp);

  static bool isCacheableResp(const Poco::Net::HTTPResponse& resp, std::string id);

  static bool hasNoCacheDirective(const Poco::Net::HTTPResponse& resp);

  ProxyServerCache();

  ~ProxyServerCache();

  std::string makeKey(Poco::URI& targetURI);

};
