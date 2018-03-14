#include "ProxyServerCache.h"
#include <Poco/DateTimeParser.h>

using namespace Poco;
using namespace std;
using Poco::DateTimeFormat;

// constructor
ProxyServerCache::ProxyServerCache(){
}

// destructor
ProxyServerCache::~ProxyServerCache(){
}

std::string ProxyServerCache::makeKey(Poco::URI& uri){
  std::string key(uri.getHost());
  key.append(std::to_string(uri.getPort()));
  key.append(uri.getPathAndQuery());
  LOG(DEBUG) << "Constructed key = " << key << std::endl;
  return key;
}


void ProxyServerCache::copyResponseObj(const Poco::Net::HTTPResponse &fromResp, Poco::Net::HTTPResponse &toResp) {
  // Copy over headers
  Poco::Net::NameValueCollection::ConstIterator i = fromResp.begin();
  while(i!=fromResp.end()){
      //LOG(DEBUG) << "Copying over header=" <<  i->first << std::endl;
      toResp.add(i->first, i->second);
      //LOG(DEBUG) << "Copying over value=" <<  toResp.get(i->first) << std::endl;
      ++i;
  }

  // Copy over cookies from proxy's response to client response
  std::vector<Poco::Net::HTTPCookie> respCookies;
  fromResp.getCookies(respCookies);

  for (int i=0 ; i < respCookies.size(); i++) {
    Poco::Net::HTTPCookie cookie(respCookies[i]);
    toResp.addCookie(cookie);
  }

  toResp.setStatus(fromResp.getStatus());
  toResp.setContentType(fromResp.getContentType());
}

void ProxyServerCache::copyResponseObj(const Poco::Net::HTTPResponse &fromResp, Poco::Net::HTTPServerResponse &toResp) {
  // Copy over headers
  Poco::Net::NameValueCollection::ConstIterator i = fromResp.begin();
  while(i!=fromResp.end()){
      //LOG(DEBUG) << "Copying over header=" <<  i->first << std::endl;
      toResp.add(i->first, i->second);
      //LOG(DEBUG) << "Copying over value=" <<  toResp.get(i->first) << std::endl;
      ++i;
  }

  // Copy over cookies from proxy's response to client response
  std::vector<Poco::Net::HTTPCookie> respCookies;
  fromResp.getCookies(respCookies);

  for (int i=0 ; i < respCookies.size(); i++) {
    Poco::Net::HTTPCookie cookie(respCookies[i]);
    toResp.addCookie(cookie);
  }

  toResp.setStatus(fromResp.getStatus());
  toResp.setContentType(fromResp.getContentType());
}

/*
Expiration time is determined by more than the cache-control header
- may be easier to just return a struct with all the information the cache needs to
- consume
*/

std::map<std::string, std::string> ProxyServerCache::getCacheControlHeaders(const Poco::Net::HTTPResponse& resp){
  /*
  if (resp.has("cacheControlHeaders")){
    return resp.get("cacheControlHeaders");
  } */

  map<std::string, std::string> ret;
  if (resp.has("Cache-Control")) {
    std::string cacheHeaders = resp.get("Cache-Control");
    LOG(DEBUG) << "cache-control headers=" << cacheHeaders << std::endl;
    if(cacheHeaders == "") {
      return ret;
    }

    std::stringstream ss(cacheHeaders);
    while (ss.good())
    {
        while (ss.peek() == ' ') {// skip spaces
        ss.get();
        }

        std::string substr;
        getline(ss, substr, ',');
        //std::remove_if(substr.begin(), substr.end(), ::isspace);

        std::size_t current = substr.find("=");
        if(current != std::string::npos) {
          LOG(DEBUG) << "CC header key=" << substr.substr(0, current) << " value=" << substr.substr(current + 1) << std::endl;
          ret[substr.substr(0, current)] = substr.substr(current + 1);
        } else {
          LOG(DEBUG) << "CC header=" << substr << std::endl;
          ret[substr] = "";
        }
    }

  }

  //resp.add("cacheControlHeaders", ret);
  return ret;
}


// Not cacheable if has directives: no-store, private
// no-cache: may implement this directive, may not, functionality is met but just
// forwarding the response to the client in this case

