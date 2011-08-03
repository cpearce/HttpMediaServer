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

#ifdef _DEBUG
  static void Test();
#endif

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