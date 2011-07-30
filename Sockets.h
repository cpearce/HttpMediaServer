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
