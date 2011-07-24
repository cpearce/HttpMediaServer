#ifndef __SOCKETS_H__
#define __SOCKETS_H__


class Socket {
public:
  // Initialize sockets library.
  static int Init();
  
  // Shutdown sockets library.
  static int Shutdown();
  
  virtual ~Socket() {}
  
  static Socket* Open(int aPort);
  
  virtual Socket* Accept() = 0;

  virtual void Close() = 0;

  // Number of bytes sent, or -1 on error.
  virtual int Send(const char* aBuf, int aSize) = 0;

  // Returns number of bytes read, or 0 if connection is closing.
  // <0 on error.
  virtual int Receive(char* aBuf, int aSize) = 0;

protected:
  Socket() {}
};


// Send data on an open socket.

// Receive data on an open socket.

#endif
