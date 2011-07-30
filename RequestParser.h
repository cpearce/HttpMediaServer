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

#ifndef __REQUEST_PARSER_H__
#define __REQUEST_PARSER_H__

#include "Utils.h"

enum eMethod { UNKNOWN, HEAD, GET, POST };

class RequestParser {
public:
  RequestParser();

  void Add(const char* buf, unsigned len);

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

  static void Test();

  void GetRange(int64_t& start, int64_t& end) const;

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
                        int64_t expEnd);

  void Parse(const string& s);

  static bool ParseRange(const string& s, int64_t& start, int64_t& end);

  static eMethod ExtractMethod(const string& request);

  static string ExtractTarget(const string& request);

  static map<string, string> ExtractQueryParams(const string& request);

  void ParseRequestLine(const string& request);
  
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

#endif
