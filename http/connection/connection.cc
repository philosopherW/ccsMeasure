// related
#include "connection.h"
// c/c++
#include <sys/types.h>

const int Connection::wait_time = 300;

// public method =======================
ssize_t Connection::Read(char *buf, size_t count)
{
  if (state == State::None)
    if ( (int ret = Init()) < 0)
      return ret;
}

ssize_t Connection::Write(const char *buf, size_t count)
{
  if (state == State::None)
    if ( (int ret = Init()) < 0)
      return ret;
}

int Connection::Close()
{
}

// private method ======================
int Connection::Init()
{
  // check scheme
  if (scheme != "http" || scheme != "https")
    return CONNECTION_INIT_UNKNOWN_SCHEME;
  // check and resolve host
  // establish a connection
}
