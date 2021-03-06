/**********************************************************************************
MIT License

Copyright (c) 2021-22 doa379

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

#include <netdb.h>
#include <cmath>
#include <libsockpp/sock.h>
#include <libsockpp/time.h>

static const std::array<std::string, 4> REQ { "GET", "POST", "PUT", "DELETE" };
static const unsigned char LISTEN_QLEN { 16 };

bool sockpp::Http::init_client(const char HOST[], const char PORT[]) {
  struct ::addrinfo hints { };
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = { };
  hints.ai_protocol = { };
  struct ::addrinfo *result;
  if (::getaddrinfo(HOST, PORT, &hints, &result)) return false;
  for (struct ::addrinfo *rp { result }; rp; rp = rp->ai_next) {
    if ((sockfd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) > -1 &&
          ::connect(sockfd, rp->ai_addr, rp->ai_addrlen) > -1) {
      ::freeaddrinfo(result);
      return true;
    }
    
    deinit();
  }

  ::freeaddrinfo(result);
  return false;
}

bool sockpp::Http::init_server(const char PORT[]) {
  struct ::addrinfo hints { };
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = { };
  hints.ai_canonname = { };
  hints.ai_addr = { };
  hints.ai_next = { };
  struct ::addrinfo *result;
  if (::getaddrinfo(nullptr, PORT, &hints, &result)) return false;
  for (struct ::addrinfo *rp { result }; rp; rp = rp->ai_next) {
    if ((sockfd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) > -1 &&
          ::bind(sockfd, rp->ai_addr, rp->ai_addrlen) > -1 &&
            ::listen(sockfd, LISTEN_QLEN) > -1) {
      ::freeaddrinfo(result);
      return true;
    }

    deinit();
  }

  ::freeaddrinfo(result);
  return false;
}

void sockpp::Http::deinit(void)  {
  if (sockfd > -1 && ::close(sockfd) > -1)
    sockfd = -1;
}

void sockpp::Http::init_poll(void) {
  pollfd.fd = sockfd;
}

bool sockpp::Http::pollin(const int timeout_ms) {
  pollfd.events = POLLIN;
  pollfd.revents = 0;
  return ::poll(&pollfd, 1, timeout_ms) > 0 && (pollfd.revents & POLLIN);
  // Poll retval > 0 success, < 0 fail, == 0 timeout
}

bool sockpp::Http::pollout(const int timeout_ms) {
  pollfd.events = POLLOUT;
  pollfd.revents = 0;
  return ::poll(&pollfd, 1, timeout_ms) > 0 && (pollfd.revents & POLLOUT);
}

bool sockpp::Http::pollerr(const int timeout_ms) {
  auto event { POLLERR | POLLHUP | POLLNVAL };
  pollfd.events = event;
  pollfd.revents = 0;
  return ::poll(&pollfd, 1, timeout_ms) > 0 && (pollfd.revents & event);
}

bool sockpp::Http::read(char &p) const {
  return ::read(sockfd, &p, sizeof p) > 0;
}

bool sockpp::Http::write(const std::string &req) const {
  return ::write(sockfd, req.c_str(), req.size()) > 0;
}

void sockpp::Https::deinit(void) const {
  if (ssl) {
    ::SSL_shutdown(ssl);
    ::SSL_free(ssl);
  }

  if (ctx)
    ::SSL_CTX_free(ctx);
}

bool sockpp::Https::configure_ctx(const char CERT[], const char KEY[]) const {
  SSL_CTX_set_ecdh_auto(ctx, 1);
  return SSL_CTX_use_certificate_file(ctx, CERT, SSL_FILETYPE_PEM) > 0 &&
      SSL_CTX_use_PrivateKey_file(ctx, KEY, SSL_FILETYPE_PEM) > 0 &&
        SSL_CTX_check_private_key(ctx) > 0;
}

bool sockpp::Https::connect(const char HOST[]) {
  if (Https::init_client() && init() && set_hostname(HOST)) {
      set_connect_state();
      if (set_fd(Http::sockfd) && do_handshake() && init_rbio() && init_wbio()) {
        set_rwbio();
        return true;
      }
  }

  return false;
}

void sockpp::Https::readfilter(char p) {
  ::BIO_write(r, &p, sizeof p);
}

bool sockpp::Https::postread(char &p) {
  return ::SSL_read(ssl, &p, sizeof p) > 0;
}

bool sockpp::Https::write(const std::string &req) const {
  char buffer[16384] { };
  ssize_t Nenc { };
  return ::SSL_write(ssl, req.c_str(), req.size()) > 0 &&
    (Nenc = ::BIO_read(w, buffer, sizeof buffer)) > 0 &&
      ::write(sockfd, buffer, Nenc) > 0;
}

void sockpp::Https::certinfo(std::string &cipherinfo, std::string &cert, std::string &iss) const {
  cipherinfo = std::string { ::SSL_get_cipher(ssl) };
  ::X509 *server_cert { ::SSL_get_peer_certificate(ssl) };
  if (!server_cert) return;

  auto CERT { ::X509_NAME_oneline(X509_get_subject_name(server_cert), 0, 0) };
  if (CERT) {
    cert = std::string { CERT };
    ::OPENSSL_free(CERT);
  }

  auto ISS { ::X509_NAME_oneline(X509_get_issuer_name(server_cert), 0, 0) };
  if (ISS) {
    iss = std::string { ISS };
    ::OPENSSL_free(ISS);
  }

  ::X509_free(server_cert);
}

template<typename S>
sockpp::Send<S>::Send(const float ver) { 
  char httpver[8] { };
  ::snprintf(httpver, sizeof httpver - 1, "%.1f", ver);
  this->httpver = std::string { httpver };
}

template<typename S>
bool sockpp::Send<S>::req(S &s, const std::string &host, const Req req, const std::vector<std::string> &HEAD, const std::string &data, const std::string &endp) const {
  if (&REQ[static_cast<int>(req)] > &REQ[REQ.size() - 1] || (req == Req::GET && data.size()))
    return false;
  
  std::string request { 
    REQ[static_cast<int>(req)] + " " + endp + " " + "HTTP/" + httpver + "\r\n" +
      "Host: " + host + "\r\n" +
        "User-Agent: " + agent + "\r\n" +
          "Accept: */*" + "\r\n" 
    };

  for (auto &h : HEAD)
    request += h + "\r\n";

  if (data.size())
    request += "Content-Length: " + std::to_string(data.size()) + "\r\n\r\n" + data;

  request += "\r\n";
  return s.write(request);
}

