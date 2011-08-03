/*
   Copyright (c) 2011, Nils Maier

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

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

#include "PathEnumerator.h"

using std::string;

#ifdef _WIN32

#include <windows.h>

class Win32PathEnumerator : public PathEnumerator {
private:
  const string mPath;
  mutable HANDLE hFind;
  mutable WIN32_FIND_DATA fad;

  static string initPath(const string& path) {
    string rv(path);

    if (rv.empty()) {
      return string(".\\*");
    }
    rv.append("\\*");
    return rv;
  }

public:
  Win32PathEnumerator(const std::string& aPath)
    : mPath(initPath(aPath)), hFind(0) {}
  virtual ~Win32PathEnumerator() {
    if (hFind) {
      FindClose(hFind);
    }
  }
  virtual bool next(string& file) const {
    if (!hFind) {
      if (!(hFind = FindFirstFile(mPath.c_str(), &fad))) {
        return false;
      }
      file.assign(fad.cFileName);
      return true;
    }

    if (FindNextFile(hFind, &fad)) {
      file.assign(fad.cFileName);
      return true;
    }

    return false;
  }
};

PathEnumerator* PathEnumerator::getEnumerator(const string& path) {
  return new Win32PathEnumerator(path);
}

#else

#include <sys/types.h>
#include <dirent.h>

class PosixPathEnumerator : public PathEnumerator {
private:
  const string mPath;
  mutable DIR* dir;

  static string initPath(const string& path) {
    string rv(path);

    if (rv.empty()) {
      return string(".");
    }
    return rv;
  }

public:
  PosixPathEnumerator(const std::string& aPath)
    : mPath(initPath(aPath)), dir(0) {}
  virtual ~PosixPathEnumerator() {
    if (dir) {
      closedir(dir);
    }
  }
  virtual bool next(string& file) const {
    if (!dir) {
      if (!(dir = opendir(mPath.c_str()))) {
        return false;
      }
    }

    struct dirent *ent;
    if (!(ent = readdir(dir))) {
      return false;
    }
    file.assign(ent->d_name);
    return true;
  }
};


PathEnumerator* PathEnumerator::getEnumerator(const string& path) {
  return new PosixPathEnumerator(path);
}

#endif
