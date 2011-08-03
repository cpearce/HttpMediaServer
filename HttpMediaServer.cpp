/*
   Copyright (c) 2011, Chris Pearce

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of Chris Pearce nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <vector>
#include <string>
#include <assert.h>
#include <stdlib.h>
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
#ifdef _DEBUG
  RequestParser::Test();
  Response::Test();
  Thread_Test();
#endif

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
