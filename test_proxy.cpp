#include <iostream>
#include <sstream>
#include <string>
using namespace std;

string extract_host(const string &http_request) {
  istringstream stream(http_request);
  string line;
  while (getline(stream, line)) {
    if (line.find("Host: ") == 0) {
      string host = line.substr(6);
      if (!host.empty() && host.back() == '\r') {
        host.pop_back();
      }
      return host;
    }
  }
  return "";
}

int main() {
  string req = "GET http://neverssl.com/ HTTP/1.1\r\nHost: neverssl.com\r\nUser-Agent: curl/8.7.1\r\nAccept: */*\r\nProxy-Connection: Keep-Alive\r\n\r\n";
  string host = extract_host(req);
  cout << "Extracted: [" << host << "]" << endl;
  cout << "Successfully Connected to " << host << ". Forwarding Traffic . . . \n";
  return 0;
}
