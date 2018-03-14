// ECE590 ERSS HW#2 Citation:
// ProxyServerConnection.cpp
//
// Library: Net
// Package: HTTPServer
// Module:  ProxyServerConnection
//
// Copyright (c) 2005-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//

#include "logging/aixlog.hpp"

#include "ProxyServerConnection.h"
#include <Poco/Net/HTTPServerSession.h>
#include <Poco/Net/HTTPServerRequestImpl.h>
#include <Poco/Net/HTTPServerResponseImpl.h>
#include <Poco/Net/NetException.h>
#include <Poco/NumberFormatter.h>
#include <Poco/Timestamp.h>
#include <Poco/Delegate.h>
#include <memory>
#include <iostream>
#include <Poco/Thread.h>
#include <Poco/Mutex.h>

namespace Poco {
  namespace Net {

    class DestinationRelay:public Poco::Runnable {
    public:
      DestinationRelay(HTTPServerSession& sess, StreamSocket& dest, std::string h): session(sess), destinationServer(dest), host(h) {}

      virtual void run() {

        // 128 KB for destination (assumption is destination will pass more data than client)
        unsigned char destinationBuffer[131072];

        bool isOpen = true;

        Poco::Timespan readPollTimeOut(10,500);
        Poco::Timespan writePollTimeOut(1,0);

        LOG(TRACE) << destinationServer.getBlocking() << "-" << destinationServer.getKeepAlive() << "-" << destinationServer.getNoDelay()
        << "-" << destinationServer.getReceiveBufferSize() << "-" << destinationServer.getReceiveTimeout().seconds() << "-"
        << destinationServer.getSendBufferSize() << "-" << destinationServer.getSendTimeout().seconds() << std::endl;

        while(isOpen){
          int nDestinationBytes = -1;

          // Strong guarantee -- Either HTTPS data is handled and transmitted
          //                      or objects are unmodified and memory is not leaked, and tunnel is closed
          try {
            if (destinationServer.poll(readPollTimeOut, Socket::SELECT_READ) == true) {
              LOG(TRACE) << "Destination server has more requests! host=" << host << std::endl  << std::flush;
              nDestinationBytes = destinationServer.receiveBytes(destinationBuffer, sizeof(destinationBuffer));
            }
            if (nDestinationBytes > 0 && session.socket().poll(writePollTimeOut, Socket::SELECT_WRITE) == true) {
              LOG(TRACE) << "Number of bytes received from destination=" << nDestinationBytes << "host=" << host << std::endl << std::flush;
              LOG(TRACE) << "Sending bytes to client server... host=" << host << std::endl;
              session.socket().sendBytes(destinationBuffer, nDestinationBytes);
              LOG(TRACE) << "Sent bytes to client server. host=" << host << std::endl;
            }
          }
          catch (Poco::Exception& exc) {
            //Handle your network errors.
            LOG(DEBUG) << "Network error=" << exc.displayText() << "host=" << host << std::endl;
            isOpen = false;
          }

          if (nDestinationBytes == 0){
            LOG(TRACE) << "Destination closes connection! host=" << host << std::endl << std::flush;
            isOpen = false;
          }

        }
      }

    private:
      HTTPServerSession& session;
      StreamSocket& destinationServer;
      std::string host;
    };


    int ProxyServerConnection::request_id = 0;

    ProxyServerConnection::ProxyServerConnection(const StreamSocket& socket, HTTPServerParams::Ptr pParams, ProxyRequestHandlerFactory::Ptr pFactory):
    TCPServerConnection(socket),
    _pParams(pParams),
    _pFactory(pFactory),
    _stopped(false)
    {
      poco_check_ptr (pFactory);

      _pFactory->serverStopped += Poco::delegate(this, &ProxyServerConnection::onServerStopped);
    }


    ProxyServerConnection::~ProxyServerConnection()
    {
      try
      {
        _pFactory->serverStopped -= Poco::delegate(this, &ProxyServerConnection::onServerStopped);
      }
      catch (...)
      {
        poco_unexpected();
      }
    }

