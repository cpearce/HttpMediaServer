
#include <stdio.h>
#include <vector>
#include <string>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <memory>

#include "Utils.h"
#include "Thread.h"
#include "RequestParser.h"
#include "Sockets.h"
#include "Response.h"

using std::auto_ptr;

#ifdef WIN32
#define PORT 80
#else
#define PORT 8080
#endif

class Connection : public Runnable{
public:
  Connection(Socket* s)
    : mClientSocket(s)
  {
    mThread = Thread::Create(this);
  }

  void Start() {
    mThread->Start();
  }

  virtual void Run() {
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    // Receive until the peer shuts down the connection
    while (!parser.IsComplete()) {
      int r = mClientSocket->Receive(recvbuf, recvbuflen);
      if (r > 0) {
        parser.Add(recvbuf, r);
      } else if (r == 0) {
        printf("Connection closing...\n");
        break;
      } else {
        mClientSocket->Close();
        return;
      }
    }

    Response response(parser);

    if (!response.SendHeaders(mClientSocket.get())) {
      mClientSocket->Close();
      return;
    }

    while (response.SendBody(mClientSocket.get())) {
      // Transmit the body.
    }


    // shutdown the send half of the connection since no more data will be sent
    // TODO: Is this needed?
    //mClientSocket->CloseSend();
    //iResult = shutdown(ClientSocket, SD_SEND);
    //if (iResult == SOCKET_ERROR) {
    //  printf("shutdown failed: %d\n", WSAGetLastError());
    //  mClientSocket->Close();
    //  return;
    //}

    // cleanup
    mClientSocket->Close();
  }
private:
  auto_ptr<Socket> mClientSocket;
  RequestParser parser;
  Thread* mThread;
};

int main(int argc, char* argv[])
{
  RequestParser::Test();
  Response::Test();
  Thread_Test();

  Socket::Init();

  auto_ptr<Socket> listener(Socket::Open(PORT));
  if (!listener.get()) {
    Socket::Shutdown();
    return 1;
  }

  vector<Connection*> Connections;
  while (true) {
    // Accept a single connection.
    Socket* client = listener->Accept();
    if (!client) {
      Socket::Shutdown();
      return 1;
    }

    Connection *c = new Connection(client);
    c->Start();
    Connections.push_back(c);
  }

  Socket::Shutdown();
  
  return 0;
}
