#include "http/parser.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using hearth::http::ParserLimits;
using hearth::http::ParseStatus;
using hearth::http::RequestParser;
using hearth::http::Status;

namespace {

/// Feeds the whole input at once.
ParseStatus parse_all(RequestParser &parser, std::string_view input) {
  return parser.feed(input).status;
}

/// Feeds one byte at a time; returns the final status and checks that the
/// parser never reports a result before the last byte it needs.
ParseStatus parse_bytewise(RequestParser &parser, std::string_view input) {
  ParseStatus status = ParseStatus::Incomplete;
  for (std::size_t i = 0; i < input.size(); ++i) {
    const auto result = parser.feed(input.substr(i, 1));
    status = result.status;
    if (status != ParseStatus::Incomplete) {
      EXPECT_EQ(result.consumed, 1U);
      break;
    }
  }
  return status;
}

Status error_for(std::string_view input, ParserLimits limits = {}) {
  RequestParser parser(limits);
  EXPECT_EQ(parse_all(parser, input), ParseStatus::Error) << input;
  return parser.error_status();
}

} // namespace

// --- Valid requests --------------------------------------------------------

TEST(Parser, SimpleGet) {
  RequestParser parser;
  const std::string input = "GET /index.html HTTP/1.1\r\nHost: example.com\r\n\r\n";
  const auto result = parser.feed(input);
  ASSERT_EQ(result.status, ParseStatus::Complete);
  EXPECT_EQ(result.consumed, input.size());
  const auto &req = parser.request();
  EXPECT_EQ(req.method, "GET");
  EXPECT_EQ(req.target, "/index.html");
  EXPECT_EQ(req.path, "/index.html");
  EXPECT_EQ(req.query, "");
  EXPECT_EQ(req.version_major, 1);
  EXPECT_EQ(req.version_minor, 1);
  EXPECT_EQ(req.headers.get("Host"), "example.com");
  EXPECT_TRUE(req.body.empty());
}

TEST(Parser, SplitsQueryAndDecodesPath) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "GET /a%20b/c?x=1&y=%20 HTTP/1.1\r\nHost: h\r\n\r\n"),
            ParseStatus::Complete);
  EXPECT_EQ(parser.request().path, "/a b/c");
  EXPECT_EQ(parser.request().query, "x=1&y=%20");
  EXPECT_EQ(parser.request().target, "/a%20b/c?x=1&y=%20");
}

TEST(Parser, AbsoluteFormTargetIsReducedToPath) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "GET http://example.com:8080/x?q HTTP/1.1\r\nHost: "
                              "example.com\r\n\r\n"),
            ParseStatus::Complete);
  EXPECT_EQ(parser.request().path, "/x");
  EXPECT_EQ(parser.request().query, "q");
}

TEST(Parser, AsteriskFormForOptions) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "OPTIONS * HTTP/1.1\r\nHost: h\r\n\r\n"), ParseStatus::Complete);
  EXPECT_EQ(parser.request().path, "*");
}

TEST(Parser, Http10WithoutHostIsAccepted) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "GET / HTTP/1.0\r\n\r\n"), ParseStatus::Complete);
  EXPECT_EQ(parser.request().version_minor, 0);
}

TEST(Parser, HeaderValueWhitespaceIsTrimmed) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "GET / HTTP/1.1\r\nHost:   h  \t\r\nX-Empty:\r\n\r\n"),
            ParseStatus::Complete);
  EXPECT_EQ(parser.request().headers.get("Host"), "h");
  EXPECT_EQ(parser.request().headers.get("X-Empty"), "");
}

TEST(Parser, HeaderNamesAreCaseInsensitive) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "GET / HTTP/1.1\r\nhOsT: h\r\nCONTENT-type: a/b\r\n\r\n"),
            ParseStatus::Complete);
  const auto &headers = parser.request().headers;
  EXPECT_EQ(headers.get("host"), "h");
  EXPECT_EQ(headers.get("HOST"), "h");
  EXPECT_EQ(headers.get("Content-Type"), "a/b");
  EXPECT_FALSE(headers.get("Content-Length").has_value());
}

