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
      return (O_RDONLY);
    case WRITE:
      return (O_WRONLY | O_CREAT);
    case APPEND:
      return (O_WRONLY | O_APPEND | O_CREAT);
    case READWRITE:
      return (O_RDWR | O_CREAT);
    default:
      return -1;
  }
}

void CopyEntries(char*** entries, std::vector<std::string>& results) {
  *entries = static_cast<char**>(
      tensorflow::io::plugin_memory_allocate(results.size() * sizeof(char*)));

  for (uint32_t i = 0; i < results.size(); i++) {
    (*entries)[i] = static_cast<char*>(tensorflow::io::plugin_memory_allocate(
        results[i].size() * sizeof(char)));
    if (results[i][0] == '/') results[i].erase(0, 1);
    strcpy((*entries)[i], results[i].c_str());
  }
}

const std::string GetPath(const std::string& path) {
  auto pos = path.find("://");
  if (pos != std::string::npos)
      return path.substr(pos + 3);
  return path;
}

const std::string GetParent(const std::string& path) {
  std::filesystem::path p = path;
  return p.parent_path();
}

int CHFS::NewFile(const std::string path, FileMode mode, int32_t flags,
                  TF_Status* status) {
  int rc, fd;
  mode_t m_mode;
  const std::string cpath = GetPath(path);
  std::shared_ptr<struct stat> st(
      static_cast<struct stat*>(
          tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
      tensorflow::io::plugin_memory_free);

  rc = Stat(cpath, st, status);
  if (rc != 0 && errno != ENOENT) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION, "Error stating the path");
    return -1;
  }

  if (IsDir(st)) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION, "Path is a directory");
    return -1;
  }

  if (IsFile(st) && mode == READ) {
    fd = Open(path, flags, status);
    if (fd < 0) {
      TF_SetStatus(status, TF_INTERNAL, "Error opening a file");
      return -1;
    }
    TF_SetStatus(status, TF_OK, "");
    return fd;
  }

  const std::string parent = GetParent(cpath);
  st.reset(static_cast<struct stat*>(
               tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
           tensorflow::io::plugin_memory_free);
  rc = Stat(parent, st, status);
  if (rc != 0) {
    TF_SetStatus(status, TF_INTERNAL, "Cannot retrieve mode from parent");
    return -1;
  }
  if (st != nullptr && !IsDir(st)) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION, "parent is not a directory");
    return -1;
  }

  m_mode = getFlag(mode);
  fd = libchfs->chfs_create(cpath.c_str(), m_mode, flags);
  if (fd < 0) {
    TF_SetStatus(status, TF_INTERNAL, "Error creating writable file");
    return -1;
  }
  return fd;
}

int CHFS::Open(const std::string path, int32_t flags, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int fd;
  std::string cpath = GetPath(path);

  fd = libchfs->chfs_open(cpath.c_str(), flags);
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

int CHFS::Stat(const std::string path, std::shared_ptr<struct stat>& st,
               TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;
  std::string cpath = GetPath(path);

  if (st == NULL) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION, "Error on stat file");
    return -1;
  }

  struct stat* st_ptr = st.get();
  rc = libchfs->chfs_stat(cpath.c_str(), st_ptr);
  return rc;
}

int CHFS::IsDir(std::shared_ptr<struct stat>& st) {
  if (st == nullptr) {
    return false;
  }
  if (S_ISDIR(st->st_mode)) {
    return true;
  }
  return false;
}

int CHFS::IsFile(std::shared_ptr<struct stat>& st) {
  if (st == NULL) {
    return false;
  }
  if (S_ISREG(st->st_mode)) {
    return true;
  }
  return false;
}

static std::vector<std::string> readdir_entries;

static int readdirFiller(void *buf, const char *name, const struct stat *st, off_t off) {
    if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
        return 0;

    std::string filename(name);
    readdir_entries.emplace_back(filename);
    return 0;
}

