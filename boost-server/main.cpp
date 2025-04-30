#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <iomanip>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/url/url_view.hpp>
#include <boost/url/parse.hpp>
#include <filesystem>

namespace fs = std::filesystem;
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace urls = boost::urls;
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

http::response<http::string_body> make_error_response(
    http::status status,
    std::string_view error_message,
    unsigned version,
    bool keep_alive)
{
   http::response<http::string_body> res{status, version};
   res.set(http::field::server, "Boost.Beast REST Server");
   res.set(http::field::content_type, "text/plain");
   res.keep_alive(keep_alive);
   res.body() = std::string(error_message);
   res.prepare_payload();
   return res;
}

template <class Body, class Allocator, class Send>
void handle_request(http::request<Body, http::basic_fields<Allocator>> &&req, Send &&send)
{
   auto const bad_request = [&](std::string_view why)
   {
      send(make_error_response(http::status::bad_request, why, req.version(), req.keep_alive()));
   };
   auto const not_found = [&]()
   {
      send(make_error_response(http::status::not_found, "Not found", req.version(), req.keep_alive()));
   };
   auto const server_error = [&](std::string_view what)
   {
      send(make_error_response(http::status::internal_server_error, what, req.version(), req.keep_alive()));
   };
   auto const forbidden = [&]()
   {
      send(make_error_response(http::status::forbidden, "Forbidden", req.version(), req.keep_alive()));
   };

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
      res.body() = req.body();
      res.prepare_payload();
      res.keep_alive(req.keep_alive());
      return send(std::move(res));
   }
   else if (req.method() == http::verb::get && req.target().starts_with("/cog?"))
   {
      urls::result<urls::url_view> rv = urls::parse_uri_reference(req.target());
      if (!rv)
      {
         return bad_request("Invalid URI");
      }
      urls::url_view uv = *rv;
      auto params = uv.params();
      auto it = params.find("path");
      if (it == params.end())
      {
         return bad_request("Missing 'path' query parameter");
      }
      std::string cog_path_str = (*it).value;
      fs::path cog_path(cog_path_str);
      fs::path base_dir = "./data";
      fs::path absolute_cog_path;
      try
      {
         absolute_cog_path = fs::weakly_canonical(base_dir / cog_path);
      }
      catch (const fs::filesystem_error &e)
      {
         print_error("Path canonicalization error: ", e.what());
         return bad_request("Invalid path format.");
      }

      print("Absolute path: ", absolute_cog_path.string());
      // if (absolute_cog_path.string().rfind(base_dir.string(), 0) != 0)
      // {
      //    print_error("Forbidden path access attempted: ", cog_path_str);
      //    return forbidden();
      // }
      if (!fs::exists(absolute_cog_path) || !fs::is_regular_file(absolute_cog_path))
      {
         return not_found();
      }

      beast::error_code ec;
      http::file_body::value_type file;

      file.open(cog_path_str.c_str(), beast::file_mode::scan, ec);

      if (ec == beast::errc::no_such_file_or_directory)
         return not_found();
      if (ec)
         return server_error("Failed to open file");

      auto const size = file.size();

      http::response<http::file_body> res{
          std::piecewise_construct,
          std::make_tuple(std::move(file)),
          std::make_tuple(http::status::ok, req.version())};

      res.set(http::field::server, "Boost.Beast REST Server");
      res.set(http::field::content_type, "image/tiff"); // GeoTIFF の MIME タイプ
      res.content_length(size);
      res.keep_alive(req.keep_alive());

      return send(std::move(res));
   }
   else
   {
      return not_found(); // どのルートにもマッチしない場合は 404
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
