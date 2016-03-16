#ifndef CCSMEASURE_HTTP_HTTP_SESSION_H_
#define CCSMEASURE_HTTP_HTTP_SESSION_H_

#include <openssl/ssl.h>
#include <string>  // 处理字符串用string，处理内存块用char数组
#include <vector>  // 大小不定数组用vector

// http session定义: 一次http message的交换
// 不自动处理3XX重定向，交给用户发现并重新创建http session
// TODO: 网络资源复用
//   提供接管网络资源的method TakeResource()
//   自动管理
class HttpSession {
 public:
  // 提供method和url创建session
  HttpSession(const std::string &, const std::string &);

  // TODO: check fd, ssl, ...
  ~HttpSession() = default;

  // 阻止拷贝（构造和赋值）
  HttpSession(const HttpSession &) = delete;
  HttpSession &operator=(const HttpSession &) = delete;

  // 添加request header
  void AddHeader(const std::string &, const std::string &);

  // 发送request body; body不一定是字符串，不能用string
  int SendBody(const char *, int);

  // 返回response的status code
  int GetStatusCode(std::string &);

  // 返回response的指定header的value
  int GetHeader(const std::string &, std::string &);

  // 接收body，仅用于确定body为字符串的情况
  int RecvBodyStr(std::string &body_str);

  // 接收（但不保存）body
  // return: body字节总数
  int PassBody();

  // 接管argument的网络连接，状态变为Connected
  int TakeResource(HttpSession &);

 private:
  enum class ClientSessionState {Origin, Connected, RequestHeadSent, ResponseHeadRecved};

  static const int timed_out_second;

  // 等待指定的client state
  int WaitState(ClientSessionState);

  // 与server建立连接
  int ConnectServer();

  // 发送request head (request line + headers)
  int SendHead();

  // 接收response head (status line + headers)
  int RecvHead();

  // 将buf中len个字节写到网络中
  int WriteNet(const char *buf, int len);

  // 从网络读最多size字节到buf
  // return value: 读到的字节数
  int ReadNet(char *buf, int size);

  // request message & response message
  std::string method;                  // request method
  std::string filepath;                // request filepath (URI)
  std::string request_header_fields;   // request header fields
  std::string status_code;             // response status code
  std::string reason_phrase;           // response reason message
  std::string response_header_fields;  // response header fields
  std::vector<char> response_body_cache;
  std::vector<char>::size_type body_begin = -1;  // 初始值应该用最大值

  // session info
  ClientSessionState client_state = ClientSessionState::Origin;
  std::string server_domain;
  std::string server_ip;
  bool use_ssl = false;

  // resource handle
  int fd = -1;  // 当前session所使用的TCP连接
  SSL_CTX *p_ssl_ctx = NULL;
  SSL *p_ssl = NULL;
};

#endif // CCSMEASURE_HTTP_HTTP_SESSION_H_
