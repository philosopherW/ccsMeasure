// [RFC 3986] Uniform Resource Identifier (URI)

#ifndef CCSMEASURE_HTTP_URI_H_
#define CCSMEASURE_HTTP_URI_H_

#include <string>

// 生成URI前对component进行UriEncoding，分解URI后对component进行UriDecoding
// 理解：URI由delimiter分隔component组成，其中delimiter从reserved characters中选取。component原始内容可能包含reserved characters，为避免其被理解为delimiter，因此用percent-encode进行替换。即percent-encode存在的意义是在(且仅在)URI中区分用作delimiter的和用作component一部分的reserved characters。至于unreserved characters，其在URI中无论是原始形式或是percent-encode都表示相同的意义，即普通字符，则编码时可以跳过，解码时对所有percent-encode解码即可
// 注：需要机器字符集为ASCII
// 将raw中所有非unreserved characters转化为percent-encode
std::string UriEncoding(const std::string &raw);
// 将encoded中所有percent-encode转化为character
std::string UriDecoding(const std::string &encoded);



// URI = scheme ":" hier-part [ "?" query ] { "#" fragment }
//
// scheme = ALPHA *( ALPHT / DIGIT / "+" / "-" / "." ) ; case-insensitive, the canonical form is lowercase, an implementation should accept uppercase
//
// hier-part = "//" authority path-abempty ; if a URI does not contain an authority component, the the path cannot begin with "//"
//           / path-absolute
//           / path-rootless
//           / path-empty
//
//   authority = [ userinfo "@" ] host [ ":" port ] ; preceded by a "//" and is terminatd by the next "/", "?", or "#", or by the end of the URI
//
//     userinfo = *( unreserved / pct-encoded / sub-delims / ":" ) ; is followed by a "@" that delimits it from the host
//
//     host = IP-literal / IPv4address / reg-name ; case-insensitive. authority的左1"@"和右1":"之间
//
//       IP-literal = "[" ( IPv6address / IPvFuture ) "]" ; this is the only place where "[" and "]" are allowed in the URI syntax
//
//       IPv4address = dec-octet "." dec-octet "." dec-octet "." dec-octet ; dec-octet表示0-255的十进制的字符串
//
//       reg-name = *( unreserved / pct-encoded / sub-delims ) ; the most common name registry mechanism is the DNS
//
//     port = *DIGIT
//
//   path-abempty = *( "/" segment )

// URI parser
class Uri {
 public:
  Uri(std::string);
  Uri(const char *arg) : Uri(std::string(arg)) { }
  ~Uri() = default;
  Uri(const Uri &) = default;
  Uri &operator=(const Uri &) = default;

  std::string GetUri() const { return uri; }
  std::string GetScheme() const { return scheme; }
  std::string GetHost() const { return host; }
  std::string GetFromPath() const { return from_path; }

 private:
  const std::string uri;
  std::string scheme;
  std::string host;
  std::string from_path;
};

#endif  // CCSMEASURE_HTTP_URI_H_
