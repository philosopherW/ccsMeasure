#ifndef CCSMEASURE_HTTP_CONNECTION_H_
#define CCSMEASURE_HTTP_CONNECTION_H_

#include <sys/types.h>  // size_t, ssize_t
#include <string>

class Connection {
 public:
  Connection(const std::string &scheme_arg, const std::string &host_arg)
      : scheme(scheme_arg), host(host_arg) {}
  ~Connection() = default;
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;

  // read up to count bytes from connection into buf
  ssize_t Read(char *buf, size_t count);

  // write count bytes from buf to connection
  ssize_t Write(const char *buf, size_t count);

  //
  int Close();

 private:
  enum class State = {None, Tcp, TlsTcp};

  static const int wait_time;  // time out

  // check scheme, "http" or "https"; determine port number, http-80, https-443
  // check host, find existed connection and check
  // if no available connection, establish a new connection, record
  int Init();
  
  const std::string scheme;
  const std::string host;
  std::string domain;
  std::string ip;

  State state = State::None;

  int fd = -1;
  SSL_CTX *p_ssl_ctx = NULL;
  SSL *p_ssl = NULL;
};

#endif  // CCSMEASURE_HTTP_CONNECTION_H_
