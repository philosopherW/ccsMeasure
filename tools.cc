#include "tools.h"
#include <ctype.h>
#include <stdio.h>
#include <string>

using std::string;

bool IsIp(const string &str)
{
  if (str.empty())
    return false;

  if (!isdigit(str[0]) || !isdigit(str[str.size()-1]))
    return false;

  int dot_count = 0;
  for (int i = 0; i < str.size(); ++i)
    if (isdigit(str[i]))
      continue;
    else if (str[i] == '.')
      if (str[i-1] != '.')
        ++dot_count;
      else
        return false;
    else
      return false;
  
  if (dot_count != 3)
    return false;

  int a, b, c, d;
  sscanf(str.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d);
  if (a >= 0 && a <= 255 &&
      b >= 0 && b <= 255 &&
      c >= 0 && c <= 255 &&
      d >= 0 && d <= 255)
    return true;
  else
    return false;
}

string IntToStr(int num)
{
  char buf[20];
  snprintf(buf, sizeof(buf), "%d", num);
  return buf;
}

// TODO: check validity of str
int StrToInt(string str)
{
  int ret;
  sscanf(str.c_str(), "%d", &ret);
  return ret;
}

int MemFind(const char *buf, int buf_len, const char *key, int key_len)
{
  int nfound;
  for (int pos = 0; pos <= buf_len - key_len; ++pos) {
    nfound = 0;
    while (buf[pos+nfound] == key[nfound]) {
      ++nfound;
      if (nfound == key_len)
        return pos;
    }
  }
  return -1;
}
