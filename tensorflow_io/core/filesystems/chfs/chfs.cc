#include "tensorflow_io/core/filesystems/chfs/chfs.h"

#include <dlfcn.h>

// #undef NDEBUG
#include <cassert>

CHFS::CHFS(const char* server, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;

  libchfs.reset(new libCHFS(status));
  if (TF_GetCode(status) != TF_OK) {
      libchfs.reset(nullptr);
      return;
  }

  rc = libchfs->chfs_init(server);
  if (rc) {
    TF_SetStatus(status, TF_INTERNAL, "Error initializing CHFS library");
    return;
  }
}

CHFS::~CHFS() {
  libchfs->chfs_term();

  libchfs.reset(nullptr);
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
      tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
      tensorflow::io::plugin_memory_free);

  rc = Stat(path, st, status);
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
  rc = libchfs->chfs_create(path_str, flags, m_mode);
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

  fd = libchfs->chfs_open(path_str, flags);
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

  rc = libchfs->chfs_close(fd);
  if (rc) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION, "Error closing a file");
  }
}

int CHFS::Stat(const std::string path, std::shared_ptr<struct stat> st, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;
  const char* path_str = path.c_str();

  if (st == NULL) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION, "Error on stat file");
    return -1;
  }

  struct stat* st_ptr = st.get();
  rc = libchfs->chfs_stat(path_str, st_ptr);
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
      tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
      tensorflow::io::plugin_memory_free);

  rc = Stat(path, st, status);
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
  rc = libchfs->chfs_mkdir(path_str, S_IWUSR | S_IRUSR | S_IXUSR);
  if (rc) {
    TF_SetStatus(status, TF_INTERNAL, "Error creating directory");
  }

  return rc;
}

int CHFS::DeleteEntry(const std::string path, bool is_dir, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;
  const char* path_str = path.c_str();
  std::shared_ptr<struct stat> st(static_cast<struct stat*>(
      tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
      tensorflow::io::plugin_memory_free);

  rc = Stat(path, st, status);
  if (rc != 0) {
    if (IsDir(st)) {
      if (!is_dir) {
        TF_SetStatus(status, TF_FAILED_PRECONDITION, "Entory is a directory");
        return -1;
      }
      rc = libchfs->chfs_rmdir(path_str);
      if (rc < 0) {
        TF_SetStatus(status, TF_INTERNAL, "Error removing a directory");
        return -1;
      }
    } else {
      if (is_dir) {
        TF_SetStatus(status, TF_FAILED_PRECONDITION, "Entory is a file");
        return -1;
      }
      rc = libchfs->chfs_unlink(path_str);
      if (rc < 0) {
        TF_SetStatus(status, TF_INTERNAL, "Error removing a file");
        return -1;
      }
    }
  }
}

static void* LoadSharedLibrary(const char* library_filename,
                               TF_Status* status) {
    std::vector<std::string> libdirs{"/usr/lib64", "/usr/local/lib64", "/opt/chfs/lib64"};
    char* libdir;
    void* handle;

    if ((libdir = std::getenv("TF_IO_CHFS_LIBRARY_DIR")) != nullptr) {
        libdirs.push_back(libdir);
    }

    for (auto itr = libdirs.rbegin(), e = libdirs.rend(); itr != e; ++itr) {
        std::string path = *itr;
        if (path.back() != '/')
            path.push_back('/');
        path.append(library_filename);
        handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle != nullptr) {
            TF_SetStatus(status, TF_OK, "");
            return handle;
        }
    }

    std::string error_message =
        absl::StrCat("Library (", library_filename, ") not found: ", dlerror());
    TF_SetStatus(status, TF_NOT_FOUND, error_message.c_str());
    return nullptr;
}

static void* GetSymbolFromLibrary(void* handle, const char* symbol_name,
                                  TF_Status* status) {
  if (handle == nullptr) {
    TF_SetStatus(status, TF_INVALID_ARGUMENT, "library handle cannot be null");
    return nullptr;
  }
  void* symbol = dlsym(handle, symbol_name);
  if (symbol == nullptr) {
    std::string error_message =
        absl::StrCat("Symbol (", symbol_name, ") not found: ", dlerror());
    TF_SetStatus(status, TF_NOT_FOUND, error_message.c_str());
    return nullptr;
  }

  TF_SetStatus(status, TF_OK, "");
  return symbol;
}

template <typename R, typename... Args>
void BindFunc(void* handle, const char* name, std::function<R(Args...)>* func,
              TF_Status* status) {
  *func = reinterpret_cast<R (*)(Args...)>(
      GetSymbolFromLibrary(handle, name, status));
}

libCHFS::~libCHFS() {
    if (libchfs_handle_ != nullptr) {
        dlclose(libchfs_handle_);
    }
}

void libCHFS::LoadAndBindCHFSLibs(TF_Status* status) {
    libchfs_handle_ = LoadSharedLibrary("libchfs.so", status);
    if (TF_GetCode(status) != TF_OK)
        return;

#define BIND_CHFS_FUNC(handle, function)                \
    do {                                                \
        BindFunc(handle, #function, &function, status); \
        if (TF_GetCode(status) != TF_OK) return;        \
    } while (0);

    BIND_CHFS_FUNC(libchfs_handle_, chfs_init);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_term);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_create);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_open);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_close);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_pread);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_pwrite);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_seek);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_unlink);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_mkdir);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_rmdir);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_stat);
    BIND_CHFS_FUNC(libchfs_handle_, chfs_readdir);

#undef BIND_CHFS_FUNC
}
