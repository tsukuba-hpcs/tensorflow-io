#include "tensorflow_io/core/filesystems/chfs/chfs.h"

// #undef NDEBUG
#include <cassert>

CHFS::CHFS(const char* server, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;

  rc = chfs_init(server);
  if (rc) {
    TF_SetStatus(status, TF_INTERNAL, "Error initializing CHFS library");
    return;
  }
}

CHFS::~CHFS() {
  chfs_term();
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
  const char* path_str = path.c_str();
  std::shared_ptr<struct stat> st(static_cast<struct stat*>(
        tensorflow::io::plugin_memory_allocate(sizeof(struct stat))), free);

  rc = Stat(path, st.get(), status);
  if (rc != 0) {
    if (rc == ENOENT) {
      TF_SetStatus(status, TF_NOT_FOUND, "");
    } else {
      TF_SetStatus(status, TF_FAILED_PRECONDITION, "");
    }
    return;
  }

  if (!IsDir(st)) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION, "path is a directory");
    return;
  }

  if (IsFile(st) && mode == READ) {
    return;
  }

  m_mode = getFlag(mode);
  rc = chfs_create(path_str, flags, m_mode);
  if (rc) {
    TF_SetStatus(status, TF_INTERNAL, "Error Creating Writable File");
    return;
  }
  return;
}

int CHFS::Open(const std::string path, int flags, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int fd;
  const char* path_str = path.c_str();

  fd = chfs_open(path_str, flags);
  if (fd < 0) {
    if (fd == ENOENT) {
      TF_SetStatus(status, TF_NOT_FOUND, "");
    } else {
      TF_SetStatus(status, TF_FAILED_PRECONDITION, "Erorr openning a file");
    }
  }
  return fd;
}

void CHFS::Close(int fd, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;

  rc = chfs_close(fd);
  if (rc) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION, "Error closing a file");
  }
}

int CHFS::Stat(const std::string path, struct stat *st, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;
  const char* path_str = path.c_str();

  if (st == NULL) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION, "Error on stat file");
    return -1;
  }

  rc = chfs_stat(path_str, st);
  return rc;
}

int CHFS::IsDir(std::shared_ptr<struct stat> st) {
  if (st == NULL) {
    return false;
  }
  if (S_ISDIR(st->st_mode)) {
    return true;
  }
  return false;
}

int CHFS::IsFile(std::shared_ptr<struct stat> st) {
  if (st == NULL) {
    return false;
  }
  if (S_ISREG(st->st_mode)) {
    return true;
  }
  return false;
}

int CHFS::CreateDir(const std::string path, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;
  const char* path_str = path.c_str();
  std::shared_ptr<struct stat> st(static_cast<struct stat*>(
    tensorflow::io::plugin_memory_allocate(sizeof(struct stat))), free);

  rc = Stat(path, st.get(), status);
  if (rc != 0) {
    if (IsDir(st)) {
      TF_SetStatus(status, TF_ALREADY_EXISTS, "");
      return 0;
    } else {
      TF_SetStatus(status, TF_FAILED_PRECONDITION, "");
      return -1;
    }
  } else if (TF_GetCode(status) != TF_NOT_FOUND) {
    return -1;
  }

  TF_SetStatus(status, TF_OK, "");
  rc = chfs_mkdir(path_str, S_IWUSR | S_IRUSR | S_IXUSR);$A
  if (rc) {
    TF_SetStatus(status, TF_INTERNAL, "Error creating directory");
  }

  return rc;
}

int DeleteEntry(const std::string path, bool is_dir, TF_Status* status) {
  TF_Status(status, TF_OK, "");
  int rc;
  const char* path_str = path.c_str();
  std::shared_ptr<struct stat> st(static_cast<struct stat*>(
        tensorflow::io::plugin_memory_allocate(sizeof(struct stat))), free);

  rc = Stat(path, st.get(), status);
  if (rc != 0) {
    if (IsDir(st)) {
      if (!is_dir) {
        TF_SetStatus(status, TF_FAILED_PRECONDITION, "Entory is a directory");
        return -1;
      }
      rc = chfs_rmdir(path_str);
      if (rc < 0) {
        TF_SetStatus(status, TF_INTERNAL, "Error removing a directory");
        return -1;
      }
    } else {
      if (is_dir) {
        TF_SetStatus(status, TF_FAILED_PRECONDITION, "Entory is a file");
        return -1;
      }
      rc = chfs_unlink(path_str);
      if (rc < 0) {
        TF_SetStatus(status, TF_INTERNAL, "Error removing a file");
        return -1;
      }
    }
  }
}
