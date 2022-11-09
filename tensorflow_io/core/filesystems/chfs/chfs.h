#ifndef TENSORFLOW_IO_CORE_FILESYTEM_CHFS_CHFS_H_
#define TENSORFLOW_IO_CORE_FILESYTEM_CHFS_CHFS_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
// clang-format off
#include <chfs.h>
// clang-format on

#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "tensorflow/c/tf_status.h"
#include "tensorflow_io/core/filesystems/filesystem_plugins.h"

enum FileMode { READ, WRITE, APPEND, READWRITE };

class libCHFS;

class CHFS {
 public:
  std::unique_ptr<libCHFS> libchfs;

  // std::string GetPath(const std::string& path);
  // std::string GetParent(const std::string& path);

  explicit CHFS(const char* server, TF_Status* status);

  const std::string GetPath(const std::string& string);

  const std::string GetParent(const std::string& string);

  int NewFile(const std::string path, FileMode mode, int flags,
              TF_Status* status);

  int CreateDir(const std::string path, TF_Status* status);

  int Open(const std::string path, int flags, TF_Status* status);

  void Close(int fd, TF_Status* status);

  int Stat(const std::string path, std::shared_ptr<struct stat>& st,
           TF_Status* status);

  int IsDir(std::shared_ptr<struct stat>& st);

  int IsFile(std::shared_ptr<struct stat>& st);

  int ReadDir(const std::string path, std::vector<std::string>& child);

  int DeleteEntry(const std::string path, bool is_dir, TF_Status* status);

  ~CHFS();
};

using Filler = std::function<int(void*, const char*, const struct stat*, off_t)>;

class libCHFS {
 public:
  explicit libCHFS(TF_Status* status) { LoadAndBindCHFSLibs(status); }

  ~libCHFS();

  std::function<int(const char*)> chfs_init;
  std::function<int(void)> chfs_term;
  std::function<int(const char*, int32_t, mode_t)> chfs_create;
  std::function<int(const char*, int32_t flags)> chfs_open;
  std::function<int(int)> chfs_close;
  std::function<int(int, void*, size_t, off_t)> chfs_pread;
  std::function<int(int, const void*, size_t, off_t)> chfs_pwrite;
  std::function<int(int, off_t, int)> chfs_seek;
  std::function<int(const char*)> chfs_unlink;
  std::function<int(const char*, mode_t)> chfs_mkdir;
  std::function<int(const char*)> chfs_rmdir;
  std::function<int(const char*, struct stat*)> chfs_stat;
  std::function<int(const char *path, void *buf,
          int (*)(void *, const char *, const struct stat *, off_t))> chfs_readdir;
  // std::function<int(const char*, void*, Filler*)> chfs_reddir;

 private:
  void LoadAndBindCHFSLibs(TF_Status* status);
  void* libchfs_handle_;
};

#endif
