#include "ProxyRequestHandlerFactory.h"
#include "logging/aixlog.hpp"

namespace Poco {
  namespace Net {

    ProxyRequestHandlerFactory::ProxyRequestHandlerFactory()
    {
    }


    ProxyRequestHandlerFactory::~ProxyRequestHandlerFactory()
    {
    }


    ProxyRequestHandler* ProxyRequestHandlerFactory::createRequestHandler(const HTTPServerRequest&)
    {
      return new ProxyRequestHandler();
    }
  }
}