TEST(Parser, DuplicateGenericHeadersArePreservedInOrder) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "GET / HTTP/1.1\r\nHost: h\r\nAccept: a\r\naccept: b\r\n\r\n"),
            ParseStatus::Complete);
  const auto &headers = parser.request().headers;
  EXPECT_EQ(headers.count("Accept"), 2U);
  EXPECT_EQ(headers.get("Accept"), "a");
  EXPECT_EQ(headers.all()[2].value, "b");
}

TEST(Parser, ObsTextInHeaderValueIsAllowed) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "GET / HTTP/1.1\r\nHost: h\r\nX-Name: caf\xc3\xa9\r\n\r\n"),
            ParseStatus::Complete);
}

// --- Partial reads ----------------------------------------------------------

TEST(Parser, ByteByByteGet) {
  RequestParser parser;
  ASSERT_EQ(parse_bytewise(parser, "GET /p?q=1 HTTP/1.1\r\nHost: h\r\nX-A: 1\r\n\r\n"),
            ParseStatus::Complete);
  EXPECT_EQ(parser.request().path, "/p");
  EXPECT_EQ(parser.request().headers.get("X-A"), "1");
}

TEST(Parser, ByteByByteWithBody) {
  RequestParser parser;
  ASSERT_EQ(parse_bytewise(parser, "POST /u HTTP/1.1\r\nHost: h\r\nContent-Length: "
                                   "11\r\n\r\nhello world"),
            ParseStatus::Complete);
  EXPECT_EQ(parser.request().body, "hello world");
}

TEST(Parser, ArbitraryChunkBoundaries) {
  const std::string input = "PUT /x HTTP/1.1\r\nHost: h\r\nContent-Length: 5\r\n\r\nabcde";
  for (std::size_t split = 1; split < input.size(); ++split) {
    RequestParser parser;
    const auto first = parser.feed(std::string_view(input).substr(0, split));
    ASSERT_EQ(first.status, ParseStatus::Incomplete) << "split=" << split;
    ASSERT_EQ(first.consumed, split);
    const auto second = parser.feed(std::string_view(input).substr(split));
    ASSERT_EQ(second.status, ParseStatus::Complete) << "split=" << split;
    EXPECT_EQ(parser.request().body, "abcde");
  }
}

TEST(Parser, IncompleteHeadersStayIncomplete) {
  RequestParser parser;
  EXPECT_EQ(parse_all(parser, "GET / HTTP/1.1\r\nHost: h\r\n"), ParseStatus::Incomplete);
  EXPECT_TRUE(parser.in_progress());
}

TEST(Parser, IncompleteBodyStaysIncomplete) {
  RequestParser parser;
  EXPECT_EQ(parse_all(parser, "POST / HTTP/1.1\r\nHost: h\r\nContent-Length: 10\r\n\r\n12345"),
            ParseStatus::Incomplete);
  EXPECT_EQ(parse_all(parser, "67890"), ParseStatus::Complete);
  EXPECT_EQ(parser.request().body, "1234567890");
}

// --- Pipelining & reset ------------------------------------------------------

TEST(Parser, PipelinedRequestsReportConsumedBytes) {
  const std::string first = "GET /a HTTP/1.1\r\nHost: h\r\n\r\n";
  const std::string second = "GET /b HTTP/1.1\r\nHost: h\r\n\r\n";
  const std::string both = first + second;
  RequestParser parser;
  auto result = parser.feed(both);
  ASSERT_EQ(result.status, ParseStatus::Complete);
  EXPECT_EQ(result.consumed, first.size());
  EXPECT_EQ(parser.request().path, "/a");

  parser.reset();
  result = parser.feed(std::string_view(both).substr(result.consumed));
  ASSERT_EQ(result.status, ParseStatus::Complete);
  EXPECT_EQ(parser.request().path, "/b");
}

TEST(Parser, BodyDoesNotSwallowNextRequest) {
  const std::string input = "POST / HTTP/1.1\r\nHost: h\r\nContent-Length: 3\r\n\r\nabcGET";
  RequestParser parser;
  const auto result = parser.feed(input);
  ASSERT_EQ(result.status, ParseStatus::Complete);
  EXPECT_EQ(parser.request().body, "abc");
  EXPECT_EQ(input.substr(result.consumed), "GET");
}

