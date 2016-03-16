#include <stdio.h>
#include <iostream>
#include <string>
#include "tools.h"
#include "http/uri/uri.h"

using std::string;
using std::cout;
using std::endl;

void uri_test(void)
{
  Uri test[] = {
    "asdf",
    "HTTP:asdf",
    "HTTP://www.baidu.com",
    "HTTP://www.baidu.com/index.html",
    "HTTP://gray@www.baidu.com:123/index.html"};
  for (int i = 0; i < sizeof(test)/sizeof(test[0]); ++i) {
    cout << test[i].GetUri() << endl;
    cout << test[i].GetScheme() << endl;
    cout << test[i].GetHost() << endl;
    cout << test[i].GetFromPath() << endl;
    cout << endl;
  }
}

int main(int argc, char *argv[])
{
  //printf("%s\n", (IsIp(argv[1]) ? "true" : "false"));
  //printf("%s\n", NumToString(123).c_str());
  //string raw = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~:/?#[]@";
  //string encoded = UriEncoding(raw);
  //printf("%s\n", raw.c_str());
  //printf("%s\n", encoded.c_str());
  //printf("%s\n", UriDecoding(encoded).c_str());
  uri_test();
}
