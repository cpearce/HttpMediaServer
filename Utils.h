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

#include <algorithm>
#include <string>
#include <map>
#include <vector>
#include <iostream>

using std::map;
using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;

string ToString(int64_t i);
string ToString(int i);

string Flatten(const map<string, string>& m);

bool ContainsKey(const map<string,string> m, const string& key);

inline string& StrToLower(string& s) {
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  return s;
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
              const string& delimiters);

#define ARRAY_LENGTH(array_) \
  (sizeof(array_)/sizeof(array_[0]))

#define DEFAULT_BUFLEN 512

#endif