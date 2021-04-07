#pragma once

#include <string>
#include <netinet/in.h>
#include <vector>
#include <functional>
#include <openssl/ssl.h>
#include <regex>

static const float DEFAULT_HTTPVER { 2.0 };
static const std::string CERT { "/tmp/cert.pem" };
static const std::string KEY { "/tmp/key.pem" };

enum REQUEST { GET, POST, PUT, DELETE };

class Http
{
protected:
  int sd;
  struct sockaddr_in sa;
  char httpver[4];
  std::string hostname, report;
  std::function<bool(void)> connector;
  std::function<bool(char *)> reader;
  std::function<bool(const std::string &)> writer;
public:
  Http(const float);
  ~Http(void);
  std::string &get_report(void) { return report; };
  bool init_connect(const std::string &, const unsigned);
};

struct Secure
{
//protected:
  SSL_CTX *ctx { nullptr };
  SSL *ssl { nullptr };
  std::string cipherinfo, certificate, issuer;
//public:
  Secure(void);
  ~Secure(void);
  void gather_certificate(void);
  bool configure_context(std::string &);
  std::string &get_cipherinfo(void) { return cipherinfo; };
  std::string &get_certificate(void) { return certificate; };
  std::string &get_issuer(void) { return issuer; };
};

class SecureClientPair : public Secure
{
  public:
  SecureClientPair(void);
  ~SecureClientPair(void);
};

class SecureServerPair : public Secure
{
  public:
  SecureServerPair(void);
  ~SecureServerPair(void);
};

class Client : public Http
{
  std::string agent { "HttpRequest" }, response_header, response_body;
  std::smatch match;
  const std::regex content_length_regex { std::regex("Content-Length: ", std::regex_constants::icase) };
  std::function<void(std::string &)> response_cb { [](std::string &) { } };
public:
  Client(const float);
  ~Client(void);
  bool connect(const std::string &, const unsigned);
  void recvreq(void);
  bool sendreq(REQUEST, const std::string &, const std::vector<std::string> &, const std::string &);
  std::string &get_response(void) { return response_body; };
  std::string &get_header(void) { return response_header; };
  void set_cb(decltype(response_cb) &callback) { response_cb = callback; };
};

class HttpClient : public Client
{
public:
  HttpClient(const float);
  ~HttpClient(void);
};

class HttpsClient : public Client, public SecureClientPair
{
public:
  HttpsClient(const float);
  ~HttpsClient(void);
};

class Server : public Http
{
protected:
  bool is_running;
public:
  Server(const float);
  bool connect(const std::string &, const unsigned);
  virtual bool run(const std::string &) = 0;
  void stop(void) { is_running = false; };
};

class HttpServer : public Server
{
public:
  HttpServer(void);
  ~HttpServer(void);
  bool run(const std::string &);
};

class HttpsServer : public Server, public SecureServerPair
{
public:
  SecureClientPair client;
  HttpsServer(void);
  ~HttpsServer(void);
	static int sni_cb(SSL *, int *, void *);
  bool run(const std::string &);
};
