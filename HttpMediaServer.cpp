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
#include <map>
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512

typedef __int64 int64_t;

enum eMethod { UNKNOWN, HEAD, GET, POST };

string ToString(int64_t i) {
  char buf[256];
  sprintf_s(buf, 256, "%lld", i);
  return string(buf);
}

string Flatten(const map<string, string>& m) {
  string s;
  map<string,string>::const_iterator itr = m.begin();
  while (itr != m.end()) {
    string key = itr->first;
    string value = itr->second;
    s.append(key);
    s.append("='");
    s.append(value);
    s.append("'");
    itr++;
    if (itr != m.end()) {
      s.append(" ");
    }
  }
  return s;
}

bool ContainsKey(const map<string,string> m, const string& key) {
  return m.count(key) > 0;
}

/*
Usage:

 vector<string> tokens;
	 string str("Split,me up!Word2 Word3.");
	 Tokenize(str, tokens, ",<'> " );
	 vector <string>::iterator iter;
	 for(iter = tokens.begin();iter!=tokens.end();iter++){
	 		cout<< (*iter) << endl;
	}

*/
void Tokenize(const string& str,
              vector<string>& tokens,
              const string& delimiters)
{
  // Skip delimiters at beginning.
  string::size_type lastPos = str.find_first_not_of(delimiters, 0);
  // Find first "non-delimiter".
  string::size_type pos     = str.find_first_of(delimiters, lastPos);
  while (string::npos != pos || string::npos != lastPos)
  {
    // Found a token, add it to the vector.
    tokens.push_back(str.substr(lastPos, pos - lastPos));
    // Skip delimiters.  Note the "not_of"
    lastPos = str.find_first_not_of(delimiters, pos);
    // Find next "non-delimiter"
    pos = str.find_first_of(delimiters, lastPos);
  }
}

static int gCount = 0;

class RequestParser {
public:
  RequestParser()
    : start(0),
      complete(false),
      method(UNKNOWN),
      rangeStart(-1),
      rangeEnd(-1),
      hasRange(false),
      id(gCount++)
  {
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
      string l = string(request, start, end - start);
      Parse(l);
      start = end + 2;
    }
    printf("Request %d:\n%s", id, buf);
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

  const map<string, string>& GetParams() const {
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

    assert(Flatten(ExtractQueryParams("GET / HTTP1.1")) == "");

    assert(Flatten(ExtractQueryParams("GET // HTTP1.1")) == "");
    assert(Flatten(ExtractQueryParams("GET /// HTTP1.1")) == "");
    assert(Flatten(ExtractQueryParams("GET /dir/file.txt HTTP1.1")) == "");
    assert(Flatten(ExtractQueryParams("GET /dir/file.txt? HTTP1.1")) == "");
    assert(Flatten(ExtractQueryParams("GET /dir/file.txt?params HTTP1.1")) == "params=''");
    assert(Flatten(ExtractQueryParams("GET /dir/file.txt?param1&param2&param3 HTTP1.1")) == "param1='' param2='' param3=''");
    assert(Flatten(ExtractQueryParams("GET /dir/file.txt?param1=val1&param2=val2&param3=val3 HTTP1.1")) == "param1='val1' param2='val2' param3='val3'");
    assert(Flatten(ExtractQueryParams("GET /?params HTTP1.1")) == "params=''");
    assert(Flatten(ExtractQueryParams("GET /? HTTP1.1")) == "");
    assert(Flatten(ExtractQueryParams("GET //?params HTTP1.1")) == "params=''");

    assert(TestRange(true, "Range: bytes=0-1024", 0, 1024));
    assert(TestRange(false, "Range: time=0-1024", 0, 0));
    assert(TestRange(true, "Range: bytes=0-", 0, -1));
    assert(TestRange(true, "Range: bytes=1024-", 1024, -1));
    assert(TestRange(true, "Range: bytes=232128512-", 232128512, -1));
  }