int CHFS::ReadDir(const std::string path, std::vector<std::string>& child) {
    int rc = 0;
    char **buffer;
    std::string cpath = GetPath(path);

    rc = libchfs->chfs_readdir(cpath.c_str(), NULL, readdirFiller);

    for (auto &entry : readdir_entries) {
        child.emplace_back(entry);
    }
    return rc;
}

int CHFS::CreateDir(const std::string path, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;
  const std::string cpath = GetPath(path);
  std::shared_ptr<struct stat> st(
      static_cast<struct stat*>(
          tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
      tensorflow::io::plugin_memory_free);

  rc = Stat(cpath, st, status);
  if (rc != 0) {
    if (errno != ENOENT) {
      TF_SetStatus(status, TF_FAILED_PRECONDITION, strerror(errno));
      return -1;
    }
  }
  if (IsDir(st)) {
    TF_SetStatus(status, TF_ALREADY_EXISTS, "");
    return 0;
  }

  TF_SetStatus(status, TF_OK, "");
  rc = libchfs->chfs_mkdir(cpath.c_str(), S_IWUSR | S_IRUSR | S_IXUSR);
  if (rc) {
    TF_SetStatus(status, TF_INTERNAL, "Error creating directory");
  }

  return rc;
}

int CHFS::DeleteEntry(const std::string path, bool is_dir, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;
  const std::string cpath = GetPath(path);
  std::shared_ptr<struct stat> st(
      static_cast<struct stat*>(
          tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
      tensorflow::io::plugin_memory_free);

  rc = Stat(cpath, st, status);
  if (rc != 0) {
    if (errno == ENOENT)
      TF_SetStatus(status, TF_NOT_FOUND, "");
    else if (TF_GetCode(status) != TF_FAILED_PRECONDITION)
      TF_SetStatus(status, TF_INTERNAL, "");
    return -1;
  }
  if (IsDir(st)) {
    if (!is_dir) {
      TF_SetStatus(status, TF_FAILED_PRECONDITION, "Entory is a directory");
      return -1;
    }
    rc = libchfs->chfs_rmdir(cpath.c_str());
    if (rc != 0) {
      TF_SetStatus(status, TF_INTERNAL, "Error removing a directory");
      return -1;
    }
  } else {
    if (is_dir) {
      TF_SetStatus(status, TF_FAILED_PRECONDITION, "Entory is a file");
      return -1;
    }
    rc = libchfs->chfs_unlink(cpath.c_str());
    if (rc != 0) {
      TF_SetStatus(status, TF_INTERNAL, "Error removing a file");
      return -1;
    }
  }
  return 0;
}

static void* LoadSharedLibrary(const char* library_filename,
                               TF_Status* status) {
  std::vector<std::string> libdirs{"/usr/lib64", "/usr/local/lib64",
                                   "/opt/chfs/lib64"};
  char* libdir;
  void* handle;

  if ((libdir = std::getenv("TF_IO_CHFS_LIBRARY_DIR")) != nullptr) {
    libdirs.push_back(libdir);
  }

  for (auto itr = libdirs.rbegin(), e = libdirs.rend(); itr != e; ++itr) {
    std::string path = *itr;
    if (path.back() != '/') path.push_back('/');
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

void BindFunc(void* handle, const char* name,
        std::function<int(const char *, void *,
        int (*)(void *, const char *, const struct stat *, off_t))>* func,
        TF_Status* status) {
  *func = reinterpret_cast<int (*)(const char*, void*,
          int(*)(void *, const char *, const struct stat *, off_t))
      >(GetSymbolFromLibrary(handle, name, status));
}

libCHFS::~libCHFS() {
  if (libchfs_handle_ != nullptr) {
    dlclose(libchfs_handle_);
  }
}

void libCHFS::LoadAndBindCHFSLibs(TF_Status* status) {
  libchfs_handle_ = LoadSharedLibrary("libchfs.so", status);
  if (TF_GetCode(status) != TF_OK) return;

#define BIND_CHFS_FUNC(handle, function)            \
  do {                                              \
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
