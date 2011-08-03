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

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <iostream>
#include <sstream>

#include "PathEnumerator.h"
#include "Response.h"
#include "Utils.h"

#ifdef _WIN32
#include <Windows.h>
#define S_ISDIR(m) ((m & _S_IFDIR) == _S_IFDIR)
#define fseek64 _fseeki64
#define ftell64 _ftelli64

static int gmtime_r(const time_t *timep, struct tm *result) {
  return gmtime_s(result, timep) == 0;
}

#else
#define __stat64 stat64
#define _stat64 stat64
#include <unistd.h>
#define fseek64 fseeko64
#define ftell64 ftello64

void Sleep(int ms) {
  usleep(ms * 1000);
}

#endif

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define DEFAULT_BUFLEN 512


static const char* gContentTypes[][2] = {
  {"ogv", "video/ogg"},
  {"ogg", "video/ogg"},
  {"oga", "audio/ogg"},
  {"webm", "video/webm"},
  {"wav", "audio/x-wav"},
  {"html", "text/html; charset=utf-8"},
  {"txt", "text/plain; charset=utf-8"},
  {"js", "text/javascript; charset=utf-8"},
  {"js", "text/css; charset=utf-8"},
  {"jpg", "image/jpeg"},
  {"jpeg", "image/jpeg"},
  {"png", "image/png"},
  {"gif", "image/gif"}
};

