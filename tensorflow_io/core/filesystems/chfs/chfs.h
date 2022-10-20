#ifndef TENSORFLOW_IO_CORE_FILESYTEM_CHFS_CHFS_FILESYSTEM_H_
#define TENSORFLOW_IO_CORE_FILESYTEM_CHFS_CHFS_FILESYSTEM_H_

#include "tensorflow/c/tf_status.h"
#include "tensorflow_io/core/filesystems/filesystem_plugins.h"

#include <string>
#include <vector>
#include <memory>
#include <cstring>

#include <chfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

enum FileMode { READ, WRITE, APPEND, READWRITE };

class CHFS {
  public:

    explicit CHFS(const char* server, TF_Status* status);

    void NewFile(const std::string path, FileMode mode, int flags, TF_Status* status);

    int CreateDir(const std::string path, bool is_dir, bool recursive, TF_Status* status);

    int Open(const std::string path, int flags, TF_Status* status);

    void Close(int fd, TF_Status* status);

    int Stat(const std::string path, struct stat *st, TF_Status* status);

    int IsDir(std::shared_ptr<struct stat> st);

    int IsFile(std::shared_ptr<struct stat> st);

    int ReadDir(const std::string path, std::vector<std::string>& child);

    ~CHFS();
};

#endif