template class sockpp::Send<sockpp::Http>;
template class sockpp::Send<sockpp::Https>;

template<typename S>
bool sockpp::Recv<S>::is_chunked(const std::string &header) const {
  std::smatch match { };
  return std::regex_search(header, match, transfer_encoding_regex) &&
    std::regex_match(header.substr(match.prefix().length() + 19, 7), chunked_regex);
}

template<typename S>
bool sockpp::Recv<S>::req_header(S &s, std::string &header) const {
  char p { };
  while (s.pollin(timeout_ms) && s.read(p)) {
    s.readfilter(p);
    while (s.postread(p)) {
      header += p;
      if ((ssize_t) header.rfind("\r\n\r\n") > -1) return true;
    }
  }

  return false;
}

template<typename S>
std::size_t sockpp::Recv<S>::parse_cl(const std::string &header) const {
  std::size_t cl { };
  std::smatch match { };
  if (std::regex_search(header, match, content_length_regex) &&
      (cl = std::stoull(header.substr(match.prefix().length() + 16,
        header.substr(match.prefix().length() + 16).find("\r\n")))));
  return cl;
}

template<typename S>
bool sockpp::Recv<S>::req_body(S &s, std::string &body, const std::size_t cl) const {
  char p { };
  while (body.size() < cl && s.postread(p))
    body += p;
  if (body.size() == cl)
    return true;

  while (s.pollin(timeout_ms) && s.read(p)) {
    s.readfilter(p);
    while (s.postread(p)) {
      body += p;
      if (body.size() == cl)
        return true;
    }
  }

  return false;
}

template<typename S>
bool sockpp::Recv<S>::req_chunked(S &s, const Client_cb &cb, std::string &body) const {
  char p { };
  std::size_t l { };
  while (s.pollin(timeout_ms) && s.read(p)) {
    s.readfilter(p);
    while (s.postread(p)) {
      body += p;
      if (body == "\r\n");
      else if (l == 0 && (ssize_t) body.rfind("\r\n") > -1) {
        body.erase(body.end() - 2, body.end());
        try {
          l = std::stoull(body, nullptr, 16);
        } catch (...) { return false; }

        if (l == 0) return true;
      } else if (body.size() == l) {
        cb(body);
        l = 0;
      } else continue;
      body.clear();
    }
  }

  return true;
}

template<typename S>
bool sockpp::Recv<S>::req_chunked_raw(S &s, const Client_cb &cb, std::string &body) const {
  char p { };
  while (s.pollin(timeout_ms) && s.read(p)) {
    s.readfilter(p);
    while (s.postread(p)) {
      body += p;
      cb(body);
      body.clear();
    }
  }

  return true;
}

template class sockpp::Recv<sockpp::Http>;
template class sockpp::Recv<sockpp::Https>;

template<typename S>
sockpp::Client<S>::Client(const float ver, const char HOST[], const char PORT[]) : 
  ver { ver }, host { std::string { HOST } } {
  if (sock.Http::init_client(HOST, PORT) && sock.connect(HOST)) sock.init_poll();
  else throw std::runtime_error("Unable to connect");
}

