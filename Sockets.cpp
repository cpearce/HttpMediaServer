#include "Sockets.h"
#include "Utils.h"

#ifdef WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

class Win32Socket : public Socket {
public:
  virtual ~Win32Socket();
  Socket* Accept();
  Win32Socket(SOCKET aSocket);
  void Close();
  int Send(const char* aBuf, int aSize);
  int Receive(char* aBuf, int aSize);
  int Select();

  static WSADATA sWsaData;
  SOCKET mSocket;
};

WSADATA Win32Socket::sWsaData = {0};

Win32Socket::Win32Socket(SOCKET aSocket)
  : mSocket(aSocket)
{ }

Win32Socket::~Win32Socket()
{
  Close();
}

Socket* Socket::Open(int aPort) {
  // Init a server port.
  struct addrinfo *addr = NULL, *ptr = NULL, hints;

  ZeroMemory(&hints, sizeof (hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  // Resolve the local address and port to be used by the server
  string port = ToString(aPort);
  int res = getaddrinfo(NULL, port.c_str(), &hints, &addr);
  if (res != 0) {
    printf("getaddrinfo failed: %d\n", res);
    return 0;
  }

  SOCKET serverSocket = INVALID_SOCKET;
  // Create a SOCKET for the server to listen for client connections
  serverSocket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

  if (serverSocket == INVALID_SOCKET) {
    printf("Error at socket(): %ld\n", WSAGetLastError());
    freeaddrinfo(addr);
    return 0;
  }

  // Setup the TCP listening socket
  res = bind( serverSocket, addr->ai_addr, (int)addr->ai_addrlen);
  if (res == SOCKET_ERROR) {
    printf("bind failed: %d\n", WSAGetLastError());
    freeaddrinfo(addr);
    closesocket(serverSocket);
    return 0;
  }

  // Address info is no longer needed, we've bound the socket.
  freeaddrinfo(addr);

  if ( listen( serverSocket, SOMAXCONN ) == SOCKET_ERROR ) {
    printf("Listen failed with error: %ld\n", WSAGetLastError() );
    closesocket(serverSocket);
    return 0;
  }

  return new Win32Socket(serverSocket);
}

Socket* Win32Socket::Accept() {
  SOCKET client = accept(mSocket, NULL, NULL);
  if (client == INVALID_SOCKET) {
    printf("accept failed: %d\n", WSAGetLastError());
    closesocket(mSocket);
    return 0;
  }
  return new Win32Socket(client);
}

void Win32Socket::Close() {
  if (mSocket != INVALID_SOCKET) {
    closesocket(mSocket);
  }
}

int Win32Socket::Receive(char* aBuf, int aSize) {
  memset(aBuf, 0, aSize);
  int r = recv(mSocket, aBuf, aSize, 0);
  if (r < 0) {
    printf("recv failed: %d\n", WSAGetLastError());
  }
  return r;
}

int Win32Socket::Send(const char* aBuf, int aSize) {
  return send(mSocket, aBuf, aSize, 0);
}

int Win32Socket::Select() {
  fd_set readFd, writeFd;
  FD_ZERO(&readFd);
  FD_ZERO(&writeFd);
  FD_SET(mSocket, &readFd);
  FD_SET(mSocket, &writeFd);
  // Wait until we can write.
  select(0, &readFd, &writeFd, 0, 0);
  int r = 0;
  if (FD_ISSET(socket, &readFd)) {
    r |= SOCKET_CAN_READ;
  }
  if (!FD_ISSET(socket, &writeFd)) {
    r |= SOCKET_CAN_WRITE;
  }
  return r;
}

int Socket::Init() {
  // Initialize Winsock
  int err = WSAStartup(MAKEWORD(2,2), &Win32Socket::sWsaData);
  if (err) {
    printf("WSAStartup failed: %d\n", err);
    return 1;
  }
  return 0;
}

int Socket::Shutdown() {
  WSACleanup();
  return 0;
}

#endif