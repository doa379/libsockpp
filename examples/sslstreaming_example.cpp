#include <iostream>
#include <libsockpp/sock.h>

static const std::string host0 { "localhost" };
static const unsigned port0 { 4433 };

// Remember to generate a set of pems
// $ openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout /tmp/key.pem -out /tmp/cert.pem

int main(int argc, char *argv[])
{
  std::string hostname;
  unsigned port_no;
  if (argc != 3)
  {
    std::cerr << "Usage: ./sslstreaming_example <hostname> <port>\n";
    hostname = host0;
    port_no = port0;
  }

  else
  {
    hostname = std::string(argv[1]);
    port_no = std::atoi(argv[2]);
  }
  
  sockpp::Cb cb { [](const std::string &buffer) { std::cout << buffer; } };
  // Data sent as POST request
  // Header validates request is OK
  // Chunked transfer calls cb()
  sockpp::XHandle h { cb, POST, { "OK" }, "Some Data", "/" };

  try {
    sockpp::Client<sockpp::Https> client(1.1, hostname, port_no);
    if (!client.performreq(h))
      throw "Unable to sendreq()";

    std::cout << "Stream disconnected\n";
    std::cout << "The response header:\n===================\n";
    std::cout << h.header << std::endl;
  }

  catch (const char e[]) {
    std::cout << std::string(e) << std::endl;
  }

  return 0;
}