    void ProxyServerConnection::run()
    {

      std::string server = _pParams->getSoftwareVersion();
      HTTPServerSession session(socket(), _pParams);
      int count = 0;
      // Each thread has an HTTPServerSession obj and hence is either transmitting HTTP or HTTPS (post-connect) data
      bool connect = false;
      //StreamSocket destinationServer = StreamSocket();
      while (!_stopped && session.hasMoreRequests())
      {
        // Strong guarantee -- Either request is parsed for this particular connection
        // or objects are unmodified and memory is not leaked, while solely this connection closes silently
        try
        {
          LOG(TRACE) << "Grabbing lock. from host=" << session.clientAddress().toString() << std::endl;
          Poco::FastMutex::ScopedLock lock(_mutex);
          LOG(TRACE) << "Grabbed lock. from host=" << session.clientAddress().toString() << std::endl;
          if (!_stopped)
          {
            HTTPServerResponseImpl response(session);

            // Increment unique request id for each new request created
            HTTPServerRequestImpl request(response, session, _pParams);
            std::string host(request.getHost());
            request.add("ip_addr", session.clientAddress().toString());

            _requestIdMutex.lock();
            request.add("unique_id", std::to_string(request_id));
            request_id++;
            _requestIdMutex.unlock();

            LOG(TRACE) << "Sucessfully parsed request." << std::endl;
            count++;
            LOG(TRACE) << "Request count=" <<  count << "from host=" << host << std::endl;

            Poco::Timestamp now;
            response.setDate(now);
            response.setVersion(request.getVersion());

            response.setKeepAlive(_pParams->getKeepAlive() && request.getKeepAlive() && session.canKeepAlive());
            if (!server.empty())
            response.set("Server", server);

            // Strong guarantee -- Either request is handled and expected response is returned to user for this particular connection
            //    or objects are unmodified and memory is not leaked, while solely this connection closes silently
            try
            {
              std::unique_ptr<ProxyRequestHandler> pHandler(_pFactory->createRequestHandler(request));
              if (pHandler.get())
              {
                if (request.expectContinue() && response.getStatus() == HTTPResponse::HTTP_OK)
                response.sendContinue();

                Poco::Timestamp now;
                std::string fmt = "%w %b %f %H:%M:%S %Y";
                std::string timestamp_str = Poco::DateTimeFormatter::format(now, fmt);
                LOG(INFO) << request.get("unique_id") << ": \"" << request.getMethod() << " " << request.getHost() << " " << request.getVersion()
                << "\" from " << request.get("ip_addr") << " @ " << timestamp_str << std::endl;

                if (request.getMethod() != "CONNECT") {
                  // HTTP so pass to our ProxyRequestHandler (controls GET, POST, and cache logic)
                  pHandler->handleRequest(request, response);
                } else {
                  LOG(TRACE) << "Potential CONNECT request for this session." << std::endl;
                  connect = true;

                  StreamSocket destinationServer = StreamSocket();
                  SocketAddress addr = SocketAddress(host);
                  destinationServer.connect(addr);

                  const char okMessage[] = "HTTP/1.1 200 Connection Established\r\n\n\r\n";
                  session.socket().sendBytes(okMessage, std::strlen(okMessage));

                  /*
                  Poco::Net::NameValueCollection::ConstIterator i = response.begin();
                  while(i!=response.end()){
                      LOG(DEBUG) << "CONNECT header=" <<  i->first << std::endl;
                      LOG(DEBUG) << "CONNECT value=" <<  i->second << std::endl;
                      ++i;
                  }
                  response.send(); */

                  destinationServer.setBlocking(false);
                  session.socket().setBlocking(false);
                  LOG(TRACE) << "Handling request as HTTPS data. host=" << host << std::endl;

                  Poco::Thread destThread;

                  Poco::Net::DestinationRelay dRelay(session, destinationServer, host);
                  destThread.start(dRelay);
                  relayClientData(session, destinationServer, host);
                  LOG(TRACE) << "Waiting for dest relay thread to exit..." << std::endl;
                  destThread.join();
                  LOG(TRACE) << "Dest relay thread exited" << std::endl;

                  LOG(INFO) << request.get("unique_id") << ": " << "Tunnel closed" << std::endl;
                  continue;
                }

                session.setKeepAlive(_pParams->getKeepAlive() && response.getKeepAlive() && session.canKeepAlive());

              }

              else sendErrorResponse(session, HTTPResponse::HTTP_NOT_IMPLEMENTED);
            }
            catch (Poco::Exception&)
            {

              if (!response.sent())
              {
                try
                {
                  sendErrorResponse(session, HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                }
                catch (...)
                {
                }
              }

              throw;
            }
          }
        }
        catch (NoMessageException&)
        {
          LOG(DEBUG) << "EXCEPTION: Parse Data w/ NoMessage" << std::endl;
          break;
        }
        catch (MessageException&)
        {
          LOG(DEBUG) << "EXCEPTION: Parse Data w/ Message" << std::endl;
          sendErrorResponse(session, HTTPResponse::HTTP_BAD_REQUEST);
        }
        catch (Poco::Exception&)
        {
          LOG(DEBUG) << "EXCEPTION: Parse Data" << std::endl;
          if (session.networkException())
          {
            session.networkException()->rethrow();
          }
          else throw;
        }
      }
    }

    void ProxyServerConnection::relayClientData(HTTPServerSession& session, StreamSocket& destinationServer, std::string host) {

      // 64 KB for client (assumption is destination will pass more data than client)
      unsigned char clientBuffer[65336];

      bool isOpen = true;

      Poco::Timespan readPollTimeOut(10,500);
      Poco::Timespan writePollTimeOut(1,0);

      LOG(TRACE) << session.socket().getBlocking() << "-" << session.socket().getKeepAlive() << "-" << session.socket().getNoDelay()
      << "-" << session.socket().getReceiveBufferSize() << "-" << session.socket().getReceiveTimeout().seconds() << "-"
      << session.socket().getSendBufferSize() << "-" << session.socket().getSendTimeout().seconds() << std::endl;

      while(isOpen){
        int nClientBytes = -1;

        // Strong guarantee -- Either HTTPS data is handled and transmitted
        //                      or objects are unmodified and memory is not leaked, and tunnel is closed
        try {
          if (session.socket().poll(readPollTimeOut, Socket::SELECT_READ) == true) {
            LOG(TRACE) << "Session has more requests! host=" << host << std::endl << std::flush;
            nClientBytes = session.socket().receiveBytes(clientBuffer, sizeof(clientBuffer));
          }

          if (nClientBytes > 0 && destinationServer.poll(writePollTimeOut, Socket::SELECT_WRITE) == true) {
            LOG(TRACE) << "Number of bytes received from client=" << nClientBytes << " host=" << host << std::endl << std::flush;
            LOG(TRACE) << "Sending bytes to destination server.. host=" << host << std::endl;
            destinationServer.sendBytes(clientBuffer, nClientBytes);
            LOG(TRACE) << "Sent bytes to destination server. host=" << host << std::endl;
          }

        }
        catch (Poco::Exception& exc) {
          //Handle your network errors.
          LOG(DEBUG) << "Network error=" << exc.displayText() << "host=" << host << std::endl;
          isOpen = false;
        }

        if (nClientBytes==0){
          LOG(TRACE) << "Client closes connection! host=" << host << std::endl << std::flush;
          isOpen = false;
        }

      }
    }


    void ProxyServerConnection::sendErrorResponse(HTTPServerSession& session, HTTPResponse::HTTPStatus status)
    {
      LOG(DEBUG) << "Send error response="
      << status << std::endl;

      HTTPServerResponseImpl response(session);
      response.setVersion(HTTPMessage::HTTP_1_1);
      response.setStatusAndReason(status);
      response.setKeepAlive(false);
      response.send();
      session.setKeepAlive(false);
    }


    void ProxyServerConnection::onServerStopped(const bool& abortCurrent)
    {
      _stopped = true;
      if (abortCurrent)
      {
        try
        {
          // Note: On Windows, select() will not return if one of its socket is being
          // shut down. Therefore we have to call close(), which works better.
          // On other platforms, we do the more graceful thing.
          #if defined(_WIN32)
          socket().close();
          #else
          socket().shutdown();
          #endif
        }
        catch (...)
        {
        }
      }
      else
      {
        Poco::FastMutex::ScopedLock lock(_mutex);

        try
        {
          #if defined(_WIN32)
          socket().close();
          #else
          socket().shutdown();
          #endif
        }
        catch (...)
        {
        }
      }
    }


  }
} // namespace Poco::Net