// must-revalidate makes you re-validate a cached response if it is stale
// no-cache makes you re-validate a cached response everytime
bool ProxyServerCache::isCacheableResp(const Poco::Net::HTTPResponse& resp, std::string id){
  std::map<std::string,std::string> cacheHeaders = getCacheControlHeaders(resp);

  //  LOG(DEBUG) << "cache-control headers=" << cacheHeaders;
    if (cacheHeaders.find("no-store") != cacheHeaders.end()){
      LOG(INFO) << id << ": " << "not cacheable because no-store"<< std::endl;
      return false;
    } else if (cacheHeaders.find("private") != cacheHeaders.end()) {
      LOG(INFO) << id << ": " << "not cacheable because private" << std::endl;
      return false;
    }
  return true;
}


bool ProxyServerCache::hasNoCacheDirective(const Poco::Net::HTTPResponse& resp){
  std::map<std::string,std::string> cacheControlHeaders = getCacheControlHeaders(resp);
  return (cacheControlHeaders.find("no-cache") != cacheControlHeaders.end());
}

// string NO_ETAG indicates no strong validator found
std::string ProxyServerCache::getEtag(const Poco::Net::HTTPResponse& resp){
  if (resp.has("ETag")) {
    return resp.get("ETag");
  }
  return "";
}


// -1 indicates no max-age directive found
long ProxyServerCache::getMaxAge(const Poco::Net::HTTPResponse& resp){
  std::map<std::string,std::string> cacheHeaders = getCacheControlHeaders(resp);
  if (cacheHeaders.find("max-age") != cacheHeaders.end()) {
    std::map<std::string, std::string>::iterator i = cacheHeaders.find("max-age");
    return stod(i->second);
  }
  return -1;
}

// string NO_EXPIRE indicates no expiration header was found
std::string ProxyServerCache::getExpires(const Poco::Net::HTTPResponse& resp){
  if(resp.has("ETag")) {
    return resp.get("ETag");
  }
  return "";
}



std::string ProxyServerCache::getLastModified(const Poco::Net::HTTPResponse& resp){
  if(resp.has("Last-Modified")) {
    return resp.get("Last-Modified");
  }
  return "";
}

CacheResponse::CacheResponse(const Poco::Net::HTTPResponse& response, std::string respData, std::string unique_id):
  responseData(respData)
{
  this->responseTimestamp = response.getDate();
  this->isNoCache = false;
  this->mustRevalidate = false;
  this->maxFreshness = Poco::Timespan(-1, -1);

  if (response.has("Cache-Control")){ // assign cache control values
    std::map<std::string,std::string> cacheHeaders = ProxyServerCache::getCacheControlHeaders(response);
    // no need to check here for no-store or private, already filtered

      // (second, ms)
      long maxAge = ProxyServerCache::getMaxAge(response);
      this->maxFreshness = Poco::Timespan(maxAge, 0);
      this->isNoCache = (cacheHeaders.find("no-cache") != cacheHeaders.end());
      this->mustRevalidate = (cacheHeaders.find("must-revalidate") != cacheHeaders.end());

      if (isNoCache) {
        LOG(INFO) << unique_id << ": "<< "NOTE Cache-Control: no-cache" << std::endl;
      }
      if (mustRevalidate) {
        LOG(INFO) << unique_id << ": "<< "NOTE Cache-Control: must-revalidate" << std::endl;
      }
  }
  else if (this->maxFreshness.seconds() == -1){
    // determine freshness lifetime from Expires - Date, or (Date - Last-Modified)/10

    response.has("Expires") ? this->expiresStr = response.get("Expires") : this->expiresStr = "";
    response.has("Last-Modified") ? this->last_modified = response.get("Last-Modified") : this->last_modified = "";

    string fmt = "%w, %d %b %Y %H:%M:%S GMT";
    int tzDiff = 0;

    if(this->expiresStr != "") {
      Poco::DateTime expDT;

      // no-throw guarantee
      if (DateTimeParser::tryParse(fmt, this->expiresStr, expDT, tzDiff)){
        Poco::Timestamp expTS = Poco::Timestamp::fromUtcTime(expDT.utcTime());
        this->maxFreshness = expTS - response.getDate();
        LOG(DEBUG) << "freshness from max-age.seconds (exp)="
        << this->maxFreshness.totalSeconds() << endl;
      } else {
      LOG(DEBUG) << "failed to parse date" << std::endl;
        }


    } else if (this->last_modified != "") {
      Poco::DateTime modDT;

      // no-throw guarantee
      if (DateTimeParser::tryParse(fmt, this->expiresStr,modDT, tzDiff)){
        Poco::Timestamp modTS = Poco::Timestamp::fromUtcTime(modDT.utcTime());
        this->maxFreshness = (response.getDate() - modTS) / 10;
        LOG(DEBUG) << "freshness from max-age.seconds (m-f)="
        << this->maxFreshness.totalSeconds() << endl;

      } else {
      LOG(DEBUG) << "failed to parse date" << std::endl;
        }
    }

  }
  if(this->mustRevalidate) {
    LOG(INFO) << unique_id << ": "<< "cached, but requires re-validation when stale" << std::endl;
  }
  else if (this->maxFreshness.seconds() == -1 || this->isNoCache) {
    LOG(INFO) << unique_id << ": "<< "cached, but requires re-validation" << std::endl;
  }
  else if (this->maxFreshness.seconds() != -1) {
    this->expireTimestamp = this->responseTimestamp + this->maxFreshness;

    Poco::DateTime expirationDateTime(this->expireTimestamp.utcTime(),0);

    LOG(INFO) << unique_id << ": "<< "cached, expires at "
    << expirationDateTime.month() << "-" << expirationDateTime.day() << "-" << expirationDateTime.year() << " "
    << expirationDateTime.hour() << ":"<< expirationDateTime.minute() << ":" << expirationDateTime.second()
    << " GMT" << std::endl;
  }



  // Compute expiration timestamp

  response.has("ETag") ? this->Etag = response.get("ETag") : this->Etag = "";

  ProxyServerCache::copyResponseObj(response, responseObj);
}


