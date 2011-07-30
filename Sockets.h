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

#ifndef __SOCKETS_H__
#define __SOCKETS_H__

// Wraps platform-specific socket API.
class Socket {
public:
  // Initialize sockets library.
  static int Init();
  
  // Shutdown sockets library.
  static int Shutdown();
  
  virtual ~Socket() {}
  
  // Opens a port for inbound connections. Use this to create a socket
  // and open an inbound port.
  static Socket* Open(int aPort);
  
  // Accepts connections on an open port.
  virtual Socket* Accept() = 0;

  // Shutsdown connection.
  virtual void Close() = 0;

  // Sends data over socket. Returns number of bytes sent, or -1 on error.
  // Blocking.
  virtual int Send(const char* aBuf, int aSize) = 0;

  // Reads data from an open socket. Returns number of bytes read, or 0 if
  // connection is closing. <0 on error. Blocking.
  virtual int Receive(char* aBuf, int aSize) = 0;

protected:
  Socket() {}
};

#endif
