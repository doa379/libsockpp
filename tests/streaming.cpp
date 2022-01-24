#include <iostream>
#include <libsockpp/sock.h>

static const char HOST[] { "127.0.0.1" };
static const char PORT[] { "8080" };

int main(int ARGC, char *ARGV[]) {
  sockpp::Cb cb { [](const std::string &buffer) { std::cout << "Received " << buffer; } };
  sockpp::XHandle h { cb, POST, { "Some Header", "Some Header" }, "Some Data", "/" };
  try {
    sockpp::Client<sockpp::Http> client { 1.1, HOST, PORT };
    while (1) {
      if (!client.performreq(h))
        throw "Unable to sendreq()";
      std::cout << "Stream disconnected\n";
      std::cout << "The response header:\n===================\n";
      std::cout << h.header << std::endl;
      std::cout << "The response body:\n===================\n";
      std::cout << h.body << std::endl;
    }
  } catch (const char E[]) { std::cout << E << std::endl; }
  return 0;
}
