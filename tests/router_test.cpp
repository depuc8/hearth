#include "router/router.hpp"

#include <gtest/gtest.h>

using hearth::http::Request;
using hearth::http::Response;
using hearth::http::Status;
using hearth::router::prefix_matches;
using hearth::router::Router;

namespace {

Request make(std::string method, std::string path) {
  Request r;
  r.method = std::move(method);
  r.path = std::move(path);
  return r;
}

/// Handler that answers with its own name so tests can see who ran.
hearth::router::Handler named(std::string name) {
  return [name = std::move(name)](const Request &) {
    return Response::text(Status::Ok, name);
  };
}

} // namespace

TEST(PrefixMatch, SegmentBoundaries) {
  EXPECT_TRUE(prefix_matches("/static", "/static"));
  EXPECT_TRUE(prefix_matches("/static", "/static/app.js"));
  EXPECT_FALSE(prefix_matches("/static", "/staticfoo"));
  EXPECT_TRUE(prefix_matches("/static/", "/static/app.js"));
  EXPECT_FALSE(prefix_matches("/static/", "/static"));
  EXPECT_TRUE(prefix_matches("/", "/anything/at/all"));
  EXPECT_FALSE(prefix_matches("/api", "/"));
}

TEST(Router, ExactMatch) {
  Router router;
  router.add("GET", "/hello", named("hello"));
  const auto r = router.dispatch(make("GET", "/hello"));
  EXPECT_EQ(r.status, Status::Ok);
  EXPECT_EQ(r.body, "hello");
}

TEST(Router, ExactDoesNotMatchSubpaths) {
  Router router;
  router.add("GET", "/hello", named("hello"));
  EXPECT_EQ(router.dispatch(make("GET", "/hello/x")).status, Status::NotFound);
  EXPECT_EQ(router.dispatch(make("GET", "/hell")).status, Status::NotFound);
}

TEST(Router, PrefixMatch) {
  Router router;
  router.add_prefix("GET", "/files", named("files"));
  EXPECT_EQ(router.dispatch(make("GET", "/files/a/b.txt")).body, "files");
  EXPECT_EQ(router.dispatch(make("GET", "/files")).body, "files");
  EXPECT_EQ(router.dispatch(make("GET", "/filesystem")).status, Status::NotFound);
}

TEST(Router, ExactBeatsPrefix) {
  Router router;
  router.add_prefix("GET", "/", named("static"));
  router.add("GET", "/stats", named("stats"));
  EXPECT_EQ(router.dispatch(make("GET", "/stats")).body, "stats");
  EXPECT_EQ(router.dispatch(make("GET", "/stats.html")).body, "static");
}

TEST(Router, LongestPrefixWins) {
  Router router;
  router.add_prefix("GET", "/", named("root"));
  router.add_prefix("GET", "/api/v1", named("v1"));
  router.add_prefix("GET", "/api", named("api"));
  EXPECT_EQ(router.dispatch(make("GET", "/api/v1/users")).body, "v1");
  EXPECT_EQ(router.dispatch(make("GET", "/api/v2/users")).body, "api");
  EXPECT_EQ(router.dispatch(make("GET", "/other")).body, "root");
}

TEST(Router, FirstRegisteredWinsTies) {
  Router router;
  router.add("GET", "/dup", named("first"));
  router.add("GET", "/dup", named("second"));
  EXPECT_EQ(router.dispatch(make("GET", "/dup")).body, "first");
}

TEST(Router, FallsThroughToLessSpecificRouteForMethod) {
  Router router;
  router.add("GET", "/api/thing", named("get-thing"));
  router.add_prefix("POST", "/api", named("post-api"));
  EXPECT_EQ(router.dispatch(make("POST", "/api/thing")).body, "post-api");
}

TEST(Router, UnknownPathIs404) {
  Router router;
  router.add("GET", "/a", named("a"));
  const auto r = router.dispatch(make("GET", "/b"));
  EXPECT_EQ(r.status, Status::NotFound);
  EXPECT_FALSE(r.headers.contains("Allow"));
}

TEST(Router, WrongMethodIs405WithAllow) {
  Router router;
  router.add("GET", "/res", named("get"));
  router.add("POST", "/res", named("post"));
  router.add("DELETE", "/other", named("other"));
  const auto r = router.dispatch(make("PUT", "/res"));
  EXPECT_EQ(r.status, Status::MethodNotAllowed);
  EXPECT_EQ(r.headers.get("Allow"), "GET, HEAD, POST");
}

TEST(Router, AllowUnionsAllMatchingRoutes) {
  Router router;
  router.add_prefix("GET", "/", named("static"));
  router.add("POST", "/upload", named("upload"));
  const auto r = router.dispatch(make("DELETE", "/upload"));
  EXPECT_EQ(r.status, Status::MethodNotAllowed);
  EXPECT_EQ(r.headers.get("Allow"), "GET, HEAD, POST");
}

TEST(Router, HeadFallsBackToGet) {
  Router router;
  router.add("GET", "/page", named("page"));
  const auto r = router.dispatch(make("HEAD", "/page"));
  EXPECT_EQ(r.status, Status::Ok);
  EXPECT_EQ(r.body, "page");
}

TEST(Router, ExplicitHeadRouteBeatsGet) {
  Router router;
  router.add("GET", "/page", named("get"));
  router.add("HEAD", "/page", named("head"));
  EXPECT_EQ(router.dispatch(make("HEAD", "/page")).body, "head");
}

TEST(Router, MethodsAreCaseSensitive) {
  Router router;
  router.add("GET", "/x", named("x"));
  EXPECT_EQ(router.dispatch(make("get", "/x")).status, Status::MethodNotAllowed);
}

TEST(Router, EmptyRouterIs404) {
  const Router router;
  EXPECT_EQ(router.dispatch(make("GET", "/")).status, Status::NotFound);
}
