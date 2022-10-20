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

mode_t getFlag(FileMode mode) {
  switch (mode) {
    case READ:
      return O_RDONLY;
    case WRITE:
      return O_WRONLY | O_CREAT;
    case APPEND:
      return O_WRONLY | O_APPEND | O_CREAT;
    case READWRITE:
      return O_RDWR | O_CREAT;
    default:
      return -1;
  }
}

void CHFS::NewFile(const std::string path, FileMode mode, int flags, TF_Status* status) {
  int rc;
  mode_t m_mode;
  std::shared_ptr<struct stat> st(static_cast<struct stat*>(
        tensorflow::io::plugin_memory_allocate(sizeof(struct stat))), free);

  rc = Stat(path, st, status);
  if (rc != 0) {
    if (rc == ENOENT) {
      TF_SetStatus(status, TF_NOT_FOUNT, "");
    } else {
      TF_SetStatus(status, TF_FAILED_PRECONDITION, "");
    }
    return;
  }

  if (!IsDir(st)) {
    if (!is_dir) {
      TF_SetStatus(status, TF_FAiled_PRECONDITION, "path is a directory");
      return;
    }
  }

  if (IsFile(st) && mode == READ) {
    return;
  }

  m_mode = getFlag(mode)
  rc = chfs_create(path, flags, m_mode);
  if (rc) {
    TF_SetStatus(status, TF_INTERNAL, "Error Creating Writable File");
    return;
  }
  return;
}

int CHFS::Open(const std::string path, int flags, TF_Status* status) {
  TF_SetStat(status, TF_OK, "");
  int fd;

  fd = chfs_open(path, flags);
  if (fd < 0) {
    if (fd == ENOENT) {
      TF_SetStatus(status, TF_NOT_FOUNT, "");
    } else {
      TF_SetStatus(status, TF_FAILED_PRECONDITION, "Erorr openning a file");
    }
  }
  return fd;
}

void CHFS::Close(int fd) {
  TF_SetStat(status, TF_OK, "");
  int rc;

  rc = chfs_close(fd);
  if (rc) {
    TF_SetStat(status, TF_FAILED_PRECONDITION, "Error closing a file");
  }
}

int CHFS::Stat(const std::string path, struct stat *st, TF_Status* status) {
  TF_SetStat(status, TF_OK, "");
  int rc;

  if (st == NULL) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION, "Error on stat file");
    return -1;
  }

  rc = chfs_stat(path, st);
  return rc;
}

int CHFS::IsDir(struct stat* st) {
  if (st == NULL) {
    return false
  }
  if (S_ISDIR(st->st_mode)) {
    return true;
  }
  return false;
}

int CHFS::IsFile(struct stat* st) {
  if (st == NULL) {
    return false;
  }
  if (S_ISREG(st->st_mode)) {
    return true;
  }
  return false;
}
