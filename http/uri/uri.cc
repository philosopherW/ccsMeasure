// [RFC 3986] Uniform Resource Identifier (URI)

// related header file
#include "uri.h"
// libraries
#include <ctype.h>
#include <string>
// my header files

using std::string;

// ======== uri encoding/decoding ========
// 10进制数值转换为16进制字符
// argument (do not check)
//   0 - 15
// return value
//   '0' - '9' / 'A' - 'F'
inline char Dig2Hex(int dig)
{
  return ((dig < 10) ? (dig + '0') : (dig - 10 + 'A'));
}

// 16进制字符转换为10进制数值
// argument (do not check)
//   '0' - '9' / 'a' - 'f' / 'A' - 'F'
// return value
//   0 - 15
inline int Hex2Dig(char hex)
{
  return (isdigit(hex) ? (hex - '0') : (toupper(hex) - 'A' + 10));
}

// 基于percent-encoding，将octet转化为character triplet
inline string PctEncoding(char octet)
{
  return string("%") + Dig2Hex(octet/16) + Dig2Hex(octet%16);
}

// 基于percent-encoding，将character triplet转化为octet
// argument (do not check)
//   "%" HEXDIG HEXDIG ; ABNF
inline char PctDecoding(const string &str)
{
  return Hex2Dig(str[1]) * 16 + Hex2Dig(str[2]);
}

// 将raw中除unreserved characters外的字符都替换为percent-encoding
string UriEncoding(const string &raw)
{
  string encoded;
  for (string::size_type i = 0; i != raw.size(); ++i)
    if (isalnum(raw[i]) ||
        raw[i] == '-' ||
        raw[i] == '.' ||
        raw[i] == '_' ||
        raw[i] == '~')
      encoded += raw[i];
    else
      encoded += PctEncoding(raw[i]);
  return encoded;
}

// 将encoded中所有percent-encoding替换为octet
// return value
//   success: raw string
//   error: empty string ('%'后跟的不是2个16进制字符，包括提前到达字符串尾)
string UriDecoding(const string &encoded)
{
  string raw, tmp;
  string::size_type i = 0;
  while (i != encoded.size())
    if (encoded[i] == '%') {
      tmp = encoded.substr(i, 3);
      if (tmp.size() != 3 ||
          !isxdigit(tmp[1]) ||
          !isxdigit(tmp[2]))
        return "";
      raw += PctDecoding(tmp);
      i += 3;
    } else {
      raw += encoded[i];
      ++ i;
    }
  return raw;
}


// ======== class Uri ========
Uri::Uri(std::string arg)
    : uri(arg)
{
  string::size_type pbegin, pend;
  // scheme (不检查scheme词法)
  if ( (pend = uri.find(":")) == string::npos)
    return;
  for (pbegin = 0; pbegin != pend; ++pbegin)
    scheme += string(1, tolower(uri[pbegin]));
  // hier-part
  ++ pbegin;  // pbegin现在指向hier-part
  // |-"//"
  if (uri[pbegin] != '/' || uri[pbegin+1] != '/')
    return;
  pbegin += 2;  // pbegin现在指向authority
  // |-authority
  string authority;
  if ( (pend = uri.find_first_of("/?#", pbegin)) == string::npos) {
    authority = uri.substr(pbegin);
  } else {
    authority = uri.substr(pbegin, pend - pbegin);
    from_path = uri.substr(pend);
  }
  //   |-host
  if ( (pbegin = authority.find("@")) == string::npos)
    pbegin = 0;
  else
    ++ pbegin;
  if ( (pend = authority.rfind(":")) == string::npos)
    pend = authority.size();
  host = authority.substr(pbegin, pend - pbegin);
}
