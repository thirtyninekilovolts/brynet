#include <atomic>
#include <brynet/base/AppStatus.hpp>
#include <brynet/base/Packet.hpp>
#include <brynet/net/EventLoop.hpp>
#include <brynet/net/ListenThread.hpp>
#include <brynet/net/SocketLibFunction.hpp>
#include <brynet/net/TcpConnection.hpp>
#include <brynet/net/TcpService.hpp>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>
#include <vector>

using namespace brynet;
using namespace brynet::net;
using namespace brynet::base;
using namespace brynet::net::http;

std::vector<brynet::net::http::HttpSession::Ptr> sessions;
std::queue<std::string> m_buffer;

void addClient(const brynet::net::http::HttpSession::Ptr& client)
{
	sessions.push_back(client);
}

void sendStreamData()
{

	while (true)
	{
		std::this_thread::sleep_for(10ms);
		HttpResponse httpResponse;

		// ...
		// Prepare frame to client
		if (!m_buffer.empty())
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				httpResponse.setBody(std::move(m_buffer.front()));
				m_buffer.pop();
			}

			// ...
			httpResponse.setContentType("image/jpeg");
			for (auto i : m_sessions)
			{
				i->send(httpResponse.getResult("--mjpegstream"));
			}
		}
	}	
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage : <listen port> <thread num> \n");
        exit(-1);
    }

	auto service = TcpService::Create();
	service->startWorkerThread(std::thread::hardware_concurrency());

	auto mainLoop = std::make_shared<EventLoop>();

	// Callback for http request
	auto httpEnterCallback = [&](const HTTPParser& httpParser,
		const HttpSession::Ptr& session)
	{
		HttpResponse firstResponse;

		firstResponse.setContentType("multipart/x-mixed-replace; boundary=mjpegstream");
		firstResponse.addHeadValue("Connection", "close");
		firstResponse.addHeadValue("max-ages", "0");
		firstResponse.addHeadValue("expires", "0");
		firstResponse.addHeadValue("Cache-Control", "no-cache, private");	
		firstResponse.addHeadValue("Pragma", "no-cache");

		session->send(firstResponse.getResult());

		mainLoop->runAsyncFunctor([&, session]() {
			addClient(session);
			});
	};


	wrapper::HttpListenerBuilder httpListenBuilder;

	unsigned int httpServerPort{ 8082 };

	try
	{
		httpListenBuilder
			.WithService(service)
			.AddSocketProcess([](TcpSocket& socket) {
			socket.setNodelay();
				})
			.WithMaxRecvBufferSize(8192)
					.WithAddr(false, "127.0.0.1", httpServerPort)
					.WithReusePort()
					.WithEnterCallback([httpEnterCallback]
					(const HttpSession::Ptr& httpSession, HttpSessionHandlers& handlers) {
							handlers.setHttpCallback(httpEnterCallback);
						})
					.asyncRun();
	}
	catch (const brynet::net::BrynetCommonException& e)
	{
		// ...
		std::string exceptionMessage("HTTP Server Error: could not bind to given port. ");
	}

	// Infinite loop for connection
	while (true)
	{
		mainLoop->loop(10);
		std::this_thread::sleep_for(1s);
		if (brynet::base::app_kbhit())
		{
			break;
		}
	}

    return 0;
}