Response::Response(RequestParser p)
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
    path = ".";
  } else if (target.find("..") != string::npos) {
    mode = ERROR_FILE_NOT_EXIST;
  } else {
    // Determine if the file exists, and if it is a directory.
    struct __stat64 buf;
    int result;
    result = _stat64(target.c_str(), &buf );
    if (result == -1) {
      std::cerr << "File not found" << std::endl;
      mode = ERROR_FILE_NOT_EXIST;
    } else if (result != 0) {
      mode = INTERNAL_ERROR;
    } else if (S_ISDIR(buf.st_mode)) {
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

bool Response::SendHeaders(Socket* aSocket) {
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

  std::cout << "Sending Headers " << parser.id << std::endl << headers;

  return aSocket->Send(headers.c_str(), (int)headers.size()) != -1;
}

// Returns true if we need to call again.
bool Response::SendBody(Socket *aSocket) {
  if (mode == ERROR_FILE_NOT_EXIST) {
    std::cout << "Sent (empty) body (" << parser.id << ")" << std::endl;
    return false;
  }

  int len = 1024;
  unsigned wait = 0;
  string rateStr;
  if (ContainsKey(parser.GetParams(), "rate")) {
    const map<string,string> params = parser.GetParams();
    rateStr = params.find("rate")->second;
    double rate = atof(rateStr.c_str());
    const double period = 0.1;
    if (rate <= 0.0) {
      len = 1024;
      wait = 0;
    } else {
      len = (unsigned)(rate * 1024 * period);
      wait = (unsigned)(period * 1000.0); // ms
    }
  }

  if (mode == GET_ENTIRE_FILE) {
    if (!file) {
#ifdef _WIN32
      if (fopen_s(&file, path.c_str(), "rb")) {
        file = 0;
        return false;
      }
#else
      file = fopen(path.c_str(), "rb");
      if (!file) {
        return false;
      }
#endif
    }
    if (feof(file)) {
      // Transmitted entire file!
      fclose(file);
      file = 0;
      return false;
    }

    int64_t tell = ftell64(file);

    // Transmit the next segment.
    char* buf = new char[len];
    int x = (int)fread(buf, 1, len, file);
    int r = aSocket->Send(buf, x);
    delete buf;
    if (r < 0) {
      // Some kind of error.
      return false;
    }
      
    if (wait > 0) {
      Sleep(wait);
    }
    // Else we tranmitted that segment, we're ok.
    return true;

  } else if (mode == GET_FILE_RANGE) {
    if (!file) {
#ifdef _WIN32
      if (fopen_s(&file, path.c_str(), "rb")) {
        file = 0;
        return false;
      }
#else
      file = fopen(path.c_str(), "rb");
      if (!file) {
        return false;
      }
#endif
      fseek64(file, rangeStart, SEEK_SET);
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

    len = (unsigned)MIN(bytesRemaining, len);
    size_t bytesSent = fread(buf, 1, len, file);
    bytesRemaining -= bytesSent;
    int r = aSocket->Send(buf, (int)bytesSent);
    delete buf;
    if (r < 0) {
      // Some kind of error.
      return false;
    }
    offset += bytesSent;
    assert(ftell64(file) == offset);

    if (wait > 0) {
      Sleep(wait);
    }

    // Else we tranmitted that segment, we're ok.
    return true;
  }
  else if (mode == DIR_LIST) {
    std::stringstream response;
    PathEnumerator *enumerator = PathEnumerator::getEnumerator(path);
    if (enumerator) {
      response << "<!DOCTYPE html>\n<ul>";
      string href;
      while (enumerator->next(href)) {
        if (href == "." || path == "." && href == "..") {
          continue;
        }
        response << "<li><a href=\"" << path + "/" + href;
        if (!rateStr.empty()) {
          response << "?rate=" + rateStr;
        }
        response << "\">" << href << "</a></li>";
      }
      response << "</ul>";
      delete enumerator;
    }
    string _r = response.str();
    aSocket->Send(_r.c_str(), (int)_r.size());
    return false;
  }

  return false;
}

#ifdef _DEBUG
void Response::Test() {
  assert(ExtractContentType("dir1/dir2/file.ogv", GET_ENTIRE_FILE) == string("video/ogg"));
  assert(ExtractContentType("dir1/dir2/file.ogg", GET_ENTIRE_FILE) == string("video/ogg"));
  assert(ExtractContentType("dir1/dir2/file.oga", GET_ENTIRE_FILE) == string("audio/ogg"));
  assert(ExtractContentType("dir1/dir2/file.wav", GET_ENTIRE_FILE) == string("audio/x-wav"));
  assert(ExtractContentType("dir1/dir2/file.webm", GET_ENTIRE_FILE) == string("video/webm"));
  assert(ExtractContentType("dir1/dir2/file.txt", GET_ENTIRE_FILE) == string("text/plain; charset=utf-8"));
  assert(ExtractContentType("dir1/dir2/file.html", GET_ENTIRE_FILE) == string("text/html; charset=utf-8"));
#ifdef _WIN32
  assert(ExtractContentType("", DIR_LIST) == string("text/html; charset=windows"));
#else
  assert(ExtractContentType("", DIR_LIST) == string("text/html; charset=utf-8"));
#endif
}
#endif


string Response::StatusCode(eMode mode) {
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

string Response::ExtractContentType(const string& file, eMode mode) {
  if (mode == ERROR_FILE_NOT_EXIST || mode == INTERNAL_ERROR)
    return "text/html; charset=utf-8s";
  if (mode == DIR_LIST)
#ifdef _WIN32
    return "text/html; charset=windows";
#else
    return "text/html; charset=utf-8";
#endif

  size_t dot = file.rfind(".");
  if (dot == string::npos) {
    // File has no extension. Just send it as binary.
    return "application/octet-stream";
  }

  string extension(file, dot+1, file.size() - dot - 1);
  StrToLower(extension);

  for (unsigned i=0; i<ARRAY_LENGTH(gContentTypes); i++) {
    if (extension == gContentTypes[i][0]) {
      return string(gContentTypes[i][1]);
    }
  }
  return "application/octet-stream";
}

static char const month[12][4] = {
  "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static char const day[7][4] = {
  "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

string Response::GetDate() {
  time_t rawtime;
  time(&rawtime);
  struct tm t;
  if (!gmtime_r(&rawtime, &t)) {
    return "Date: Thu Jan 01 1970 00:00:00 GMT";
  }

  char buf[128];
  unsigned len = snprintf(buf, ARRAY_LENGTH(buf),
    "Date: %s, %d %s %d %.2d:%.2d:%.2d GMT",
    day[t.tm_wday], t.tm_mday, month[t.tm_mon],
    1900 + t.tm_year, t.tm_hour, t.tm_min, t.tm_sec);

  return string(buf, len);
}
