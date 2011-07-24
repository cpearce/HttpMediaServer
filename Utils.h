#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef _WIN32
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#define atoll _atoi64
#define snprintf sprintf_s
#else
#include <stdint.h>
#endif

#include <string>
#include <map>
#include <vector>

using std::map;
using std::string;
using std::vector;

string ToString(int64_t i);
string ToString(int i);

string Flatten(const map<string, string>& m);

bool ContainsKey(const map<string,string> m, const string& key);

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
              const string& delimiters);

#define ARRAY_LENGTH(array_) \
  (sizeof(array_)/sizeof(array_[0]))

#define DEFAULT_BUFLEN 512

#endif