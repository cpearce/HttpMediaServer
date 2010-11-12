// EchoServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include "Thread.h"
#include <vector>
#include <string>
#include <assert.h>
#include <direct.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512

enum eMethod { UNKNOWN, HEAD, GET, POST };

class RequestParser {
public:
  RequestParser() : start(0), complete(false), method(UNKNOWN) {

  }

  void Add(const char* buf, unsigned len) {
    assert(!complete);
    string str(buf, len);
    request += str;
    const char* linebr = "\r\n";
    assert(linebr[0] == 13);
    assert(linebr[1] == 10);
    // Continue to parse request.
    while (start < request.size()) {
      size_t end = request.find(linebr, start, 2);
      if (end == string::npos)
        break;
      if (start == end)
        complete = true;
      string l = string(request, start, end);
      Parse(l);
      start = end + 2;
    }
  }

  bool IsComplete() {
    return complete;
  }

  eMethod GetMethod() {
    return method;
  }
  
  string GetTarget() {
    return target;
  }

  string GetParams() {
    return params;
  }

  static void Test() {
    assert(ExtractMethod("GET / HTTP1.1") == GET);
    assert(ExtractMethod("HEAD / HTTP1.1") == HEAD);
    assert(ExtractMethod("POST / HTTP1.1") == POST);
    assert(ExtractMethod("Error / HTTP1.1") == UNKNOWN);

    assert(ExtractTarget("GET / HTTP1.1") == "");
    assert(ExtractTarget("GET // HTTP1.1") == "");
    assert(ExtractTarget("GET /// HTTP1.1") == "");
    assert(ExtractTarget("GET /dir/file.txt HTTP1.1") == "dir/file.txt");
    assert(ExtractTarget("GET /dir/file.txt?params HTTP1.1") == "dir/file.txt");
    assert(ExtractTarget("GET /?params HTTP1.1") == "");
    assert(ExtractTarget("GET //?params HTTP1.1") == "");

    assert(ExtractQueryParams("GET / HTTP1.1") == "");
    assert(ExtractQueryParams("GET // HTTP1.1") == "");
    assert(ExtractQueryParams("GET /// HTTP1.1") == "");
    assert(ExtractQueryParams("GET /dir/file.txt HTTP1.1") == "");
    assert(ExtractQueryParams("GET /dir/file.txt?params HTTP1.1") == "params");
    assert(ExtractQueryParams("GET /?params HTTP1.1") == "params");
    assert(ExtractQueryParams("GET //?params HTTP1.1") == "params");

  }

private:

  void Parse(const string& s) {
    if (start == 0) {
      // Request line.
      ParseRequestLine(s);
    }
  }

  static eMethod ExtractMethod(const string& request) {
    size_t sp = request.find(" ");
    string m(request, 0, sp);
    if (m == "GET")
      return GET;
    else if (m == "HEAD")
      return HEAD;
    else if (m == "POST")
      return POST;
    return UNKNOWN;
  }

  static string ExtractTarget(const string& request) {
    size_t start = request.find("/") + 1;
    size_t query = request.find("?", start); // Location of query params.
    size_t end = (query != string::npos) ? query : request.find(" ", start);
    // Remove trailing slashes.
    while (end > start && request.at(end - 1) == '/') {
      end--;
    }
    return string(request, start, end - start);
  }

  static string ExtractQueryParams(const string& request) {
    size_t query = request.find("?");
    if (query == string::npos)
      return string();
    size_t start = query + 1;
    size_t end = request.find(" ", start);
    return string(request, start, end - start);
  }

  void ParseRequestLine(const string& request) {
    // Extract method.
    method = ExtractMethod(request);
    target = ExtractTarget(request);
    params = ExtractQueryParams(request);
    printf("Target: '%s'\n", target.c_str());
    printf("Params: '%s'\n", params.c_str());
  }
  
  string request;
  size_t start;
  vector<string> lines;
  bool complete;
  eMethod method;
  string target;
  string params;
};

#define ARRAY_LENGTH(array_) \
  (sizeof(array_)/sizeof(array_[0]))

static const char* gContentTypes[][2] = {
  {"ogv", "video/ogg"},
  {"ogg", "video/ogg"},
  {"oga", "audio/ogg"},
  {"webm", "video/webm"},
  {"wav", "audio/x-wav"},
  {"html", "text/html"},
  {"txt", "text/plain"}
};

class Response {