  void GetRange(int64_t& start, int64_t& end) const {
    start = rangeStart;
    end = rangeEnd;
  }

  bool IsRangeRequest() const {
    return hasRange;
  }

  bool IsLive() const {
    return ContainsKey(GetParams(), "live");
  }

  const int id;

private:

  static bool TestRange(bool expected,
                        const string& s,
                        int64_t expStart,
                        int64_t expEnd)
  {
    int64_t start=-2, end=-2;
    bool r = ParseRange(s, start, end);
    return r == expected &&
           (!expected || start == expStart && end == expEnd);
  }

  void Parse(const string& s) {
    if (start == 0) {
      // Request line.
      ParseRequestLine(s);
    } else if (s.find("Range") == 0) {
      int64_t start,end;
      if (ParseRange(s, start, end)) {
        rangeStart = start;
        rangeEnd = end;
        hasRange = true;
      }
    }
  }

  static bool ParseRange(const string& s, int64_t& start, int64_t& end) {
    // Range: bytes=start,end
    size_t sp = s.find(" ");
    size_t eq = s.find("=");
    if (sp == string::npos || eq == string::npos)
      return false;
    string bytes(s, sp + 1, eq - sp - 1);
    if (bytes != "bytes") {
      // The space is not followed by "bytes=", this is an invalid range param.
      return false;
    }

    string range(s, eq + 1);
    size_t dash = range.find("-");
    if (dash == string::npos)
      return false;
    string startStr(range, 0, dash);
    string endStr(range, dash + 1);

    start = _atoi64(startStr.c_str());
    if (endStr != "") {
      end = _atoi64(endStr.c_str());
    } else {
      end = -1;
    }

    return true;
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

  static map<string, string> ExtractQueryParams(const string& request) {
    map<string,string> m;
    size_t query = request.find("?");
    if (query == string::npos)
      return m;
    size_t start = query + 1;
    size_t end = request.find(" ", start);
    vector<string> v;
    Tokenize(string(request, start, end - start), v, "&");
    for (unsigned i=0; i<v.size(); i++) {
      size_t eq = v[i].find("=");
      string key;
      string value;
      if (eq == string::npos) {
        key = v[i];
        value = "";
      } else {
        key = string(v[i], 0, eq);
        value = string(v[i], eq + 1, v[i].size() - eq - 1);
      }
      m[key] = value;
    }
    return m;
  }

  void ParseRequestLine(const string& request) {
    // Extract method.
    method = ExtractMethod(request);
    target = ExtractTarget(request);
    params = ExtractQueryParams(request);
    //printf("Target: '%s'\n", target.c_str());
    //printf("Params: '%s'\n", Flatten(params).c_str());
  }
  
  string request;
  size_t start;
  vector<string> lines;
  bool complete;
  eMethod method;
  string target;
  map<string, string> params;
  int64_t rangeStart, rangeEnd;
  bool hasRange;
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

  enum eMode { GET_ENTIRE_FILE, GET_FILE_RANGE, DIR_LIST, ERROR_FILE_NOT_EXIST, INTERNAL_ERROR };

public:
  Response(RequestParser p)
    : parser(p),
      mode(INTERNAL_ERROR),
      fileLength(-1),
      file(0),
      rangeStart(0),
      rangeEnd(0),
      offset(0),
      bytesRemaining(0)
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
      } else if (parser.IsRangeRequest() && !parser.IsLive()) {
        mode = GET_FILE_RANGE;
        path = parser.GetTarget();
        parser.GetRange(rangeStart, rangeEnd);
        if (rangeEnd == -1) {
          rangeEnd = buf.st_size;
        }
        fileLength = buf.st_size;
      } else {
        mode = GET_ENTIRE_FILE;
        path = parser.GetTarget();
        fileLength = buf.st_size;
      }
    }
  }

  bool SendHeaders(SOCKET socket) {
    string headers;
    headers.append("HTTP/1.1 ");
    headers.append(StatusCode(mode));
    headers.append("\r\n");
    headers.append("Connection: close\r\n");
    headers.append(GetDate());
    headers.append("\r\n");
    headers.append("Server: HttpMediaServer/0.1\r\n");
    
    if (!parser.IsLive()) {
      if (mode == GET_ENTIRE_FILE) {
        headers.append("Accept-Ranges: bytes\r\n");
        headers.append("Content-Length: ");
        headers.append(ToString(fileLength));
        headers.append("\r\n");
      } else if (mode == GET_FILE_RANGE) {
        headers.append("Accept-Ranges: bytes\r\n");
        headers.append("Content-Length: ");
        headers.append(ToString(rangeEnd - rangeStart));
        headers.append("\r\n");
        headers.append("Content-Range: bytes ");
        headers.append(ToString(rangeStart));
        headers.append("-");
        headers.append(ToString(rangeEnd));
        headers.append("/");
        headers.append(ToString(fileLength));
        headers.append("\r\n");
      }
    }
    //headers.append("Last-Modified: Wed, 10 Nov 2009 04:58:08 GMT\r\n");

    headers.append("Content-Type: ");
    headers.append(ExtractContentType(path, mode));
    headers.append("\r\n\r\n");

    printf("Sending Headers %d:\n%s\n", parser.id, headers.c_str());

    return SOCKET_ERROR != send(socket, headers.c_str(), headers.size(), 0);
  }

  // Returns true if we need to call again.
  bool SendBody(SOCKET socket) {
    if (mode == ERROR_FILE_NOT_EXIST) {
      printf("Sent (empty) body (%d)\n", parser.id);
      return false;
    }
    /*if (mode == DIR_LIST) {
      SendDirectoryList();
    } else */
    fd_set readFd, writeFd;
    FD_ZERO(&readFd);
    FD_ZERO(&writeFd);
    FD_SET(socket, &readFd);
    FD_SET(socket, &writeFd);
    // Wait until we can write.
    select(0, &readFd, &writeFd, 0, 0);

    if (FD_ISSET(socket, &readFd)) {

      char recvbuf[DEFAULT_BUFLEN];
      int iResult, iSendResult;
      int recvbuflen = DEFAULT_BUFLEN;

      memset(recvbuf, 0, DEFAULT_BUFLEN);
      iResult = recv(socket, recvbuf, recvbuflen, 0);
      if (iResult > 0) {
        //parser.Add(recvbuf, iResult);
        printf("%d read data unexpectedly!:\n%s\n", parser.id, recvbuf);
      } else if (iResult == 0) {
        printf("%d Connection closing...\n", parser.id);
        return false;
        //break;
      } else {
        printf("%d recv failed: %d\n", parser.id, WSAGetLastError());
        closesocket(socket);
        return false;
      }
      printf("%d recv in sendBody:\n%s", parser.id, recvbuf);

      return true;
    } else if (!FD_ISSET(socket, &writeFd)) {
      return true;
    }


    unsigned len = 1024;
    unsigned wait = 0;
    if (ContainsKey(parser.GetParams(), "rate")) {
      const map<string,string> params = parser.GetParams();
      string rateStr = params.find("rate")->second;
      float rate = atof(rateStr.c_str());
      const float period = 0.1;
      if (rate <= 0.0) {
        len = 1024;
        wait = 0;
      } else {
        len = (unsigned)(rate * 1024 * period);
        wait = 1000 * period; // ms
      }
    }

    if (mode == GET_ENTIRE_FILE) {
      if (!file) {
        file = fopen(path.c_str(), "rb");
      }
      if (feof(file)) {
        // Transmitted entire file!
        fclose(file);
        file = 0;
        return false;
      }

      int64_t tell = _ftelli64(file);

      // Transmit the next segment.
      //const unsigned len = 1024;
      char* buf = new char[len];
      unsigned x = fread(buf, 1, len, file);
      int r = send(socket, buf, x, 0);
      delete buf;
      if (SOCKET_ERROR == r) {
        // Some kind of error.
        printf("%d Send failed: %d\n", parser.id, WSAGetLastError());
        return false;
      }
      
      if (wait > 0) {
        Sleep(wait);
      }
      //printf("%d %s Send [%lld,%u]\n", parser.id, path.c_str(), tell, _ftelli64(file));
      // Else we tranmitted that segment, we're ok.
      
      // Wait if our playback rate is limited.

      return true;
    } else if (mode == GET_FILE_RANGE) {
      if (!file) {
        file = fopen(path.c_str(), "rb");
        _fseeki64(file, rangeStart, SEEK_SET);
        offset = rangeStart;
        bytesRemaining = rangeEnd - rangeStart;
      }
      if (feof(file) || bytesRemaining == 0) {
        // Transmitted entire range.
        fclose(file);
        file = 0;
        return false;
      }

      // Transmit the next segment.
      char* buf = new char[len];

      len = min(bytesRemaining, len);
      unsigned bytesSent = fread(buf, 1, len, file);
      bytesRemaining -= bytesSent;
      int r = send(socket, buf, bytesSent, 0);
      delete buf;
      if (SOCKET_ERROR == r) {
        // Some kind of error.
        printf("%d send failed: %d\n", parser.id, WSAGetLastError());
        return false;
      }
      //printf("%d %s Send [%lld,%u]\n", parser.id, path.c_str(), offset, offset+bytesSent);
      offset += bytesSent;
      assert(_ftelli64(file) == offset);

      if (wait > 0) {
        Sleep(wait);
      }

      // Else we tranmitted that segment, we're ok.
      return true;
    }

    return false;
    //return SOCKET_ERROR != send(socket, body.c_str(), body.size(), 0);
  }

  static void Test() {
    assert(ExtractContentType("dir1/dir2/file.ogv", GET_ENTIRE_FILE) == string("video/ogg"));
    assert(ExtractContentType("dir1/dir2/file.ogg", GET_ENTIRE_FILE) == string("video/ogg"));
    assert(ExtractContentType("dir1/dir2/file.oga", GET_ENTIRE_FILE) == string("audio/ogg"));
    assert(ExtractContentType("dir1/dir2/file.wav", GET_ENTIRE_FILE) == string("audio/x-wav"));
    assert(ExtractContentType("dir1/dir2/file.webm", GET_ENTIRE_FILE) == string("video/webm"));
    assert(ExtractContentType("dir1/dir2/file.txt", GET_ENTIRE_FILE) == string("text/plain"));
    assert(ExtractContentType("dir1/dir2/file.html", GET_ENTIRE_FILE) == string("text/html"));
  }

private:

  static string StatusCode(eMode mode) {
    switch (mode) {
      case GET_ENTIRE_FILE: return string("200 OK");
      case GET_FILE_RANGE: return string("206 OK");
      case DIR_LIST: return string("200 OK");
      case ERROR_FILE_NOT_EXIST: return string("404 File Not Found");
      case INTERNAL_ERROR:
      default:
        return string("500 Internal Server Error");
    };
  }

  static string ExtractContentType(const string& file, eMode mode) {
    if (mode == ERROR_FILE_NOT_EXIST || mode == INTERNAL_ERROR)
      return "text/html; charset=iso-8859-1";
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

  int64_t fileLength;
  RequestParser parser;
  eMode mode;
  string path;
  FILE* file;
  int64_t rangeStart;
  int64_t rangeEnd;
  int64_t offset;
  int64_t bytesRemaining;
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
      //iResult = recv(ClientSocket, recvbuf, recvbuflen, MSG_PEEK);
      //if (iResult == 0) {
      //  printf("Connection reset by peer\n");
      //  //closesocket(ClientSocket);
      //  break;
      //}
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