TEST(Parser, FeedAfterCompleteConsumesNothing) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "GET / HTTP/1.1\r\nHost: h\r\n\r\n"), ParseStatus::Complete);
  const auto result = parser.feed("GET / HTTP/1.1\r\n");
  EXPECT_EQ(result.status, ParseStatus::Complete);
  EXPECT_EQ(result.consumed, 0U);
}

TEST(Parser, ResetClearsPreviousRequest) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "POST /a HTTP/1.1\r\nHost: h\r\nContent-Length: 1\r\n\r\nx"),
            ParseStatus::Complete);
  parser.reset();
  EXPECT_FALSE(parser.in_progress());
  ASSERT_EQ(parse_all(parser, "GET /b HTTP/1.1\r\nHost: h\r\n\r\n"), ParseStatus::Complete);
  EXPECT_EQ(parser.request().path, "/b");
  EXPECT_TRUE(parser.request().body.empty());
  EXPECT_EQ(parser.request().headers.size(), 1U);
}

// --- CRLF edge cases ----------------------------------------------------------

TEST(Parser, BareLfLineEndingsAreAccepted) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "GET / HTTP/1.1\nHost: h\n\n"), ParseStatus::Complete);
  EXPECT_EQ(parser.request().headers.get("Host"), "h");
}

TEST(Parser, LeadingEmptyLinesAreIgnored) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "\r\n\r\nGET / HTTP/1.1\r\nHost: h\r\n\r\n"), ParseStatus::Complete);
  EXPECT_EQ(parser.request().method, "GET");
}

TEST(Parser, LeadingEmptyLinesDoNotMarkInProgress) {
  RequestParser parser;
  EXPECT_EQ(parse_all(parser, "\r\n"), ParseStatus::Incomplete);
  EXPECT_FALSE(parser.in_progress());
}

