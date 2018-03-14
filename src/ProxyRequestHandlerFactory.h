#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include "ProxyRequestHandler.h"
#include <Poco/SharedPtr.h>
#include <Poco/ExpirationDecorator.h>
#include <ctime>
#include "logging/aixlog.hpp"

namespace Poco {
  namespace Net {

    class ProxyRequestHandlerFactory : public HTTPRequestHandlerFactory
    {
    public:
      typedef Poco::SharedPtr<ProxyRequestHandlerFactory> Ptr;

      ProxyRequestHandlerFactory();
      ~ProxyRequestHandlerFactory();
      virtual ProxyRequestHandler* createRequestHandler(const HTTPServerRequest &);

      Poco::BasicEvent<const bool> serverStopped;

    };
  }
}
