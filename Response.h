#ifndef __RESPONSE_H__
#define __RESPONSE_H__

#include "Utils.h"
#include "RequestParser.h"
#include "Sockets.h"

class Response {

  enum eMode {
    GET_ENTIRE_FILE,
    GET_FILE_RANGE,
    DIR_LIST,
    ERROR_FILE_NOT_EXIST,
    INTERNAL_ERROR
  };

public:
  Response(RequestParser p);

  bool SendHeaders(Socket* aSocket);

  // Returns true if we need to call again.
  bool SendBody(Socket *aSocket);

  static void Test();

private:

  static string StatusCode(eMode mode);

  static string ExtractContentType(const string& file, eMode mode);

  static string GetDate();

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

#endif