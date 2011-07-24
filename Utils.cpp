#include "Utils.h"
#include <stdio.h>

string ToString(int64_t i) {
  char buf[256];
  snprintf(buf, 256, "%lld", i);
  return string(buf);
}

string ToString(int i) {
  char buf[12];
  snprintf(buf, 12, "%d", i);
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