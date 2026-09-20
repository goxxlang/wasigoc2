// In-guest TLS: load OpenSSL the same way ~/WASMLime's libdatachannel
// TlsTransport does (memory BIOs + SSL_do_handshake + SNI). Do not
// rewrite the handshake and do not hop to Schannel / gocvm_host.
#pragma once

#include <cerrno>
#include <cstdlib>
#include <string>

// Driver sets WASIGO_HAS_OPENSSL=1 when it links toolchain/openssl-wasm.
#if !defined(WASIGO_HAS_OPENSSL)
#  if defined(__has_include)
#    if __has_include(<openssl/ssl.h>)
#      define WASIGO_HAS_OPENSSL 1
#    endif
#  endif
#endif
#ifndef WASIGO_HAS_OPENSSL
#define WASIGO_HAS_OPENSSL 0
#endif

#if WASIGO_HAS_OPENSSL
#  include <openssl/ssl.h>
#  include <openssl/err.h>
#endif

namespace gocvm {
namespace tls_stack {

#if WASIGO_HAS_OPENSSL

inline SSL_CTX* ctx() {
  static SSL_CTX* c = [] {
    OPENSSL_init_ssl(0, nullptr);
    SSL_CTX* x = SSL_CTX_new(TLS_method());
    if (!x) return static_cast<SSL_CTX*>(nullptr);
    SSL_CTX_set_options(x, SSL_OP_NO_SSLv3 | SSL_OP_NO_RENEGOTIATION);
    SSL_CTX_set_min_proto_version(x, TLS1_2_VERSION);
    SSL_CTX_set_cipher_list(x, "ALL:!LOW:!EXP:!RC4:!MD5:@STRENGTH");
#if OPENSSL_VERSION_NUMBER >= 0x30000000
    SSL_CTX_set1_groups_list(x, "P-256");
#endif
    SSL_CTX_set_read_ahead(x, 1);
    SSL_CTX_set_verify(x, SSL_VERIFY_PEER, nullptr);
    SSL_CTX_set_default_verify_paths(x);
    if (const char* f = std::getenv("SSL_CERT_FILE")) {
      if (f[0]) SSL_CTX_load_verify_locations(x, f, nullptr);
    }
    if (const char* d = std::getenv("SSL_CERT_DIR")) {
      if (d[0]) SSL_CTX_load_verify_locations(x, nullptr, d);
    }
    return x;
  }();
  return c;
}

inline std::string sslerr(const char* what, SSL* ssl, int ret) {
  unsigned long e = ERR_get_error();
  char buf[256] = {};
  if (e) ERR_error_string_n(e, buf, sizeof(buf));
  int se = ssl ? SSL_get_error(ssl, ret) : 0;
  std::string s = "error: tls: ";
  s += what;
  if (buf[0]) {
    s += ": ";
    s += buf;
  } else if (se) {
    s += ": ssl_error ";
    s += std::to_string(se);
  }
  return s;
}

struct Sess {
  SSL* ssl = nullptr;
  std::string pending_out;
  int unflushed_write = 0;
};

inline int send_all(int fd, const char* p, int n, std::string* leftover, std::string* err) {
  int off = 0;
  while (off < n) {
    ssize_t w = send(fd, p + off, static_cast<size_t>(n - off), 0);
    if (w > 0) {
      off += static_cast<int>(w);
      continue;
    }
    if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      leftover->assign(p + off, static_cast<size_t>(n - off));
      return 0;
    }
    *err = std::string("error: tls: send: errno ") + std::to_string(errno);
    return -1;
  }
  leftover->clear();
  return 1;
}

inline int flush_out(Sess* s, int fd, std::string* err) {
  if (!s->pending_out.empty()) {
    int r = send_all(fd, s->pending_out.data(), static_cast<int>(s->pending_out.size()),
                     &s->pending_out, err);
    if (r <= 0) return r;
  }
  char buf[4096];
  for (;;) {
    int n = BIO_read(SSL_get_wbio(s->ssl), buf, sizeof(buf));
    if (n <= 0) break;
    int r = send_all(fd, buf, n, &s->pending_out, err);
    if (r <= 0) return r;
  }
  return 1;
}

inline bool open_client(Sess* s, const std::string& host, std::string* err) {
  SSL_CTX* x = ctx();
  if (!x) {
    *err = "error: tls: SSL_CTX_new failed";
    return false;
  }
  SSL* ssl = SSL_new(x);
  if (!ssl) {
    *err = "error: tls: SSL_new failed";
    return false;
  }
  BIO* in = BIO_new(BIO_s_mem());
  BIO* out = BIO_new(BIO_s_mem());
  if (!in || !out) {
    BIO_free(in);
    BIO_free(out);
    SSL_free(ssl);
    *err = "error: tls: BIO_new failed";
    return false;
  }
  BIO_set_mem_eof_return(in, -1);
  BIO_set_mem_eof_return(out, -1);
  SSL_set_bio(ssl, in, out);
  SSL_set_connect_state(ssl);
  SSL_set_verify(ssl, SSL_VERIFY_PEER, nullptr);
  SSL_set_verify_depth(ssl, 4);
  if (!host.empty()) {
    SSL_set1_host(ssl, host.c_str());
    SSL_set_tlsext_host_name(ssl, host.c_str());
  }
  s->ssl = ssl;
  s->pending_out.clear();
  s->unflushed_write = 0;
  return true;
}

inline void close_sess(Sess* s) {
  if (s && s->ssl) {
    SSL_free(s->ssl);
    s->ssl = nullptr;
  }
  if (s) {
    s->pending_out.clear();
    s->unflushed_write = 0;
  }
}

inline int feed_in(Sess* s, int fd, std::string* err) {
  char buf[4096];
  ssize_t n = recv(fd, buf, sizeof(buf), 0);
  if (n > 0) {
    BIO_write(SSL_get_rbio(s->ssl), buf, static_cast<int>(n));
    return 1;
  }
  if (n == 0) {
    *err = "error: tls: peer closed";
    return -1;
  }
  if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
  *err = std::string("error: tls: recv: errno ") + std::to_string(errno);
  return -1;
}

// 1 = done, 0 = park (events: POLLIN and/or POLLOUT in *want), -1 = fail.
inline int handshake(Sess* s, int fd, short* want, std::string* err) {
  for (;;) {
    *want = 0;
    int ret = SSL_do_handshake(s->ssl);
    int fl = flush_out(s, fd, err);
    if (fl < 0) return -1;
    if (ret == 1) {
      if (fl == 0) {
        *want = POLLOUT;
        return 0;
      }
      return 1;
    }
    int se = SSL_get_error(s->ssl, ret);
    if (se == SSL_ERROR_WANT_READ) {
      int f = feed_in(s, fd, err);
      if (f < 0) return -1;
      if (f > 0) continue;
      *want = POLLIN;
      if (fl == 0) *want = static_cast<short>(*want | POLLOUT);
      return 0;
    }
    if (se == SSL_ERROR_WANT_WRITE || fl == 0) {
      *want = POLLOUT;
      return 0;
    }
    *err = sslerr("handshake", s->ssl, ret);
    return -1;
  }
}

inline int read_app(Sess* s, int fd, char* dst, int maxlen, short* want, std::string* err) {
  for (;;) {
    *want = 0;
    int ret = SSL_read(s->ssl, dst, maxlen);
    if (ret > 0) return ret;
    int se = SSL_get_error(s->ssl, ret);
    if (se == SSL_ERROR_WANT_READ) {
      int f = feed_in(s, fd, err);
      if (f < 0) return -1;
      if (f > 0) continue;
      *want = POLLIN;
      return 0;
    }
    if (se == SSL_ERROR_WANT_WRITE) {
      int fl = flush_out(s, fd, err);
      if (fl < 0) return -1;
      if (fl > 0) continue;
      *want = POLLOUT;
      return 0;
    }
    if (se == SSL_ERROR_ZERO_RETURN) return 0;
    *err = sslerr("read", s->ssl, ret);
    return -1;
  }
}

inline int write_app(Sess* s, int fd, const char* src, int len, short* want, std::string* err) {
  *want = 0;
  if (s->unflushed_write > 0) {
    int fl = flush_out(s, fd, err);
    if (fl < 0) return -1;
    if (fl == 0) {
      *want = POLLOUT;
      return 0;
    }
    int n = s->unflushed_write;
    s->unflushed_write = 0;
    return n;
  }
  int ret = SSL_write(s->ssl, src, len);
  int fl = flush_out(s, fd, err);
  if (fl < 0) return -1;
  if (ret > 0 && fl == 0) {
    s->unflushed_write = ret;
    *want = POLLOUT;
    return 0;
  }
  if (ret > 0) return ret;
  int se = SSL_get_error(s->ssl, ret);
  if (se == SSL_ERROR_WANT_READ) {
    *want = POLLIN;
    return 0;
  }
  if (se == SSL_ERROR_WANT_WRITE || fl == 0) {
    *want = POLLOUT;
    return 0;
  }
  *err = sslerr("write", s->ssl, ret);
  return -1;
}

#else  // !WASIGO_HAS_OPENSSL

struct Sess {};
inline bool open_client(Sess*, const std::string&, std::string* err) {
  *err = "error: tls: OpenSSL not linked (same stack as WASMLime libdatachannel)";
  return false;
}
inline void close_sess(Sess*) {}
inline int handshake(Sess*, int, short*, std::string* err) {
  *err = "error: tls: OpenSSL not linked";
  return -1;
}
inline int feed_in(Sess*, int, std::string*) { return -1; }
inline int read_app(Sess*, int, char*, int, short*, std::string* err) {
  *err = "error: tls: OpenSSL not linked";
  return -1;
}
inline int write_app(Sess*, int, const char*, int, short*, std::string* err) {
  *err = "error: tls: OpenSSL not linked";
  return -1;
}

#endif

}  // namespace tls_stack
}  // namespace gocvm
