/* -*- Mode: C++ -*- */

#include <cstdio>
#include <cstdlib>
#include <cinttypes>
#include <cstdint>
#include <cstring>

#include <getopt.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>

#include <ohci-constants.h>

static char usage_peek[] = "Usage: %s [-q] [-p port] guid/nodeno address length\n";
static char usage_poke[] = "Usage: %s [-q] [-p port] guid/nodeno address\n";

const char *strippath(const char *name)
{
  char *s = strrchr(name, '/');

  if (s == NULL)
    return name;
  else
    return s+1;
}

int
main(int argc, char **argv)
{
  /* Command line parsing */
  int opt;
  unsigned port = 0;
  bool quadletwise = false;

  enum { INVALID, PEEK, POKE } mode = INVALID;

  const char *name = strippath(argv[0]);
  if (strcmp(name, "fw_peek") == 0) {
    mode = PEEK;
  } else if (strcmp(name, "fw_poke") == 0) {
    mode = POKE;
  }

  if (mode == INVALID) {
    fprintf(stderr, "Could not decide, whether we are fw_peek or fw_poke.\n");
    return EXIT_FAILURE;
  }

  while ((opt = getopt(argc, argv, "qp:")) != -1) {
    switch (opt) {
    case 'q':
      quadletwise = true;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    default:
      goto print_usage;
    }
  }

  if (((mode == PEEK) && (argc - optind) != 3) ||
      ((mode == POKE) && (argc - optind) != 2)) {
  print_usage:
    fprintf(stderr, (mode == PEEK) ? usage_peek : usage_poke, argv[0]);
    return EXIT_FAILURE;
  }

  uint64_t guid    = strtoull(argv[optind],     NULL, 0);
  uint64_t address = strtoull(argv[optind + 1], NULL, 0);
  uint64_t length  = strtoull(argv[optind + 2], NULL, 0);

  raw1394handle_t fw_handle = raw1394_new_handle_on_port(port);

  if (fw_handle == NULL) {
    perror("raw1394_new_handle_on_port");
    return EXIT_FAILURE;
  }

  nodeid_t target;

  // 63 is broadcast. Ignore that.
  if (guid < 63) {
    // GUID is actually a node number.
    target = LOCAL_BUS | (nodeid_t)guid;
  } else {
    for (unsigned no = 0; no < 63; no++) {
      nodeid_t test_node = LOCAL_BUS | (nodeid_t)no;
      uint32_t guid_hi;
      uint32_t guid_lo;

      int res = raw1394_read(fw_handle, test_node, CSR_REGISTER_BASE + CSR_CONFIG_ROM + 4*4,
			     4, &guid_lo);
      if (res != 0) { perror("read guid_lo"); return -1; }
      res = raw1394_read(fw_handle, test_node, CSR_REGISTER_BASE + CSR_CONFIG_ROM + 3*4,
			 4, &guid_hi);
      if (res != 0) { perror("read guid_hi"); return -1; }

      uint64_t test_guid = (uint64_t)ntohl(guid_hi) << 32 | ntohl(guid_lo);
      if (test_guid == guid) { 
	target = test_node;
	goto target_found;
      }
    }
    return -1;
  target_found:
    ;
  }

  unsigned step = quadletwise ? 4 : 512;
  for (uint64_t cur = address; cur < address+length; cur += step) {
    quadlet_t buf[step/sizeof(quadlet_t)];
    size_t size = (cur + step > address+length) ? (address+length - cur) : step;
    int res = raw1394_read(fw_handle, target, cur, size, buf);
    if (res != 0) { perror("read data"); return -1; }
    write(STDOUT_FILENO, buf, size);
  }

  return 0;
}

/* EOF */
