#include "http/headers.hpp"
#include "http/mime.hpp"
#include "http/response.hpp"
#include "http/status.hpp"
#include "http/url.hpp"

#include <gtest/gtest.h>

using namespace hearth::http;

TEST(Status, ReasonPhrases) {
  EXPECT_EQ(reason_phrase(Status::Ok), "OK");
  EXPECT_EQ(reason_phrase(Status::NotFound), "Not Found");
  EXPECT_EQ(reason_phrase(Status::RequestHeaderFieldsTooLarge),
            "Request Header Fields Too Large");
  EXPECT_EQ(reason_phrase(static_cast<Status>(299)), "Unknown");
  EXPECT_EQ(code(Status::MethodNotAllowed), 405);
}

TEST(Headers, SetReplacesAllCaseInsensitively) {
  Headers headers;
  headers.add("X-A", "1");
  headers.add("x-a", "2");
  headers.set("X-a", "3");
  EXPECT_EQ(headers.count("X-A"), 1U);
  EXPECT_EQ(headers.get("x-A"), "3");
  headers.remove("X-A");
  EXPECT_TRUE(headers.empty());
}

TEST(Headers, TokenListMatching) {
  EXPECT_TRUE(list_contains_token("keep-alive, Upgrade", "upgrade"));
  EXPECT_TRUE(list_contains_token("  close  ", "close"));
  EXPECT_FALSE(list_contains_token("closed", "close"));
  EXPECT_FALSE(list_contains_token("", "close"));
}

TEST(Url, PercentDecode) {
  EXPECT_EQ(percent_decode("/a%20b"), "/a b");
  EXPECT_EQ(percent_decode("%2e%2E%2f"), "../");
  EXPECT_EQ(percent_decode("a+b"), "a+b");
  EXPECT_FALSE(percent_decode("%").has_value());
  EXPECT_FALSE(percent_decode("%4").has_value());
  EXPECT_FALSE(percent_decode("%g0").has_value());
  EXPECT_FALSE(percent_decode("%00").has_value());
}

TEST(Response, SerializeAddsContentLength) {
  const auto r = Response::text(Status::Ok, "hello");
  EXPECT_EQ(r.serialize(), "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain; charset=utf-8\r\n"
                           "Content-Length: 5\r\n"
                           "\r\n"
                           "hello");
}

TEST(Response, HeadOmitsBodyButKeepsLength) {
  const auto r = Response::text(Status::Ok, "hello");
  const auto wire = r.serialize(false);
  EXPECT_NE(wire.find("Content-Length: 5\r\n"), std::string::npos);
  EXPECT_EQ(wire.substr(wire.size() - 4), "\r\n\r\n");
}

TEST(Response, NoContentHasNoBodyOrLength) {
  Response r;
  r.status = Status::NoContent;
  r.body = "ignored";
  EXPECT_EQ(r.serialize(), "HTTP/1.1 204 No Content\r\n\r\n");
}

TEST(Response, ErrorPage) {
  const auto r = Response::error(Status::NotFound);
  EXPECT_EQ(r.status, Status::NotFound);
  EXPECT_EQ(r.body, "404 Not Found\n");
  EXPECT_EQ(r.headers.get("Content-Type"), "text/plain; charset=utf-8");
}

TEST(Response, HttpDateFormat) {
  EXPECT_EQ(http_date(784111777), "Sun, 06 Nov 1994 08:49:37 GMT");
}

TEST(Mime, KnownExtensions) {
  EXPECT_EQ(mime_type_for("/index.html"), "text/html; charset=utf-8");
  EXPECT_EQ(mime_type_for("style.css"), "text/css; charset=utf-8");
  EXPECT_EQ(mime_type_for("/js/app.js"), "text/javascript; charset=utf-8");
  EXPECT_EQ(mime_type_for("logo.svg"), "image/svg+xml");
  EXPECT_EQ(mime_type_for("photo.jpeg"), "image/jpeg");
  EXPECT_EQ(mime_type_for("data.json"), "application/json");
}

TEST(Mime, ExtensionIsCaseInsensitive) {
  EXPECT_EQ(mime_type_for("PHOTO.PNG"), "image/png");
  EXPECT_EQ(mime_type_for("Index.HtMl"), "text/html; charset=utf-8");
}

TEST(Mime, UnknownOrMissingExtension) {
  EXPECT_EQ(mime_type_for("archive.xyz"), "application/octet-stream");
  EXPECT_EQ(mime_type_for("Makefile"), "application/octet-stream");
  EXPECT_EQ(mime_type_for("trailing."), "application/octet-stream");
  EXPECT_EQ(mime_type_for("/dir.d/file"), "application/octet-stream");
}