bool CacheResponse::isValidResponse(std::string unique_id){
  // current timestamp
  Poco::Timestamp now;

  if (this->isNoCache){
    // no cache directive, must validate
    LOG(INFO) << unique_id << ": " << "in cache, requires validation" << endl;
    return false;
  }

  if(this->maxFreshness.seconds() != -1) {
    // current time preceeds expiration time
    bool fresh = now < this->expireTimestamp;
    if (fresh) {
      LOG(INFO) << unique_id << ": " << "in cache, valid" << endl;
      return true;
    } else {
      Poco::DateTime expirationDateTime(this->expireTimestamp.utcTime(),0);
      LOG(INFO) << unique_id << ": "<< "in cache, but expired at "
      << expirationDateTime.month() << "-" << expirationDateTime.day() << "-" << expirationDateTime.year() << " "
      << expirationDateTime.hour() << ":"<< expirationDateTime.minute() << ":" << expirationDateTime.second()
      << " GMT" << std::endl;
      return false;
    }

  }

  // this means the destination server response violated the RFC and didn't include a max-age, Expires, or Last-Modified
  LOG(INFO) << unique_id << ": " << "in cache, requires validation" << endl;
  return false;
}


CacheResponse::CacheResponse(const CacheResponse& rhs) :responseData(rhs.responseData.str()),
isNoCache(rhs.isNoCache), mustRevalidate(rhs.mustRevalidate), expiresStr(rhs.expiresStr),
Etag(rhs.Etag), last_modified(rhs.last_modified)
{

  expireTimestamp = rhs.expireTimestamp;
  responseTimestamp = rhs.responseTimestamp;
  maxFreshness = rhs.maxFreshness;

  LOG(DEBUG) << "Copying over headers to cache..." << std::endl;
  ProxyServerCache::copyResponseObj(rhs.responseObj, this->responseObj);
}

CacheResponse::~CacheResponse(){}


void CacheResponse::validated(std::string unique_id) {
  // if freshness time from valid request != -1
  if(this->maxFreshness.seconds() != -1) {
    // then update the expiration time to current time + freshness time
    LOG(INFO) << unique_id << ": NOTE expiration date updated." << std::endl;
    this->expireTimestamp += this->maxFreshness;
  }
}

std::string CacheResponse::getResponseDataStr(){
  return this->responseData.str();
}

std::string CacheResponse::getLastModified(){
  return this->last_modified;
}

std::string CacheResponse::getEtag(){
  return this->Etag;
}

std::ostringstream& CacheResponse::getResponseData(){
  return this->responseData;
}

bool CacheResponse::isExpired(){
  return this->expired;
}

void CacheResponse::setExpired(bool exp){
  this->expired = exp;
}


  void CacheResponse::getResponse(Poco::Net::HTTPResponse& writeToResponse) {
  // copies cookies, headers, etc
  ProxyServerCache::copyResponseObj(responseObj, writeToResponse);
}

/*
double CacheResponse::getTimeLeft(){
  return this->currentFreshness;
}
*/


void CacheResponse::setMaxFreshness(double newFreshness){
  this->maxFreshness = newFreshness;
}


Poco::Timespan CacheResponse::getMaxFreshness(){
  return this->maxFreshness;
}

bool CacheResponse::getIsNoCache(){
  return this->isNoCache;
}
