#include <iostream>
#include "sock.h"

static const std::string host0 { "webscantest.com" };
static const unsigned port0 { 80 };
static const std::string host1 { "localhost" };
static const unsigned port1 { 80 };

int main(int argc, char *argv[])
{
  try {
    Client<Sock> client0(1.1, host0, port0, 2500);
    ClientHandle<Sock> handle0 { 
      client0, dummy_cb, GET, "/", { }, { }
    };

    Client<Sock> client1(1.1, host1, port1, 2500);
    ClientHandle<Sock> handle1 {
      client1, dummy_cb, GET, "/", { }, { }
    };

    MultiAsync<Sock> M({ handle0, handle1 });
    auto conn { M.connect() };
    std::cout << std::to_string(conn) << " connections established\n";
    M.performreq(2);
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

  catch (const char e[]) {
    std::cout << std::string(e) << std::endl;
  }

  return 0;
}