template<typename S>
bool sockpp::Client<S>::performreq(XHandle &h, const unsigned timeout_ms) {
  Send<S> send { ver };
  Recv<S> recv { timeout_ms };
  if (send.req(sock, host, h.req, h.HEAD, h.data, h.endp) && recv.req_header(sock, h.header)) {
    if (recv.is_chunked(h.header)) return recv.req_body(sock, h.cb, h.body);
    else {
      auto cl { recv.parse_cl(h.header) };
      return cl && recv.req_body(sock, h.body, cl);
    }
  }

  return false;
}

template class sockpp::Client<sockpp::Http>;
template class sockpp::Client<sockpp::Https>;

template<typename S>
sockpp::MultiClient<S>::MultiClient(const float ver, const char HOST[], const char PORT[], const unsigned N) : 
  ver { ver }, host { std::string { HOST } } {
  if (N > MAX_N) throw std::runtime_error("# of requested connexions exceeds supremum");
  for (auto i { 0U }; i < N; i++) {
    S &sock { SOCK[i] };
    if (sock.Http::init_client(HOST, PORT) && sock.connect(HOST)) {
      sock.init_poll();
      CONN[i] = 1;
    }
  }
  
  if (!CONN.any()) throw std::runtime_error("Unable to connect");
}

template<typename S>
bool sockpp::MultiClient<S>::performreq(const std::vector<std::reference_wrapper<XHandle>> &H, const unsigned timeout_ms) {
  Send<S> send { ver };
  std::bitset<MAX_N> SENT;
  for (auto i { 0U }; i < H.size(); i++)
    if (CONN[i] && send.req(SOCK[i], host, H[i].get().req, H[i].get().HEAD, H[i].get().data, H[i].get().endp))
      SENT[i] = 1;
  
  if (!SENT.any()) return false;
  std::bitset<MAX_N> HDR, ISCHK;
  std::array<std::size_t, MAX_N> CL;
  Recv<S> recv { sockpp::MULTI_TIMEOUTMS };
  sockpp::Time time;
  auto init_time { time.now() };
  while (SENT.any() && time.diffpt<std::chrono::milliseconds>(time.now(), init_time) < timeout_ms)
    for (auto i { 0U }; i < H.size(); i++)
      if (SENT[i] && !HDR[i] && recv.req_header(SOCK[i], H[i].get().header)) {
        HDR[i] = 1;
        SENT[i] = 0;
        if (recv.is_chunked(H[i].get().header)) ISCHK[i] = 1;
        else CL[i] = recv.parse_cl(H[i].get().header);
      }

  while (HDR.any() && time.diffpt<std::chrono::milliseconds>(time.now(), init_time) < timeout_ms)
    for (auto i { 0U }; i < H.size(); i++)
      if (HDR[i] &&
        ((ISCHK[i] && recv.req_body(SOCK[i], H[i].get().cb, H[i].get().body)) ||
            (CL[i] && recv.req_body(SOCK[i], H[i].get().body, CL[i]))))
              HDR[i] = 0;

  return true;
}

template class sockpp::MultiClient<sockpp::Http>;
template class sockpp::MultiClient<sockpp::Https>;

template<typename S>
sockpp::Server<S>::Server(const char PORT[]) {
  if (sock.Http::init_server(PORT)) sock.init_poll();
  else throw std::runtime_error("Unable to init server");
}
// Construct a server for each client
template<>
void sockpp::Server<sockpp::Http>::recv_client(const char [], const char []) {
  auto server { std::make_unique<Http>(this->sock.accept()) };
  server->init_poll();
  SOCK.emplace_back(std::move(server));
}

template<>
void sockpp::Server<sockpp::Https>::recv_client(const char CERT[], const char KEY[]) {
  Https client;
  if (!client.init_client() || !client.configure_ctx(CERT, KEY)) return;
  auto sockfd { sock.Http::accept() };
  auto server { std::make_unique<Https>(sockfd) };
  if (server->init_server() && server->init()) {
    server->set_ctx(client.get_ctx());
    server->set_accept_state();
    if (server->set_fd(sockfd) && server->do_handshake() &&
      server->init_rbio() && server->init_wbio()) {
        server->set_rwbio();
        server->init_poll();
        SOCK.emplace_back(std::move(server));
    }
  }
}

template<typename S>
void sockpp::Server<S>::run(const Server_cb<S> &cb, const char CERT[], const char KEY[]) {
  while (!quit) {
    if (poll_listen(10)) recv_client(CERT, KEY);
    for (auto &sock : SOCK) {
      if (sock->pollin(10) && !cb(*sock)) {
        auto i { &sock - &SOCK[0] };
        SOCK.erase(SOCK.begin() + i);
        break;
      }
    }
  }
}

template class sockpp::Server<sockpp::Http>;
template class sockpp::Server<sockpp::Https>;
