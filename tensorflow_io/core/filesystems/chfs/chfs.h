#ifndef TENSORFLOW_IO_CORE_FILESYTEM_CHFS_CHFS_FILESYSTEM_H_
#define TENSORFLOW_IO_CORE_FILESYTEM_CHFS_CHFS_FILESYSTEM_H_

#include "tensorflow/c/tf_status.h"
#include "tensorflow_io/core/filesystems/filesystem_plugins.h"

#include <string>
#include <vector>

#include <chfs.h>
#include <sys/stat.h>

enum FileMode { READ, WRITE, APPEND, READWRITE };

class CHFS {
  public:

    explicit CHFS(TF_Status* status, const std::string server);

    int NewFile(const std::string path, FileMode mode, int flags, TF_Status* status);

    int CreateDir(const std::string path, bool is_dir, bool recursive, TF_Status* status);

    int Stat(const std::string path, struct stat *st, TF_Status* status);

    int IsDir(struct stat *st);

    int IsFile(struct stat *st);

    int ReadDir(const std::string path, std::vector<std::string>& child);

    ~CHFS();
};

#endif
