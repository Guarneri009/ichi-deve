#include <cstddef>
#include <iostream>
#include <string>
#include <thread>
#include <iomanip>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

template <typename T>
constexpr std::string_view type_name()
{
   std::string_view name = __PRETTY_FUNCTION__;
   auto start = name.find('=') + 2;
   auto end = name.find(';', start);
   return name.substr(start, end - start);
}

template <typename T>
constexpr void print(T value)
{
   std::cout << value << std::endl;
}
template <typename T, typename... Args>
constexpr void print(T first, Args... args)
{
   std::cout << first << " ";
   print(args...);
}

template <typename T>
constexpr void print_error(T value)
{
   std::cerr << value << std::endl;
}
template <typename T, typename... Args>
constexpr void print_error(T first, Args... args)
{
   std::cerr << first << " ";
   print_error(args...);
}

// 簡単なルーティング関数
template <class Body, class Allocator, class Send>
void handle_request(http::request<Body, http::basic_fields<Allocator>> &&req, Send &&send)
{
   if (req.method() == http::verb::get && req.target() == "/hello")
   {
      http::string_body::value_type body = "Hello, REST!";
      auto const size = body.size();

      http::response<http::string_body> res{
          std::piecewise_construct,
          std::make_tuple(std::move(body)),
          std::make_tuple(http::status::ok, req.version())};

      res.set(http::field::server, "Boost.Beast REST Server");
      res.set(http::field::content_type, "text/plain");
      res.content_length(size);
      res.keep_alive(req.keep_alive());
      return send(std::move(res));
   }
   else if (req.method() == http::verb::post && req.target() == "/echo")
   {
      http::response<http::string_body> res{
          http::status::ok, req.version()};
      res.set(http::field::server, "Boost.Beast REST Server");
      res.set(http::field::content_type, "application/json");
      res.body() = req.body(); // リクエストボディをそのまま返す
      res.prepare_payload();
      res.keep_alive(req.keep_alive());
      return send(std::move(res));
   }
   else
   {
      http::response<http::string_body> res{
          http::status::not_found, req.version()};
      res.set(http::field::content_type, "text/plain");
      res.body() = "Not found";
      res.prepare_payload();
      return send(std::move(res));
   }
}

// セッション（1クライアント用）
void do_session(tcp::socket socket)
{
   bool close = false;
   beast::error_code ec;

   beast::flat_buffer buffer;

   while (!close)
   {
      http::request<http::string_body> req;
      http::read(socket, buffer, req, ec);
      if (ec == http::error::end_of_stream)
         break;
      if (ec)
      {
         print_error("読み取りエラー: ", ec.message());
         break;
      }

      // レスポンス送信用ラムダ
      auto const send = [&](auto &&response)
      {
         using response_type = typename std::decay<decltype(response)>::type;
         http::write(socket, response, ec);
      };

      handle_request(std::move(req), send);
   }

   // セッション終了
   socket.shutdown(tcp::socket::shutdown_send, ec);
}

int main()
{
   try
   {
      net::io_context ioc{1};

      tcp::acceptor acceptor{ioc, {tcp::v4(), 8080}};
      print("サーバー起動中: http://localhost:8080");

      while (true)
      {
         tcp::socket socket{ioc};
         acceptor.accept(socket);
         std::thread([sock = std::move(socket)]() mutable
                     { do_session(std::move(sock)); })
             .detach();
      }
   }
   catch (std::exception const &e)
   {
      print_error("エラー: ", e.what());
      return EXIT_FAILURE;
   }
}
