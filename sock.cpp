/**********************************************************************************
MIT License

Copyright (c) 2021 doa379

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
**********************************************************************************/

#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <openssl/err.h>
#include <bitset>
#include "sock.h"

Sock::Sock(const int sd)
{
  init(sd);
  memset(&sa, 0, sizeof sa);
}

Sock::~Sock(void)
{
  deinit();
}

bool Sock::init(const int sd)
{
  if (sd)
    this->sd = sd;
  else if ((this->sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return false;
  return true;
}

void Sock::deinit(void)
{
  struct linger lo { 1, 0 };
  setsockopt(sd, SOL_SOCKET, SO_LINGER, &lo, sizeof lo);
  close(sd);
}

bool Sock::init_connect(const std::string &hostname, const unsigned port)
{
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  struct hostent *host { gethostbyname(hostname.c_str()) };
  if (host)
  {
    sa.sin_addr.s_addr = *(long *) host->h_addr;
    return true;
  }
  
  return false;
}

bool Sock::connect(const std::string &)
{
  if (::connect(sd, (struct sockaddr *) &sa, sizeof sa) > -1)
    return true;
  return false;
}

bool Sock::read(char &p)
{
  if (::recv(sd, &p, sizeof p, 0) > -1)
    return true;
  return false;
}

bool Sock::write(const std::string &data)
{
  if (::write(sd, data.c_str(), data.size()) > -1)
  {
    fsync(sd);
    return true;
  }
  return false;
}

int Sock::accept(void)
{
  struct sockaddr_in addr;
  uint len { sizeof addr };
  return ::accept(sd, (struct sockaddr *) &addr, &len);
  // Return a sd
}
  
bool Sock::bind(void)
{
  if (::bind(sd, (struct sockaddr *) &sa, sizeof sa) > -1)
    return true;
  return false;
}

bool Sock::listen(void)
{
  if (::listen(sd, 1) > -1)
    return true;
  return false;
}

InitSSock::InitSSock(void)
{
  InitSSock::init();
}

void InitSSock::init(void)
{
  OpenSSL_add_ssl_algorithms();
  SSL_load_error_strings();
}

SSock::SSock(const SSL_METHOD *meth, const unsigned sd) : 
  Sock(sd),
  ctx(SSL_CTX_new(meth)), ssl(SSL_new(ctx))
{
  if (!ctx)
    throw "Unable to create context";
  else if (!ssl)
    throw "Unable to create ssl";
}

SSock::~SSock(void)
{
  SSL_shutdown(ssl);
  SSL_free(ssl);
  SSL_CTX_free(ctx);
}

bool SSock::configure_context(const std::string &certpem, const std::string &keypem)
{
  SSL_CTX_set_ecdh_auto(ctx, 1);
  if (SSL_CTX_use_certificate_file(ctx, certpem.c_str(), SSL_FILETYPE_PEM) < 1)
    return false;
  if (keypem.size() && SSL_CTX_use_PrivateKey_file(ctx, keypem.c_str(), SSL_FILETYPE_PEM) < 1)
    return false;
  else if (!keypem.size() && SSL_CTX_use_PrivateKey_file(ctx, certpem.c_str(), SSL_FILETYPE_PEM) < 1)
    return false;
  return true;
}

bool SSock::set_hostname(const std::string &hostname)
{
  if (SSL_set_tlsext_host_name(ssl, hostname.c_str()) > 0)
    return true;
  return false;
}

bool SSock::set_fd(void)
{
  if (SSL_set_fd(ssl, sd) > 0)
    return true;
  return false;
}

bool SSock::connect(const std::string &hostname)
{
  if (Sock::connect())
  {
    set_hostname(hostname);
    set_fd();
    if (SSL_connect(ssl) > 0)
      return true;
  }

  return false;
}

bool SSock::read(char &p)
{
  if (SSL_read(ssl, &p, sizeof p) > 0)
    return true;
  return false;
}

bool SSock::write(const std::string &data)
{
  if (SSL_write(ssl, data.c_str(), data.size()) > 0)
    return true;
  return false;
}

bool SSock::clear(void)
{
  if (SSL_clear(ssl) > 0)
    return true;

  return false;
}

bool SSock::accept(void)
{
  if (SSL_accept(ssl) > 0)
    return true;
  return false;
}

SSL_CTX *SSock::ssl_ctx(SSL_CTX *ctx)
{
  return SSL_set_SSL_CTX(ssl, ctx);
}

int SSock::error(int err)
{
  return SSL_get_error(ssl, err);
}

void SSock::certinfo(std::string &cipherinfo, std::string &cert, std::string &issue)
{
  // Method must be called after connect()
  cipherinfo = std::string(SSL_get_cipher(ssl));
  X509 *server_cert { SSL_get_peer_certificate(ssl) };
  if (!server_cert)
    return;

  char *certificate { X509_NAME_oneline(X509_get_subject_name(server_cert), 0, 0) };
  if (certificate)
  {
    cert = std::string(certificate);
    OPENSSL_free(certificate);
  }

  char *issuer { X509_NAME_oneline(X509_get_issuer_name(server_cert), 0, 0) };
  if (issuer)
  {
    issue = std::string(issuer);
    OPENSSL_free(issuer);
  }

  X509_free(server_cert);
}

template<typename T>
bool Recv::req(T &sock, const Cb &cb)
{
  clear_header();
  clear_body();
  // Header
  char p;
  bool res;
  do
  {
    res = sock.read(p);
    _header += p;
  }
  while (res && !(_header.rfind("\r\n\r\n") < std::string::npos));
  
  if (!std::regex_search(_header, match, ok_regex))
    return false;
  // Body
  std::size_t l { };
  if (std::regex_search(_header, match, content_length_regex) &&
      (l = std::stoull(_header.substr(match.prefix().length() + 16,
        _header.substr(match.prefix().length() + 16).find("\r\n")))))
    do
    {
      res = sock.read(p);
      _body += p;
    }
    while (res && _body.size() < l);

  else if (std::regex_search(_header, match, transfer_encoding_regex) &&
      std::regex_match(_header.substr(match.prefix().length() + 19, 7), chunked_regex))
  {
    auto now { time.now() };
    while (sock.read(p) && time.diffpt<std::chrono::milliseconds>(time.now(), now) < timeout_ms)
    {
      _body += p;
      now = time.now();
      if (_body == "\r\n");
      else if (!l && _body.rfind("\r\n") < std::string::npos)
      {
        _body.erase(_body.end() - 2, _body.end());
        if (!(l = std::stoull(_body, nullptr, 16)))
          break;
      }
      else if (_body.size() == l)
      {
        cb(_body);
        l = 0;
      }
      else
        continue;

      _body.clear();
    }
  }

  return true;
}

template<typename T>
void Recv::req_raw(T &sock, const Cb &cb)
{
  char p;
  auto now { time.now() };
  while (sock.read(p) && time.diffpt<std::chrono::milliseconds>(time.now(), now) < timeout_ms)
  {
    _body += p;
    now = time.now();
    cb(_body);
    clear_body();
  }
}

template<typename T>
Client<T>::Client(const float httpver, const std::string &hostname, const unsigned port) : 
  httpver(httpver), hostname(hostname), port(port)
{
  init_sock();
}

template<>
void Client<Sock>::init_sock(void)
{
  sock = std::make_unique<Sock>();
}

template<>
void Client<SSock>::init_sock(void)
{
  sock = std::make_unique<SSock>(TLS_client_method());
}

template<typename T>
bool Client<T>::connect(void)
{
  if (sock->init_connect(hostname, port))
    return sock->connect(hostname);
  return false;
}

template<typename T>
bool Client<T>::sendreq(const std::vector<std::string> &H, const std::string &data)
{
  std::string request;
  for (auto &h : H)
    request += h + "\r\n";

  if (data.size())
    request += "Content-Length: " + std::to_string(data.size()) + "\r\n\r\n" + data;

  request += "\r\n";
  return sock->write(request);
}

template<typename T>
bool Client<T>::sendreq(const Req req, const std::string &endp, const std::vector<std::string> &H, const std::string &data)
{
  if (&REQ[req] > &REQ[REQ.size() - 1]
    || (req == GET && data.size()))
    return false;

  char httpver[8];
  {
    snprintf(httpver, sizeof httpver - 1, "%.1f", this->httpver);
  }
  std::string request { 
    REQ[req] + " " + endp + " " + "HTTP/" + std::string(httpver) + "\r\n" +
    "Host: " + hostname + "\r\n" +
    "User-Agent: " + std::string(agent) + "\r\n" +
    "Accept: */*" + "\r\n" };

  for (auto &h : H)
    request += h + "\r\n";

  if (data.size())
    request += "Content-Length: " + std::to_string(data.size()) + "\r\n\r\n" + data;

  request += "\r\n";
  return sock->write(request);
}

template<typename T>
bool Client<T>::performreq(const Cb &cb, const std::vector<std::string> &H, const std::string &data)
{
  if (connect() && sendreq(H, data))
    return recvreq(cb);
  return false;
}

template<typename T>
bool Client<T>::performreq(const Req req, const Cb &cb, const std::string &endp, const std::vector<std::string> &H, const std::string &data)
{
  if (connect() && sendreq(req, endp, H, data))
    return recvreq(cb);
  return false;
}

template class Client<Sock>;
template class Client<SSock>;

template<typename T>
bool MultiClient<T>::reg_client(Client<T> &c)
{
  if (C.size() < MAX_CLIENTS)
  {
    C.emplace_back(c);
    return true;
  }

  return false;
}

template<typename T>
bool MultiClient<T>::connect(void)
{
  bool retval { true };
  for (auto &c : C)
    if (!c.get().connect())
      retval = false;

  return retval;
}

template<typename T>
void MultiClient<T>::recvreq(unsigned timeout, const std::vector<Cb> &CB)
{
  struct pollfd PFD[MAX_CLIENTS] { };
  std::bitset<MAX_CLIENTS> M;
  for (auto i { 0U }; i < C.size(); i++)
  {
    PFD[i].fd = C[i].get().sock->get();
    PFD[i].events = POLLIN;
  }

  const auto init { time.now() };
  auto now { init };
  while (M.count() < C.size() && time.diffpt<std::chrono::seconds>(now, init) < timeout)
  {
    poll(PFD, C.size(), 100);
    for (auto i { 0U }; i < C.size(); i++)
      if (PFD[i].revents & POLLIN && !M[i])
      {
        if (CB.size())
          C[i].get().recvreq(CB[i]);
        else
          C[i].get().recvreq();
        M |= 1 << i;
      }

    now = time.now();
  }
}

template class MultiClient<Sock>;
template class MultiClient<SSock>;

template<typename T>
Server<T>::Server(const std::string &hostname, const unsigned port) :
  hostname(hostname), port(port)
{
  init_sock();
}

template<>
void Server<Sock>::init_sock(void)
{
  sock = std::make_unique<Sock>();
}

template<>
void Server<SSock>::init_sock(void)
{
  sock = std::make_unique<SSock>(TLS_server_method());
}

template<typename T>
bool Server<T>::connect(void)
{
  if (sock->init_connect(hostname, port) &&
    sock->bind() &&
      sock->listen())
  {
    listensd.fd = sock->get();
    listensd.events = POLLIN;
    return true;
  }

  return false;
}

template<typename T>
bool Server<T>::poll_listen(unsigned timeout_ms)
{
  poll(&listensd, 1, timeout_ms);
  if (listensd.revents & POLLIN)
    return true;
  return false;
}

template<>
std::shared_ptr<Sock> Server<Sock>::recv_client(const std::string &, const std::string &)
{
  return std::make_shared<Sock>(sock->accept());
}

template<>
std::shared_ptr<SSock> Server<SSock>::recv_client(const std::string &certpem, const std::string &keypem)
{
  auto client { std::make_shared<SSock>(TLS_client_method()) };
  if (!client->configure_context(certpem, keypem) ||
    !client->set_hostname(hostname))
    return nullptr;
  auto sd { sock->Sock::accept() };
  auto socks { std::make_shared<SSock>(TLS_server_method(), sd) };
  socks->set_fd();
  socks->ssl_ctx(client->ssl_ctx());
  if (socks->accept())
    return socks;
  return nullptr;
}

template<typename T>
void Server<T>::new_client(std::shared_ptr<T> t, const std::function<void(T &)> &cb)
{
  auto c { std::async(std::launch::async, [=](void) { cb(*t); }) };
  C.emplace_back(std::move(c));
}

template<typename T>
void Server<T>::refresh_clients(void)
{
  C.remove_if([](auto &c) { 
    return c.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready; });
}

template class Server<Sock>;
template class Server<SSock>;