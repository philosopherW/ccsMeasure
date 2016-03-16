// related header file
#include "http_session.h"
// network
#include <arpa/inet.h>   // inet_pton()
#include <netdb.h>       // gethostbyname()
#include <netinet/in.h>  // sockaddr_in; htons()
#include <openssl/ssl.h>
#include <sys/socket.h>
// others (c, c++)
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <vector>
// my header files
#include "error_code.h"
#include "tools.h"
#include "uri.h"

using std::string;
using std::vector;

const int HttpSession::timed_out_second = 300;

// public methods =========================================================
// url has the following pattern
//   http/https://[domain/ip][filepath]
HttpSession::HttpSession(const string &method_arg, Uri uri)
    : method(method_arg)
{
  // protocol
  if (uri.GetScheme() == "https")
    use_ssl = true;
  else if (uri.GetScheme() == "http")
    use_ssl = false;
  else
    ;  // 忽略。。。
  // domain/ip
  if (IsIp(uri.GetHost()))
    server_ip = uri.GetHost();
  else
    server_domain = uri.GetHost();
  // filepath
  filepath = uri.GetFromPath();
}

void HttpSession::AddHeader(const string &key, const string &value)
{
  request_header_fields += key + ": " + value + "\r\n";
}

int HttpSession::SendBody(const char *buf, int len)
{
  if ( (int ret = WaitState(ClientSessionState::RequestHeadSent)) < 0)
    return ret;

  return WriteNet(buf, len);
}

int HttpSession::GetStatusCode(string &status_code_arg)
{
  if ( (int ret = WaitState(ClientSessionState::ResponseHeadRecved)) < 0)
    return ret;

  status_code_arg = status_code;
  return SUCCESS;
}

int HttpSession::GetHeader(const string &key, string &value)
{
  if ( (int ret = WaitState(ClientSessionState::ResponseHeaderRecved)) < 0)
    return ret;

  string::size_type pos, len;
  string ekey = string("\r\n") + key + ":";
  if ( (pos = response_header_fields.find(ekey)) != string::npos) {
    pos = response_header_fields.find(":", pos) + 1;
    while (response_header_fields[pos] == ' ')
      ++pos;
    len = response_header_fields.find("\r\n", pos) - pos;
    value = response_header_fields.substr(pos, len);
  } else {
    value = "";  // 找不到不返回error；不要随便返回error
  }
  return SUCCESS;
}

int HttpSession::RecvBodyStr(string &body_str)
{
  if ( (int ret = WaitState(ClientSessionState::ResponseHeaderRecved)) < 0)
    return ret;

  body_str = "";
  // 处理response_body_cache
  for (; body_begin != response_body_cache.size(); ++body_begin)
    body_str += string(1, response_body_cache[body_begin]);
  // 根据body类型确定body完结标志
  string value;
  if ( (int ret = GetHeader("Transfer-Encoding", value)) < 0)
    return ret;
  if (value == "chunked") {
    // 遇到"\r\n\r\n"则body结束
    char buf[65536];
    int nread;
    while (body_str.find("\r\n\r\n") == string::npos) {
      if ( (nread = ReadNet(buf, sizeof(buf)-1)) < 0)
        return nread;
      buf[nread] = '\0';
      body_str += buf;
    }
    return SUCCESS;
  } else {
    // 根据Content-Length确定body是否读完
    if ( (int ret = GetHeader("Content-Length", value)) < 0)
      return ret;
    if (value == "")
      return RECVBODY_UNKNOWNFORMAT;
    int ntotal = StrToInt(value);
    char buf[65536];
    int nread;
    while (body_str.size() < ntotal) {
      if ( (nread = ReadFromServer(buf, sizeof(buf)-1)) < 0)
        return nread;
      buf[nread] = '\0';
      body_str += buf;
    }
    return SUCCESS;
  }
}

