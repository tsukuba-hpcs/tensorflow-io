#include "tensorflow_io/core/filesystems/chfs/chfs.h"

// #undef NDEBUG
#include <cassert>

CHFS::CHFS(TF_Status* status, const std::string server) {
  TF_SetStatus(status, TF_OK, "");
  int rc;

  rc = chfs_init(server);
  if (rc) {
    TF_SetStatus(status, TF_INTERNAL, "Error initializing CHFS library");
    return;
  }
}

CHFS::~CHFS() {
  int rc;

  rc = chfs_term();
  assert(rc == 0);
}