TEST(Parser, BareCrInsideLineIsRejected) {
  EXPECT_EQ(error_for("GET / HTTP/1.1\r\nHost: h\rX: y\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, CrLfSplitAcrossFeeds) {
  RequestParser parser;
  EXPECT_EQ(parse_all(parser, "GET / HTTP/1.1\r"), ParseStatus::Incomplete);
  EXPECT_EQ(parse_all(parser, "\nHost: h\r"), ParseStatus::Incomplete);
  EXPECT_EQ(parse_all(parser, "\n\r"), ParseStatus::Incomplete);
  EXPECT_EQ(parse_all(parser, "\n"), ParseStatus::Complete);
}

// --- Malformed request lines ---------------------------------------------------

TEST(Parser, RejectsMissingParts) {
  EXPECT_EQ(error_for("GET /\r\n\r\n"), Status::BadRequest);
  EXPECT_EQ(error_for("GET\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, RejectsExtraSpaces) {
  EXPECT_EQ(error_for("GET  / HTTP/1.1\r\nHost: h\r\n\r\n"), Status::BadRequest);
  EXPECT_EQ(error_for("GET / HTTP/1.1 \r\nHost: h\r\n\r\n"), Status::BadRequest);
  EXPECT_EQ(error_for(" GET / HTTP/1.1\r\nHost: h\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, RejectsInvalidMethodCharacters) {
  EXPECT_EQ(error_for("GE(T / HTTP/1.1\r\nHost: h\r\n\r\n"), Status::BadRequest);
  EXPECT_EQ(error_for("G\x01T / HTTP/1.1\r\nHost: h\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, RejectsInvalidTargets) {
  EXPECT_EQ(error_for("GET index.html HTTP/1.1\r\nHost: h\r\n\r\n"), Status::BadRequest);
  EXPECT_EQ(error_for("GET /a#frag HTTP/1.1\r\nHost: h\r\n\r\n"), Status::BadRequest);
  EXPECT_EQ(error_for("GET /a\x7f HTTP/1.1\r\nHost: h\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, RejectsBadPercentEncoding) {
  EXPECT_EQ(error_for("GET /a%zz HTTP/1.1\r\nHost: h\r\n\r\n"), Status::BadRequest);
  EXPECT_EQ(error_for("GET /a%2 HTTP/1.1\r\nHost: h\r\n\r\n"), Status::BadRequest);
  EXPECT_EQ(error_for("GET /a%00b HTTP/1.1\r\nHost: h\r\n\r\n"), Status::BadRequest);
}

// --- Versions ---------------------------------------------------------------------

TEST(Parser, RejectsMalformedVersions) {
  for (const char *version :
       {"HTTP/1", "HTTP/1.1.1", "http/1.1", "HTTP/x.1", "HTTP1.1", "HTTP/11", "HTTPS/1.1"}) {
    EXPECT_EQ(error_for(std::string("GET / ") + version + "\r\nHost: h\r\n\r\n"),
              Status::BadRequest)
        << version;
  }
}

TEST(Parser, UnsupportedVersionsGet505) {
  EXPECT_EQ(error_for("GET / HTTP/2.0\r\nHost: h\r\n\r\n"), Status::HttpVersionNotSupported);
  EXPECT_EQ(error_for("GET / HTTP/0.9\r\nHost: h\r\n\r\n"), Status::HttpVersionNotSupported);
  EXPECT_EQ(error_for("GET / HTTP/1.2\r\nHost: h\r\n\r\n"), Status::HttpVersionNotSupported);
}

// --- Headers ------------------------------------------------------------------------

TEST(Parser, RejectsHeaderWithoutColon) {
  EXPECT_EQ(error_for("GET / HTTP/1.1\r\nHost h\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, RejectsWhitespaceBeforeColon) {
  EXPECT_EQ(error_for("GET / HTTP/1.1\r\nHost : h\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, RejectsEmptyHeaderName) {
  EXPECT_EQ(error_for("GET / HTTP/1.1\r\n: h\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, RejectsObsoleteLineFolding) {
  EXPECT_EQ(error_for("GET / HTTP/1.1\r\nHost: h\r\nX-A: one\r\n two\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, RejectsControlCharactersInValue) {
  EXPECT_EQ(error_for("GET / HTTP/1.1\r\nHost: h\r\nX-A: a\x01b\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, MissingHostOnHttp11Is400) {
  EXPECT_EQ(error_for("GET / HTTP/1.1\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, DuplicateHostIs400) {
  EXPECT_EQ(error_for("GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n"), Status::BadRequest);
}

TEST(Parser, OversizedHeaderSectionIs431) {
  const std::string big(9000, 'a');
  EXPECT_EQ(error_for("GET / HTTP/1.1\r\nHost: h\r\nX-Big: " + big + "\r\n\r\n"),
            Status::RequestHeaderFieldsTooLarge);
}

TEST(Parser, OversizedHeaderIsDetectedBeforeLineEnds) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "GET / HTTP/1.1\r\nX-Big: "), ParseStatus::Incomplete);
  const std::string chunk(9000, 'a'); // no newline yet
  EXPECT_EQ(parse_all(parser, chunk), ParseStatus::Error);
  EXPECT_EQ(parser.error_status(), Status::RequestHeaderFieldsTooLarge);
}

TEST(Parser, OversizedRequestLineIs414) {
  const std::string path(9000, 'a');
  EXPECT_EQ(error_for("GET /" + path + " HTTP/1.1\r\nHost: h\r\n\r\n"), Status::UriTooLong);
}

TEST(Parser, TooManyHeadersIs431) {
  ParserLimits limits;
  limits.max_header_count = 3;
  EXPECT_EQ(error_for("GET / HTTP/1.1\r\nHost: h\r\nA: 1\r\nB: 2\r\nC: 3\r\n\r\n", limits),
            Status::RequestHeaderFieldsTooLarge);
}

TEST(Parser, HeaderLimitIsConfigurable) {
  ParserLimits limits;
  limits.max_header_bytes = 32;
  EXPECT_EQ(error_for("GET / HTTP/1.1\r\nHost: abcdefghijklmnop\r\n\r\n", limits),
            Status::RequestHeaderFieldsTooLarge);
}

// --- Bodies ---------------------------------------------------------------------------

TEST(Parser, ZeroContentLengthCompletesImmediately) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "POST / HTTP/1.1\r\nHost: h\r\nContent-Length: 0\r\n\r\n"),
            ParseStatus::Complete);
  EXPECT_TRUE(parser.request().body.empty());
}

TEST(Parser, BinaryBodyIsPreserved) {
  const std::string body("a\0b\r\n\xff", 6);
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "POST / HTTP/1.1\r\nHost: h\r\nContent-Length: 6\r\n\r\n" + body),
            ParseStatus::Complete);
  EXPECT_EQ(parser.request().body, body);
}

TEST(Parser, InvalidContentLengthIs400) {
  for (const char *value :
       {"abc", "-1", "1.5", "", "0x10", "+5", "5 5", "99999999999999999999999999"}) {
    EXPECT_EQ(error_for(std::string("POST / HTTP/1.1\r\nHost: h\r\nContent-Length: ") + value +
                        "\r\n\r\n"),
              Status::BadRequest)
        << "'" << value << "'";
  }
}

TEST(Parser, ConflictingContentLengthsAre400) {
  EXPECT_EQ(error_for("POST / HTTP/1.1\r\nHost: h\r\nContent-Length: 1\r\nContent-Length: "
                      "2\r\n\r\nab"),
            Status::BadRequest);
}

TEST(Parser, IdenticalDuplicateContentLengthsAreAccepted) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "POST / HTTP/1.1\r\nHost: h\r\nContent-Length: 2\r\n"
                              "content-length: 2\r\n\r\nab"),
            ParseStatus::Complete);
  EXPECT_EQ(parser.request().body, "ab");
}

TEST(Parser, OversizedBodyIs413) {
  ParserLimits limits;
  limits.max_body_bytes = 10;
  EXPECT_EQ(error_for("POST / HTTP/1.1\r\nHost: h\r\nContent-Length: 11\r\n\r\n", limits),
            Status::PayloadTooLarge);
}

TEST(Parser, TransferEncodingIs501) {
  EXPECT_EQ(error_for("POST / HTTP/1.1\r\nHost: h\r\nTransfer-Encoding: chunked\r\n\r\n"),
            Status::NotImplemented);
}

TEST(Parser, TransferEncodingWithContentLengthIs400) {
  EXPECT_EQ(error_for("POST / HTTP/1.1\r\nHost: h\r\nTransfer-Encoding: chunked\r\n"
                      "Content-Length: 3\r\n\r\nabc"),
            Status::BadRequest);
}

TEST(Parser, ErrorIsSticky) {
  RequestParser parser;
  ASSERT_EQ(parse_all(parser, "BAD\r\n"), ParseStatus::Error);
  const auto result = parser.feed("GET / HTTP/1.1\r\nHost: h\r\n\r\n");
  EXPECT_EQ(result.status, ParseStatus::Error);
  EXPECT_EQ(result.consumed, 0U);
  EXPECT_FALSE(parser.error_reason().empty());
}

// --- Keep-alive semantics -----------------------------------------------------------

TEST(Request, KeepAliveDefaults) {
  RequestParser p11;
  ASSERT_EQ(parse_all(p11, "GET / HTTP/1.1\r\nHost: h\r\n\r\n"), ParseStatus::Complete);
  EXPECT_TRUE(p11.request().keep_alive());

  RequestParser p10;
  ASSERT_EQ(parse_all(p10, "GET / HTTP/1.0\r\n\r\n"), ParseStatus::Complete);
  EXPECT_FALSE(p10.request().keep_alive());
}

TEST(Request, ConnectionHeaderOverridesDefault) {
  RequestParser close11;
  ASSERT_EQ(parse_all(close11, "GET / HTTP/1.1\r\nHost: h\r\nConnection: Close\r\n\r\n"),
            ParseStatus::Complete);
  EXPECT_FALSE(close11.request().keep_alive());

  RequestParser keep10;
  ASSERT_EQ(parse_all(keep10, "GET / HTTP/1.0\r\nConnection: Keep-Alive\r\n\r\n"),
            ParseStatus::Complete);
  EXPECT_TRUE(keep10.request().keep_alive());

  RequestParser list;
  ASSERT_EQ(parse_all(list, "GET / HTTP/1.1\r\nHost: h\r\nConnection: upgrade, close\r\n\r\n"),
            ParseStatus::Complete);
  EXPECT_FALSE(list.request().keep_alive());
}
