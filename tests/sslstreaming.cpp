#include <iostream>
#include <libsockpp/sock.h>

static const char HOST[] { "localhost" };
static const char PORT[] { "4433" };

// Remember to generate a set of pems
// $ openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout /tmp/key.pem -out /tmp/cert.pem

int main(int ARGC, char *ARGV[]) {
  sockpp::Client_cb writer_cb {
    [](const char p) { std::cout << p; } };
  // Data sent as POST request
  // Header validates request is OK
  // Chunked transfer calls cb()
  sockpp::Handle::Xfr h { 
    { sockpp::Meth::POST, { "Some Header", "Some Header" }, "Some Data", "/" },
    writer_cb
  };

  try {
    sockpp::Client<sockpp::Https> client { HOST, PORT };
    if (!client.performreq(h))
      throw "Unable to sendreq()";

    std::cout << "Stream disconnected\n";
    std::cout << "The response header:\n===================\n";
    std::cout << h.header() << std::endl;
  } catch (const std::exception &e) { std::cerr << e.what() << std::endl; }
  return 0;
}
