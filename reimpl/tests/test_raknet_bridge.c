#include "sampdll/net/raknet_bridge.h"

#include <stdio.h>
#include <string.h>

static int assert_true(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
  }
  return 0;
}

int main(void) {
  int failed = 0;
  const char *name = samp_raknet_bridge_variant_name();

  failed += assert_true(samp_raknet_bridge_is_available() == 1, "bridge available");
  failed += assert_true(name != NULL && strstr(name, "Knogle") != NULL, "variant name");
  failed += assert_true(samp_raknet_bridge_self_test() == 0, "factory self-test");

  if (failed != 0) {
    fprintf(stderr, "%d checks failed\n", failed);
    return 1;
  }

  fprintf(stdout, "All raknet bridge checks passed\n");
  return 0;
}
