//
// HTTPServerConnection.h
//
// Library: Net
// Package: HTTPServer
// Module:  HTTPServerConnection
//
// Definition of the HTTPServerConnection class.
//
// Copyright (c) 2005-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef Net_HTTPServerConnection_INCLUDED
#define Net_HTTPServerConnection_INCLUDED


#include <Poco/Net/Net.h>
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/HTTPResponse.h>
#include "ProxyRequestHandlerFactory.h"
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Mutex.h>

namespace Poco {
namespace Net {


class Net_API ProxyServerConnection: public TCPServerConnection
	/// This subclass of TCPServerConnection handles HTTP
	/// connections.
{
public:
	static int request_id;

	ProxyServerConnection(const StreamSocket& socket, HTTPServerParams::Ptr pParams, ProxyRequestHandlerFactory::Ptr pFactory);
		/// Creates the ProxyServerConnection.

	virtual ~ProxyServerConnection();
		/// Destroys the ProxyServerConnection.

	void relayClientData(HTTPServerSession& session, StreamSocket& destinationServer, std::string host);
	void run();
		/// Handles all HTTP requests coming in.

protected:
	void sendErrorResponse(HTTPServerSession& session, HTTPResponse::HTTPStatus status);
	void onServerStopped(const bool& abortCurrent);

private:
	HTTPServerParams::Ptr          _pParams;
	ProxyRequestHandlerFactory::Ptr _pFactory;
	bool _stopped;
	Poco::FastMutex _mutex;
	Poco::FastMutex _requestIdMutex;
};


} } // namespace Poco::Net


#endif // Net_ProxyServerConnection_INCLUDED