int HttpSession::PassBody()
{
  if ( (int ret = WaitState(ClientSessionState::ResponseHeaderRecved)) < 0)
    return ret;

  // 根据body类型确定body完结标志
  string value;
  if ( (int ret = GetHeader("Transfer-Encoding", value)) < 0)
    return ret;
  if (value == "chunked") {
    // 遇到"\r\n\r\n"则传输完成。注意不能用string来保存，因body不一定是字符串，可能包含0，读到buf中的内容可能被截断后接到string中，若"\r\n\r\n"被截断则下一次ReadNet()会超时错误
    char buf[65536];
    int len = 0;
    while (; body_begin != response_body_cache.size(); ++body_begin, ++len)
      buf[len] = response_body_cache[body_begin];
    int nread;
    while (MemFind(buf, len, "\r\n\r\n", 4) == -1) {
      if (len > 3) {
        for (int i = 0; i < 3; ++i)
          buf[i] = buf[len-3+i];
        len = 3;
      }
      if ( (nread = ReadNet(buf+len, sizeof(buf)-len)) < 0)
        return nread;
      len += nread;
    }
    return SUCCESS;
  } else {
    // 根据Content-Length确定body是否读完
    if ( (int ret = GetHeader("Content-Length", value)) < 0)
      return ret;
    if (value == "")
      return RECVBODY_UNKNOWNFORMAT;
    int nleft = StrToInt(value) - (response_body_cache.size() - body_begin);
    char buf[65536];
    int nread;
    while (nleft > 0) {
      if ( (nread = ReadFromServer(buf, sizeof(buf))) < 0)
        return nread;
      nleft -= nread;
    }
    return SUCCESS;
  }
}

// private methods ========================================================
int HttpSession::WaitState(ClientSessionState dest_state)
{
  int ret;
  switch (client_state) {
    case ClientSessionState::Origin:
      if (client_state == dest_state)
        return SUCCESS;
      else  // 必然存在client_state < dest_state
        if ( (ret = ConnectServer()) < 0)
          return ret;
        else
          client_state = ClientSessionState::Connected;

    case ClientSessionState::Connected:
      if (client_state == dest_state)
        return SUCCESS;
      else if (client_state < dest_state)
        if ( (ret = SendHead()) < 0)
          return ret;
        else
          client_state = ClientSessionState::RequestHeaderSent;
      else
        return WAITSTATE_TOOSMALL;

    case ClientSessionState::RequestHeaderSent:
      if (client_state == dest_state)
        return SUCCESS;
      else if (client_state < dest_state)
        if ( (ret = RecvHead()) < 0)
          return ret;
        else
          client_state = ClientSessionState::ResponseHeaderRecved;
      else
        return WAITSTATE_TOOSMALL;

    case ClientSessionState::ResponseHeaderRecved:
      if (client_state == dest_state)
        return SUCCESS;
      else  // 必然存在client_state > dest_state
        return WAITSTATE_TOOSMALL;

    default:
      return WAITSTATE_INVALIDCURRENTSTATE;
  }
}

