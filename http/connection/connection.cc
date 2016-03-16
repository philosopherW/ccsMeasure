// related header
#include "connection.h"
// network
#include <arpa/inet.h>   // inet_pton()
#include <netinet/in.h>  // sockaddr_in; htons()
#include <openssl/ssl.h>
#include <sys/socket.h>
// c/c++ headers
#include <sys/types.h>
#include <string>
// my headers
#include "../../tools.h"  // IsIp()

using std::string;

const int Connection::timed_out_second = 300;

// ======== public methods ========
int TimedSslConnect(SSL *p_ssl, int timed_out_second);
int Connection::Connect(const string &scheme, const std::string &ip, int port)
{
  if (state != State::Noconnection)
    return HTTP_CONNECTION_RECONNECT;
  if (scheme != "http" || scheme != "https")
    return HTTP_CONNECTION_UNKNOWN_SCHEME;
  if (port < 0 || port > 65535)
    return HTTP_CONNECTION_INVALID_PORT;
  // server address (server_addr)
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) != 1)
    return HTTP_CONNECTION_INET_PTON_ERROR;
  // create socket (fd)
  if ( (fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    return HTTP_CONNECTION_SOCKET_ERROR;
  // TCP connect (need fd and server_addr)
  if (connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    return HTTP_CONNECTION_CONNECT_ERROR;
  // SSL connect
  if (scheme == "https") {
    SSL_load_error_strings();
    SSL_library_init();
    if ( (p_ssl_ctx = SSL_CTX_new(SSLv23_client_method())) == NULL)
      return HTTP_CONNECTION_SSL_CTX_NEW_ERROR;
    if ( (p_ssl = SSL_new(p_ssl_ctx)) == NULL)
      return HTTP_CONNECTION_SSL_NEW_ERROR;
    if (SSL_set_fd(p_ssl, fd) == 0)
      return HTTP_CONNECTION_SSL_SET_FD_ERROR;
    if ( (int ret = TimedSslConnect(p_ssl, timed_out_second)) < 0)
      return ret;
  }
  state = ((scheme == "http") ? State::Tcp : State::TlsTcp);
  return SUCCESS;
}

ssize_t Connection::Read(char *buf, size_t size)
{
  if (state == State::Noconnection)
    return -1;
}

ssize_t Connection::Write(const char *buf, size_t len)
{
  if (state == State::Noconnection)
    return -1;
}

// ======== implementations ========
static bool ssl_connect_success;
static void *ThreadSslConnect(void *p_ssl)
{
  if (SSL_connect((SSL *)p_ssl) == 1)
    ssl_connect_success = true;
  pthread_exit((void *)0);
}
static int TimedSslConnect(SSL *p_ssl, int timed_out_second)
{
  ssl_connect_success = false;
  // create thread
  pthread_t tid;
  if (pthread_create(&tid, NULL, ThreadSslConnect, p_ssl) != 0)
    return HTTP_CONNECTION_TIMEDSSLCONNECT_PTHREAD_CREATE_ERROR;
  // check
  time_t time_begin = time(NULL);
  int ret;
  while (true) {
    ret = pthread_kill(tid, 0);
    if (ret == ESRCH)
      if (ssl_connect_success) {
        pthread_join(tid, NULL);
        return SUCCESS;
      } else {
        return HTTP_CONNECTION_SSL_CONNECT_ERROR;
      }
    else if (ret == EINVAL)
      return HTTP_CONNECTION_TIMEDSSLCONNECT_PTHREAD_KILL_INVALID_SIGNAL;
    else if ((time(NULL) - time_begin) >= timed_out_second)
      return HTTP_CONNECTION_SSL_CONNECT_TIMED_OUT;
  }
}