  enum eMode { NORMAL_FILE_GET, FILE_GET_RANGE, DIR_LIST, ERROR_FILE_NOT_EXIST, INTERNAL_ERROR };

public:
  Response(RequestParser p)
    : parser(p),
      mode(INTERNAL_ERROR)
  {
    string target = parser.GetTarget();
    if (target == "") {
      mode = DIR_LIST;
    } else if (target.find("..") != string::npos) {
      mode = ERROR_FILE_NOT_EXIST;
    } else {
      // Determine if the file exists, and if it is a directory.
      struct __stat64 buf;
      int result;
      result = _stat64( target.c_str(), &buf );
      if (result == -1) {
        printf("File not found\n", result);
        mode = ERROR_FILE_NOT_EXIST;
      } else if (result != 0) {
        mode = INTERNAL_ERROR;
      } else if ((buf.st_mode & _S_IFDIR) == _S_IFDIR) {
        mode = DIR_LIST;
        path = parser.GetTarget();
      } else {
        mode = NORMAL_FILE_GET;
        path = parser.GetTarget();
      }
    }
  }

  bool SendHeaders(SOCKET socket) {
    string headers;
    headers.append("HTTP/1.1 ");
    headers.append(StatusCode(mode));
    headers.append("\n");
    headers.append(GetDate());
    headers.append("\n");
    headers.append("Server: HttpMediaServer/0.1\n");
    headers.append("Content-Type: ");
    headers.append(ExtractContentType(path, mode));
    headers.append("\n\n");

    return SOCKET_ERROR != send(socket, headers.c_str(), headers.size(), 0);
  }

  bool SendBody(SOCKET socket) {
    /*string body = "Ready to serve";
    send(socket, body.c_str(), body.size(), 0);
*/
    /*if (mode == DIR_LIST) {
      SendDirectoryList();
    } else */
    if (mode == NORMAL_FILE_GET) {
      FILE* f = fopen(path.c_str(), "rb");
      const unsigned len = 1024;
      char buf[len];
      while (!feof(f)) {
        unsigned x = fread(buf, 1, len, f);
        if (SOCKET_ERROR == send(socket, buf, x, 0)) {
          return false;
        }
      }
    }

    return false;
    //return SOCKET_ERROR != send(socket, body.c_str(), body.size(), 0);
  }

  static void Test() {
    assert(ExtractContentType("dir1/dir2/file.ogv", NORMAL_FILE_GET) == string("video/ogg"));
    assert(ExtractContentType("dir1/dir2/file.ogg", NORMAL_FILE_GET) == string("video/ogg"));
    assert(ExtractContentType("dir1/dir2/file.oga", NORMAL_FILE_GET) == string("audio/ogg"));
    assert(ExtractContentType("dir1/dir2/file.wav", NORMAL_FILE_GET) == string("audio/x-wav"));
    assert(ExtractContentType("dir1/dir2/file.webm", NORMAL_FILE_GET) == string("video/webm"));
    assert(ExtractContentType("dir1/dir2/file.txt", NORMAL_FILE_GET) == string("text/plain"));
    assert(ExtractContentType("dir1/dir2/file.html", NORMAL_FILE_GET) == string("text/html"));
  }

private:

  static string StatusCode(eMode mode) {
    switch (mode) {
      case NORMAL_FILE_GET: return string("200 OK");
      case FILE_GET_RANGE: return string("206 OK");
      case DIR_LIST: return string("200 OK");
      case ERROR_FILE_NOT_EXIST: return string("404 File Not Found");
      case INTERNAL_ERROR: return string("500 Internal Server Error");
    };
  }

  static string ExtractContentType(const string& file, eMode mode) {
    if (mode == ERROR_FILE_NOT_EXIST || mode == INTERNAL_ERROR)
      return "text/plain";
    if (mode == DIR_LIST)
      return "text/html";

    size_t dot = file.rfind(".");
    if (dot == string::npos) {
      // File has no extension. Just send it as binary.
      return "application/octet-stream";
    }

    string extension(file, dot+1, file.size() - dot - 1);

    for (unsigned i=0; i<ARRAY_LENGTH(gContentTypes); i++) {
      if (extension == gContentTypes[i][0]) {
        return string(gContentTypes[i][1]);
      }
    }
    return "application/octet-stream";
  }

