#include <iostream>
#include "socket.h"

static const std::string host0 { "webscantest.com" };
static const unsigned port0 { 80 };
static const std::string host1 { "localhost" };
static const unsigned port1 { 80 };

int main(int argc, char *argv[])
{
  try {
    Client<Sock> client0(1.1, host0, port0), client1(1.1, host1, port1);
    MultiClient<Sock> mc;
    mc.set_client(client0);
    mc.set_client(client1);
    if (!mc.connect())
      std::cerr << "There was at least one failure in resolving host(s), proceeding...\n";
    client0.sendreq(GET, "/", { }, { });
    client1.sendreq(GET, "/", { }, { });
    mc.recvreq(30);
    std::cout << "All transfer(s) completed\n";
    std::cout << "(Client0):\n===================\n";
    std::cout << "The response header (client0):\n===================\n";
    std::cout << client0.header() << std::endl;
    std::cout << "The response body (client0):\n===================\n";
    std::cout << client0.body() << std::endl;
    std::cout << "(Client1):\n===================\n";
    std::cout << "The response header (client1):\n===================\n";
    std::cout << client1.header() << std::endl;
    std::cout << "The response body (client1):\n===================\n";
    std::cout << client1.body() << std::endl;
  }

  catch (const std::string &e) {
    std::cout << e << std::endl;
  }

  return 0;
}
