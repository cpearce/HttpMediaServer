// EchoServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
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

using std::auto_ptr;

#define DEFAULT_BUFLEN 512


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

  enum eMode {
    GET_ENTIRE_FILE,
    GET_FILE_RANGE,
    DIR_LIST,
    ERROR_FILE_NOT_EXIST,
    INTERNAL_ERROR
  };

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

  bool SendHeaders(Socket* aSocket) {
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

    return aSocket->Send(headers.c_str(), headers.size()) != -1;
  }

  // Returns true if we need to call again.
  bool SendBody(Socket *aSocket) {
    if (mode == ERROR_FILE_NOT_EXIST) {
      printf("Sent (empty) body (%d)\n", parser.id);
      return false;
    }
    /*if (mode == DIR_LIST) {
      SendDirectoryList();
    } else */

    int select = aSocket->Select();
    if (select & SOCKET_CAN_READ) {
      char recvbuf[DEFAULT_BUFLEN];
      int iResult;
      int recvbuflen = DEFAULT_BUFLEN;
      iResult = aSocket->Receive(recvbuf, recvbuflen);
      if (iResult > 0) {
        //parser.Add(recvbuf, iResult);
        printf("%d read data unexpectedly!:\n%s\n", parser.id, recvbuf);
      } else if (iResult == 0) {
        printf("%d Connection closing...\n", parser.id);
        return false;
      } else {
        aSocket->Close();
        return false;
      }
      return true;
    } else if (!(select & SOCKET_CAN_WRITE)) {
      // Unable to write any more data?
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
      int r = aSocket->Send(buf, x);
      delete buf;
      if (r < 0) {
        // Some kind of error.
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
      int r = aSocket->Send(buf, bytesSent);
      delete buf;
      if (r < 0) {
        // Some kind of error.
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

int _tmain(int argc, _TCHAR* argv[])
{
  RequestParser::Test();
  Response::Test();
  Thread_Test();

  Socket::Init();

  auto_ptr<Socket> listener(Socket::Open(80));
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
