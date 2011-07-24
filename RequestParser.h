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