int TimedSslConnect(SSL *p_ssl, int timed_out_second);
int HttpSession::ConnectServer()
{
  // create socket (fd)
  if ( (fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    return SOCKET_ERROR;
  // server address (server_addr)
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(use_ssl?443:80);
  if (server_ip.empty()) {
    struct hostent *p_hostent;
    if ( (p_hostent = gethostbyname(server_domain.c_str())) == NULL)
      return GETHOSTBYNAME_ERROR;
    server_addr.sin_addr = *(struct in_addr *)*(p_hostent->h_addr_list);
  } else {
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0)
      return INET_PTON_ERROR;
  }
  // TCP connect (need fd and server_addr)
  if (connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    return CONNECT_ERROR;  // TODO 用一个数据成员保存errno
  // SSL connect
  if (use_ssl) {
    SSL_load_error_strings();
    SSL_library_init();
    if ( (p_ssl_ctx = SSL_CTX_new(SSLv23_client_method())) == NULL)
      return SSL_CTX_NEW_ERROR;
    if ( (p_ssl = SSL_new(p_ssl_ctx)) == NULL)
      return SSL_NEW_ERROR;
    if (SSL_set_fd(p_ssl, fd) == 0)
      return SSL_SET_FD_ERROR;
    if ( (int ret = TimedSslConnect(p_ssl, timed_out_second)) < 0)
      return ret;
  }

  return SUCCESS;
}

int HttpSession::SendHead()
{
  if (!server_domain.empty())
    AddHeader("Host", server_domain);
  string head = method + " " + filepath + " HTTP/1.1\r\n" + request_header_fields + "\r\n";
  return WriteToServer(head.c_str(), head.size());
}

int HttpSession::RecvHead()
{
  string tmpstr;
  char tmpbuf[65536];
  int nread;
  string::size_type pos;
  while (true) {
    if ( (nread = ReadFromServer(tmpbuf, sizeof(tmpbuf)-1)) < 0)
      return nread;
    for (int i = 0; i < nread; ++i)
      response_body_cache.push_back(tmpbuf[i]);
    tmpbuf[nread] = '\0';
    tmpstr += tmpbuf;  // 如果读到body，由于body不一定是字符串，因此可能包含内容中的0，这种情况下tmpbuf中的内容不会全部添加到tmpstr的尾部。但tmpstr用来处理head还是可以的

    if ( (pos = tmpstr.find("\r\n\r\n")) != string::npos) {
      // status_code
      auto status_code_pos = tmpstr.find(" ") + 1;
      auto status_code_len = tmpstr.find(" ", status_code_pos) - status_code_pos;
      status_code = tmpstr.substr(status_code_pos, status_code_len);
      // reason_message
      auto reason_message_pos = status_code_pos + status_code_len + 1;
      auto reason_message_len = tmpstr.find("\r\n") - reason_message_pos;
      reason_phrase = tmpstr.substr(reason_message_pos, reason_message_len);
      // response_header_fields
      auto response_headers_pos = reason_message_pos + reason_message_len + 2;
      auto response_headers_len = pos + 2 - response_headers_pos;
      response_header_fields = tmpstr.substr(response_headers_pos, response_headers_len);
      // response_body_cache
      body_begin = pos+4;

      return SUCCESS;
    } else {
      continue;  // continue to read
    }
  }
}

struct ReadWriteArg;
static bool write_success;
void *ThreadSslWrite(void *p_write_arg);
void *ThreadPlainWrite(void *p_write_arg);
// write len bytes from buf to network, time limited
// return value
//   success: SUCCESS
//   error: error code
int HttpSession::WriteNet(const char *buf, int len)
{
  write_success = false;

  struct ReadWriteArg write_arg;
  if (use_ssl)
    write_arg.p_ssl = p_ssl;
  else
    write_arg.fd = fd;
  write_arg.buf_src = buf;
  write_arg.len = len;

  pthread_t tid;
  if (pthread_create(&tid, NULL, (use_ssl?ThreadSslWrite:ThreadPlainWrite), &write_arg) != 0)
    return WRITETOSERVER_PTHREAD_CREATE_ERROR;

  time_t time_begin = time(NULL);
  int ret;
  while (true) {
    ret = pthread_kill(tid, 0);
    if (ret == ESRCH)
      if (write_success) {
        pthread_join(tid, NULL);
        return SUCCESS;
      } else {
        return WRITETOSERVER_ERROR;
      }
    else if (ret == EINVAL)
      return WRITETOSERVER_PTHREAD_KILL_INVALID_SIGNAL;
    else if ((time(NULL) - time_begin) >= timed_out_second)
      return WRITETOSERVER_TIMED_OUT;
  }
}

static bool read_success;
void *ThreadSslRead(void *p_read_arg);
void *ThreadPlainRead(void *p_read_arg);
// read as much size bytes into buf, time limited
// return value
//   success: number of bytes (>= 0)
//   error: error code (< 0)
int HttpSession::ReadNet(char *buf, int size)
{
  read_success = false;
  
  struct ReadWriteArg read_arg;
  if (use_ssl)
    read_arg.p_ssl = p_ssl;
  else
    read_arg.fd = fd;
  read_arg.buf_dest = buf;
  read_arg.size = size;

  pthread_t tid;
  if (pthread_create(&tid, NULL, (use_ssl?ThreadSslRead:ThreadPlainRead), &read_arg) != 0)
    return READFROMSERVER_PTHREAD_CREATE_ERROR;

  time_t time_begin = time(NULL);
  int ret;
  while (true) {
    ret = pthread_kill(tid, 0);
    if (ret == ESRCH)
      if (read_success) {
        pthread_join(tid, NULL);
        return read_arg.nread;
      } else {
        return READFROMSERVER_ERROR;
      }
    else if (ret == EINVAL)
      return READFROMSERVER_PTHREAD_KILL_INVALID_SIGNAL;
    else if ((time(NULL) - time_begin) >= timed_out_second)
      return READFROMSERVER_TIMED_OUT;
  }
}

// implementations unrelated to class =====================================
// HttpSession::ConnectServer()
static bool ssl_connect_success;
void *ThreadSslConnect(void *p_ssl)
{
  if (SSL_connect((SSL *)p_ssl) == 1)
    ssl_connect_success = true;
  pthread_exit((void *)0);
}
int TimedSslConnect(SSL *p_ssl, int timed_out_second)
{
  ssl_connect_success = false;
  // create thread
  pthread_t tid;
  if (pthread_create(&tid, NULL, ThreadSslConnect, p_ssl) != 0)
    return TIMEDSSLCONNECT_PTHREAD_CREATE_ERROR;
  // check
  time_t time_begin = time(NULL);
  int ret;
  while (true) {
    ret = pthread_kill(tid, 0);
    if (ret == ESRCH)
      if (ssl_connect_success) {
        pthread_join(tid, NULL);
        return SUCCESS;  // TODO error code宏定义加前缀HTTP_SESSION
      } else
        return SSL_CONNECT_ERROR;
    else if (ret == EINVAL)
      return PTHREAD_KILL_INVALID_SIGNAL_SSL_CONNECT;
    else if ((time(NULL) - time_begin) >= timed_out_second)
      return SSL_CONNECT_TIMED_OUT;
  }
}

// HttpSession::WriteNet() & HttpSession::ReadNet()
struct ReadWriteArg {
  int fd = -1;
  SSL *p_ssl = NULL;
  const char *buf_src = NULL;
  int len = 0;
  char *buf_dest = NULL;
  int size = 0;
  int nread = 0;
};

// HttpSession::WriteNet()
void *ThreadSslWrite(void *p_write_arg)
{
  SSL *p_ssl = ((struct ReadWriteArg *)p_write_arg)->p_ssl;
  const char *ptr = ((struct ReadWriteArg *)p_write_arg)->buf_src;
  int nleft = ((struct ReadWriteArg *)p_write_arg)->len;
  int nwritten;
  while (nleft > 0) {
    if ( (nwritten = SSL_write(p_ssl, ptr, nleft)) <= 0)
      pthread_exit((void *)0);
    nleft -= nwritten;
    ptr += nwritten;
  }
  write_success = true;
  pthread_exit((void *)0);
}
void *ThreadPlainWrite(void *p_write_arg)
{
  int fd = ((struct ReadWriteArg *)p_write_arg)->fd;
  const void *ptr = ((struct ReadWriteArg *)p_write_arg)->buf_src;
  int nleft = ((struct ReadWriteArg *)p_write_arg)->len;
  int nwritten;
  while (nleft > 0) {
    if ( (nwritten = write(fd, ptr, nleft)) <= 0)
      if (nwritten < 0 && errno == EINTR)
        nwritten = 0;
      else
        pthread_exit((void *)0);
    nleft -= nwritten;
    ptr += nwritten;
  }
  write_success = true;
  pthread_exit((void *)0);
}

// HttpSession::ReadNet()
void *ThreadSslRead(void *p_read_arg)
{
  SSL *p_ssl = ((struct ReadWriteArg *)p_read_arg)->p_ssl;
  void *buf = ((struct ReadWriteArg *)p_read_arg)->buf_dest;
  int size = ((struct ReadWriteArg *)p_read_arg)->size;
  int nread;
  if ( (nread = SSL_read(p_ssl, buf, size)) <= 0) {
    pthread_exit((void *)0);
  } else {
    ((struct ReadWriteArg *)p_read_arg)->nread = nread;
    read_success = true;
    pthread_exit((void *)0);
  }
}
void *ThreadPlainRead(void *p_read_arg)
{
  int fd = ((struct ReadWriteArg *)p_read_arg)->fd;
  void *buf = ((struct ReadWriteArg *)p_read_arg)->buf_dest;
  int size = ((struct ReadWriteArg *)p_read_arg)->size;
  int nread;
  if ( (nread = read(fd, buf, size)) < 0) {
    if (errno == EINTR) {
      ((struct ReadWriteArg *)p_read_arg)->nread = 0;
      read_success = true;
      pthread_exit((void *)0);
    } else {
      pthread_exit((void *)0);
    }
  } else if (nread == 0) {
    pthread_exit((void *)0);
  } else {
    ((struct ReadWriteArg *)p_read_arg)->nread = nread;
    read_success = true;
    pthread_exit((void *)0);
  }
}
