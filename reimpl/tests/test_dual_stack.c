#include "sampdll/net/dual_stack.h"

#include <stdio.h>
#include <string.h>

/* SPEC-NET-002 conformance checks. */

static int assert_true(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
  }
  return 0;
}

int main(void) {
  int failed = 0;
  samp_endpoint ep;

  failed += assert_true(samp_net_parse_hostport("example.com:7777", 7777, &ep) == 0, "parse dns:port");
  failed += assert_true(strcmp(ep.host, "example.com") == 0, "host dns:port");
  failed += assert_true(ep.port == 7777, "port dns:port");

  failed += assert_true(samp_net_parse_hostport("203.0.113.10", 7777, &ep) == 0, "parse ipv4");
  failed += assert_true(strcmp(ep.host, "203.0.113.10") == 0, "host ipv4");
  failed += assert_true(ep.port == 7777, "port ipv4 default");

  failed += assert_true(samp_net_parse_hostport("[2001:db8::10]:7777", 7777, &ep) == 0, "parse bracket ipv6");
  failed += assert_true(strcmp(ep.host, "2001:db8::10") == 0, "host bracket ipv6");
  failed += assert_true(ep.port == 7777, "port bracket ipv6");

  failed += assert_true(samp_net_parse_hostport("2001:db8::10", 7777, &ep) == 0, "parse plain ipv6");
  failed += assert_true(strcmp(ep.host, "2001:db8::10") == 0, "host plain ipv6");
  failed += assert_true(ep.port == 7777, "port plain ipv6 default");

  failed += assert_true(samp_net_parse_hostport("[2001:db8::10", 7777, &ep) != 0, "reject broken bracket");
  failed += assert_true(samp_net_parse_hostport("example.com:", 7777, &ep) != 0, "reject empty port");
  failed += assert_true(samp_net_parse_hostport(":7777", 7777, &ep) != 0, "reject empty host");
  failed += assert_true(samp_net_parse_hostport("example.com:99999", 7777, &ep) != 0, "reject out-of-range port");

  if (failed != 0) {
    fprintf(stderr, "%d checks failed\n", failed);
    return 1;
  }

  fprintf(stdout, "All dual-stack parser checks passed\n");
  return 0;
}