  static string GetDate() {
    SYSTEMTIME t;
    GetSystemTime(&t);

    string s;
    char buf[1024];
    char month[12][4] = {
      "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
    };
    char day[7][4] = {
      "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
    };
    unsigned len = sprintf(buf, "Date: %s, %d %s %d %.2d:%.2d:%.2d GMT",
                           day[t.wDayOfWeek], t.wDay, month[t.wMonth],
                           t.wYear, t.wHour, t.wMinute, t.wSecond);

    return string(buf, len);
  }


  RequestParser parser;
  eMode mode;
  string path;
};

class ConnectionThread : public Thread {
public:
  ConnectionThread(SOCKET s) : ClientSocket(s) {}
  virtual void Run() {
    char recvbuf[DEFAULT_BUFLEN];
    int iResult, iSendResult;
    int recvbuflen = DEFAULT_BUFLEN;

    // Receive until the peer shuts down the connection
    while (!parser.IsComplete()) {
      memset(recvbuf, 0, DEFAULT_BUFLEN);
      iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
      if (iResult > 0) {
        printf("%s", recvbuf);
        parser.Add(recvbuf, iResult);
      } else if (iResult == 0) {
        printf("Connection closing...\n");
        break;
      } else {
        printf("recv failed: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        return;
      }

    }

    Response response(parser);

    if (!response.SendHeaders(ClientSocket)) {
      printf("Failed to send headers: %d\n", WSAGetLastError());
      closesocket(ClientSocket);
      return;
    }

    while (response.SendBody(ClientSocket)) {
    }

    //printf("%s\n", GetDate().c_str());
    //const char* reply = "HTTP/1.1 200 OK\nDate: Fri, 12 Nov 2010 01:03:01 GMT\nServer: HttpMediaServer/0.1\nContent-Type: text/html\n\nReady to serve.\0";
    //iSendResult = send(ClientSocket, reply, strlen(reply), 0);
    //if (iSendResult == SOCKET_ERROR) {
    //  printf("send failed: %d\n", WSAGetLastError());
    //  closesocket(ClientSocket);
    //  return;
    //}

    // shutdown the send half of the connection since no more data will be sent
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
      printf("shutdown failed: %d\n", WSAGetLastError());
      closesocket(ClientSocket);
      return;
    }

    // cleanup
    closesocket(ClientSocket);
  }
private:
  SOCKET ClientSocket;
  RequestParser parser;
};

int _tmain(int argc, _TCHAR* argv[])
{
  RequestParser::Test();
  Response::Test();

  WSADATA wsaData;

  int iResult;

  // Initialize Winsock
  iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (iResult != 0) {
    printf("WSAStartup failed: %d\n", iResult);
    return 1;
  }

  // Init a server port.
  struct addrinfo *result = NULL, *ptr = NULL, hints;

  ZeroMemory(&hints, sizeof (hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  // Resolve the local address and port to be used by the server
  iResult = getaddrinfo(NULL, "80", &hints, &result);
  if (iResult != 0) {
    printf("getaddrinfo failed: %d\n", iResult);
    WSACleanup();
    return 1;
  }

  SOCKET ListenSocket = INVALID_SOCKET;
  // Create a SOCKET for the server to listen for client connections
  ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

  if (ListenSocket == INVALID_SOCKET) {
    printf("Error at socket(): %ld\n", WSAGetLastError());
    freeaddrinfo(result);
    WSACleanup();
    return 1;
  }

  // Setup the TCP listening socket
  iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
  if (iResult == SOCKET_ERROR) {
    printf("bind failed: %d\n", WSAGetLastError());
    freeaddrinfo(result);
    closesocket(ListenSocket);
    WSACleanup();
    return 1;
  }

  // Address info is no longer needed, we've bound the socket.
  freeaddrinfo(result);

  if ( listen( ListenSocket, SOMAXCONN ) == SOCKET_ERROR ) {
    printf( "Listen failed with error: %ld\n", WSAGetLastError() );
    closesocket(ListenSocket);
    WSACleanup();
    return 1;
  }

  SOCKET ClientSocket;
  vector<ConnectionThread*> Connections;
  while (1) {
    // Accept a single connection.
    ClientSocket = INVALID_SOCKET;

    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
      printf("accept failed: %d\n", WSAGetLastError());
      closesocket(ListenSocket);
      WSACleanup();
      return 1;
    }
    ConnectionThread *t = new ConnectionThread(ClientSocket);
    t->Start();
    Connections.push_back(t);
  }

  WSACleanup();
  
  return 0;
}
