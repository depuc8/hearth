#include "http/parser.hpp"
#include "static/static_handler.hpp"
#include "support/temp_dir.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>

#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;
using hearth::files::StaticHandler;
using hearth::http::Status;

namespace {

class StaticFiles : public ::testing::Test {
protected:
  void SetUp() override {
    tmp_.write("public/index.html", "<h1>home</h1>");
    tmp_.write("public/style.css", "body{}");
    tmp_.write("public/img/logo.svg", "<svg/>");
    tmp_.write("public/docs/index.html", "docs");
    tmp_.write("public/blob.bin", std::string("\0\1\2", 3));
    tmp_.write("secret.txt", "top secret");
    tmp_.write("public-evil/x.txt", "sibling dir");
    handler_.emplace(tmp_.path() / "public");
  }

  /// Runs a raw request line through the real parser, then the handler,
  /// so percent-decoding is exercised exactly as in the server.
  hearth::http::Response get(const std::string &target) {
    hearth::http::RequestParser parser;
    const auto result =
        parser.feed("GET " + target + " HTTP/1.1\r\nHost: h\r\n\r\n");
    if (result.status != hearth::http::ParseStatus::Complete) {
      return hearth::http::Response::error(parser.error_status());
    }
    return (*handler_)(parser.request());
  }

  hearth::test::TempDir tmp_;
  std::optional<StaticHandler> handler_;
};

} // namespace

TEST_F(StaticFiles, ServesFileWithTypeAndBody) {
  const auto r = get("/style.css");
  EXPECT_EQ(r.status, Status::Ok);
  EXPECT_EQ(r.body, "body{}");
  EXPECT_EQ(r.headers.get("Content-Type"), "text/css; charset=utf-8");
  EXPECT_NE(r.serialize().find("Content-Length: 6\r\n"), std::string::npos);
}

TEST_F(StaticFiles, MimeTypesFollowExtension) {
  EXPECT_EQ(get("/img/logo.svg").headers.get("Content-Type"), "image/svg+xml");
  EXPECT_EQ(get("/index.html").headers.get("Content-Type"), "text/html; charset=utf-8");
  EXPECT_EQ(get("/blob.bin").headers.get("Content-Type"), "application/octet-stream");
}

TEST_F(StaticFiles, BinaryContentIsExact) {
  EXPECT_EQ(get("/blob.bin").body, std::string("\0\1\2", 3));
}

TEST_F(StaticFiles, RootServesIndex) {
  const auto r = get("/");
  EXPECT_EQ(r.status, Status::Ok);
  EXPECT_EQ(r.body, "<h1>home</h1>");
  EXPECT_EQ(r.headers.get("Content-Type"), "text/html; charset=utf-8");
}

TEST_F(StaticFiles, DirectoryServesIndexWithOrWithoutSlash) {
  EXPECT_EQ(get("/docs/").body, "docs");
  EXPECT_EQ(get("/docs").body, "docs");
}

TEST_F(StaticFiles, DirectoryWithoutIndexIs404) {
  EXPECT_EQ(get("/img/").status, Status::NotFound);
}

TEST_F(StaticFiles, MissingFileIs404) {
  EXPECT_EQ(get("/nope.html").status, Status::NotFound);
  EXPECT_EQ(get("/style.css/extra").status, Status::NotFound);
}

TEST_F(StaticFiles, PercentEncodedNamesAreDecoded) {
  tmp_.write("public/with space.txt", "spaced");
  EXPECT_EQ(get("/with%20space.txt").body, "spaced");
}

TEST_F(StaticFiles, DotSegmentsAreHarmless) {
  EXPECT_EQ(get("/./style.css").body, "body{}");
  EXPECT_EQ(get("//style.css").body, "body{}");
}

TEST_F(StaticFiles, PlainTraversalIs403) {
  EXPECT_EQ(get("/../secret.txt").status, Status::Forbidden);
  EXPECT_EQ(get("/../etc/passwd").status, Status::Forbidden);
  EXPECT_EQ(get("/img/../../secret.txt").status, Status::Forbidden);
}

TEST_F(StaticFiles, EncodedTraversalIs403) {
  EXPECT_EQ(get("/%2e%2e/secret.txt").status, Status::Forbidden);
  EXPECT_EQ(get("/%2E%2E/%2E%2E/etc/passwd").status, Status::Forbidden);
  EXPECT_EQ(get("/.%2e/secret.txt").status, Status::Forbidden);
  EXPECT_EQ(get("/..%2fsecret.txt").status, Status::Forbidden);
  EXPECT_EQ(get("/img%2f..%2f..%2fsecret.txt").status, Status::Forbidden);
}

TEST_F(StaticFiles, InternalDotDotIsRejectedToo) {
  // Even a traversal that would land inside the root is refused: simpler to
  // reason about than normalizing.
  EXPECT_EQ(get("/img/../style.css").status, Status::Forbidden);
}

TEST_F(StaticFiles, SymlinkEscapingRootIs403) {
  fs::create_symlink(tmp_.path() / "secret.txt", tmp_.path() / "public" / "leak.txt");
  fs::create_directory_symlink(tmp_.path(), tmp_.path() / "public" / "up");
  EXPECT_EQ(get("/leak.txt").status, Status::Forbidden);
  EXPECT_EQ(get("/up/secret.txt").status, Status::Forbidden);
}

TEST_F(StaticFiles, SymlinkInsideRootIsServed) {
  fs::create_symlink(tmp_.path() / "public" / "style.css",
                     tmp_.path() / "public" / "alias.css");
  EXPECT_EQ(get("/alias.css").body, "body{}");
}

TEST_F(StaticFiles, SiblingDirectoryWithSharedPrefixIsNotInside) {
  EXPECT_FALSE(hearth::files::is_within(tmp_.path() / "public",
                                        tmp_.path() / "public-evil" / "x.txt"));
  EXPECT_TRUE(hearth::files::is_within(tmp_.path() / "public",
                                       tmp_.path() / "public" / "x.txt"));
}

TEST_F(StaticFiles, UnreadableFileIs403) {
  if (::geteuid() == 0) {
    GTEST_SKIP() << "root can read anything";
  }
  const auto file = tmp_.write("public/locked.txt", "x");
  ::chmod(file.c_str(), 0);
  EXPECT_EQ(get("/locked.txt").status, Status::Forbidden);
}

TEST(StaticHandler, RejectsMissingOrNonDirectoryRoot) {
  hearth::test::TempDir tmp;
  const auto file = tmp.write("file.txt", "x");
  EXPECT_ANY_THROW(StaticHandler(tmp.path() / "missing"));
  EXPECT_ANY_THROW(StaticHandler{file});
}
