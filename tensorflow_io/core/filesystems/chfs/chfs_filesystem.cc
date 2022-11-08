#include "tensorflow_io/core/filesystems/chfs/chfs.h"
// #undef NDEBUG
#include <cassert>

#include <iostream>

namespace tensorflow {
namespace io {
namespace chfs {

// Implementation for `TF_RandomAccessFile`
//
namespace tf_random_access_file {
typedef struct CHFSRandomAccessFile {
  CHFS* chfs;
  std::string path;
  size_t file_size;
  int fd;

  CHFSRandomAccessFile(CHFS* chfs, std::string path, int fd)
      : chfs(chfs), fd(fd) {
    path = chfs->GetPath(path);
    std::shared_ptr<struct stat> st(
        static_cast<struct stat*>(
            tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
        tensorflow::io::plugin_memory_free);
    chfs->libchfs->chfs_stat(path.c_str(), st.get());
    file_size = static_cast<size_t>(st->st_size);
  }

  int64_t Read(uint64_t offset, size_t n, char* buffer, TF_Status* status) {
    ssize_t read_size;

    read_size = chfs->libchfs->chfs_pread(fd, buffer, n, offset);
    if (read_size < 0) {
      TF_SetStatus(status, TF_INTERNAL, "Error reading");
      return read_size;
    }

    if (static_cast<size_t>(read_size) != n) {
      TF_SetStatus(status, TF_OUT_OF_RANGE, "");
      return read_size;
    }

    TF_SetStatus(status, TF_OK, "");
    return read_size;
  }
} CHFSRandomAccessFile;

void Cleanup(TF_RandomAccessFile* file) {
  auto chfs_file = static_cast<CHFSRandomAccessFile*>(file->plugin_file);
  chfs_file->chfs = nullptr;
  delete chfs_file;
}

int64_t Read(const TF_RandomAccessFile* file, uint64_t offset, size_t n,
             char* ret, TF_Status* status) {
  auto chfs_file = static_cast<CHFSRandomAccessFile*>(file->plugin_file);
  if (offset > chfs_file->file_size) {
    TF_SetStatus(status, TF_OUT_OF_RANGE, "");
    return -1;
  }

  size_t ret_offset = 0;
  size_t cur_offset = offset;
  int64_t total_bytes = 0;
  while (cur_offset < chfs_file->file_size) {
    int64_t read_bytes = 0;

    read_bytes = chfs_file->Read(cur_offset, n + ret_offset, ret, status);
    if (read_bytes > 0) {
      ret_offset += read_bytes;
      cur_offset += read_bytes;
      total_bytes += read_bytes;
      n -= read_bytes;
      continue;
    }
  }
  return total_bytes;
}
}  // namespace tf_random_access_file

// Implementation for `TF_WritableFile`
//
namespace tf_writable_file {
typedef struct CHFSWritableFile {
  CHFS* chfs;
  std::string path;
  int fd;
  size_t file_size;
  bool size_known;

  CHFSWritableFile(CHFS* chfs, std::string path, int fd)
      : chfs(chfs), fd(fd) {
    path = chfs->GetPath(path);
    size_known = false;
    size_t dummy;
    get_file_size(dummy);
  }

  int get_file_size(size_t& size) {
    int rc;

    if (!size_known) {
      std::shared_ptr<struct stat> st(
          static_cast<struct stat*>(
              tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
          tensorflow::io::plugin_memory_free);
      rc = chfs->libchfs->chfs_stat(path.c_str(), st.get());
      if (rc) {
        return rc;
      }
      file_size = static_cast<size_t>(st->st_size);
      size_known = true;
    }
    size = file_size;
    return 0;
  }

  void set_file_size(size_t size) {
    file_size = size;
    size_known = true;
  }

  void unset_file_size() { size_known = false; }
} CHFSWritableFile;

void Cleanup(TF_WritableFile* file) {
  auto chfs_file = static_cast<CHFSWritableFile*>(file->plugin_file);
  chfs_file->chfs = nullptr;
  delete chfs_file;
}

void Append(const TF_WritableFile* file, const char* buffer, size_t n,
            TF_Status* status) {
  int rc;
  ssize_t written_bytes;
  size_t cur_file_size;
  auto chfs_file = static_cast<CHFSWritableFile*>(file->plugin_file);

  rc = chfs_file->get_file_size(cur_file_size);
  if (rc != 0) {
    TF_SetStatus(status, TF_INTERNAL, "Cannot determine file size");
    return;
  }

  written_bytes = chfs_file->chfs->libchfs->chfs_pwrite(chfs_file->fd, buffer, n,
                                             cur_file_size);
  if (written_bytes < 0) {
    TF_SetStatus(status, TF_RESOURCE_EXHAUSTED, strerror(errno));
    chfs_file->unset_file_size();
    return;
  }

  chfs_file->set_file_size(cur_file_size + written_bytes);
  TF_SetStatus(status, TF_OK, "");
}

int64_t Tell(const TF_WritableFile* file, TF_Status* status) {
  off_t cur_position;
  auto chfs_file = static_cast<CHFSWritableFile*>(file->plugin_file);

  cur_position =
      chfs_file->chfs->libchfs->chfs_seek(chfs_file->fd, 0, SEEK_CUR);

  TF_SetStatus(status, TF_OK, "");
  return cur_position;
}

void Close(const TF_WritableFile* file, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  auto chfs_file = static_cast<CHFSWritableFile*>(file->plugin_file);
  chfs_file->chfs->Close(chfs_file->fd, status);
}

}  // namespace tf_writable_file

// Implementation for `TF_ReadOnlyMemoryRegion`
//
namespace tf_read_only_memory_region {
void Cleanup(TF_ReadOnlyMemoryRegion* region) {}

const void* Data(const TF_ReadOnlyMemoryRegion* region) { return nullptr; }

uint64_t Length(const TF_ReadOnlyMemoryRegion* region) { return 0; }
}  // namespace tf_read_only_memory_region

// Implementation for `TF_Filesystem`
//
namespace tf_chfs_filesystem {

void atexit_handler(void);  // forward declaration

static TF_Filesystem* chfs_filesystem;

void Init(TF_Filesystem* filesystem, TF_Status* status) {
  const char* server = std::getenv("CHFS_SERVER");
  filesystem->plugin_filesystem = new (std::nothrow) CHFS(server, status);

  if (TF_GetCode(status) == TF_OK) {
    chfs_filesystem = filesystem;
    // explicitly calling Clear from atexist handlerhandlerhandlerhandler
    // see: https://github.com/tensorflow/tensorflow/issues/27535
    std::atexit(atexit_handler);
  }
}

void Cleanup(TF_Filesystem* filesystem) {
  auto chfs = static_cast<CHFS*>(filesystem->plugin_filesystem);
  delete chfs;
}

void atexit_handler(void) {
  // terminate chfs_filesystem
  Cleanup(chfs_filesystem);
}

void NewWritableFile(const TF_Filesystem* filesystem, const char* path,
                     TF_WritableFile* file, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  auto chfs = static_cast<CHFS*>(filesystem->plugin_filesystem);
  int32_t flags = S_IRUSR | S_IWUSR | S_IFREG;
  int fd;

  fd = chfs->NewFile(path, WRITE, flags, status);
  if (TF_GetCode(status) != TF_OK) return;

  file->plugin_file = new tf_writable_file::CHFSWritableFile(chfs, path, fd);
}

void NewRandomAccessFile(const TF_Filesystem* filesystem, const char* path,
                         TF_RandomAccessFile* file, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  auto chfs = static_cast<CHFS*>(filesystem->plugin_filesystem);
  int32_t flags = S_IRUSR | S_IFREG;
  int fd;

  fd = chfs->NewFile(path, READ, flags, status);
  if (TF_GetCode(status) != TF_OK) return;

  file->plugin_file =
      new tf_random_access_file::CHFSRandomAccessFile(chfs, path, fd);
}

void NewAppendableFile(const TF_Filesystem* filesystem, const char* path,
                       TF_WritableFile* file, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  auto chfs = static_cast<CHFS*>(filesystem->plugin_filesystem);
  int32_t flags = S_IRUSR | S_IWUSR | S_IFREG;
  int fd;

  fd = chfs->NewFile(path, APPEND, flags, status);
  if (TF_GetCode(status) != TF_OK) return;

  file->plugin_file = new tf_writable_file::CHFSWritableFile(chfs, path, fd);
}

static void CreateDir(const TF_Filesystem* filesystem, const char* path,
                      TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  auto chfs = static_cast<CHFS*>(filesystem->plugin_filesystem);

  chfs->CreateDir(path, status);
}

static void RecursivelyCreateDir(const TF_Filesystem* filesystem,
                                 const char* path, TF_Status* status) {
  // unimplemented
}

static void DeleteFile(const TF_Filesystem* filesystem, const char* path,
                       TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  auto chfs = static_cast<CHFS*>(filesystem->plugin_filesystem);
  bool is_dir = false;

  chfs->DeleteEntry(path, is_dir, status);
}

static void DeleteDir(const TF_Filesystem* filesystem, const char* path,
                      TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  auto chfs = static_cast<CHFS*>(filesystem->plugin_filesystem);
  bool is_dir = true;

  chfs->DeleteEntry(path, is_dir, status);
}

static void DeleteRecursively(const TF_Filesystem* filesystem, const char* path,
                              uint64_t* undeleted_files,
                              uint64_t* undeleted_dirs, TF_Status* status) {
  // unimplemented
}

static void RenameFile(const TF_Filesystem* filesystem, const char* src,
                       const char* dst, TF_Status* status) {
  // unimplemented
}

static void PathExists(const TF_Filesystem* filesystem, const char* path,
                       TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;
  std::shared_ptr<struct stat> st(
      static_cast<struct stat*>(
          tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
      tensorflow::io::plugin_memory_free);
  auto chfs = static_cast<CHFS*>(filesystem->plugin_filesystem);

  rc = chfs->Stat(path, st, status);
  if (rc) {
    if (TF_GetCode(status) == TF_OK && errno == ENOENT) {
      TF_SetStatus(status, TF_NOT_FOUND, "");
      return;
    }
    TF_SetStatus(status, TF_FAILED_PRECONDITION, strerror(errno));
  }
}

static void Stat(const TF_Filesystem* filesystem, const char* path,
                 TF_FileStatistics* stats, TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;
  std::shared_ptr<struct stat> st(
      static_cast<struct stat*>(
          tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
      tensorflow::io::plugin_memory_free);
  auto chfs = static_cast<CHFS*>(filesystem->plugin_filesystem);

  rc = chfs->Stat(path, st, status);
  if (rc) {
    TF_SetStatus(status, TF_INTERNAL, strerror(errno));
    return;
  }

  stats->length = st->st_size;
  stats->mtime_nsec = static_cast<int64_t>(st->st_mtime) * 1e9;
  if (chfs->IsDir(st)) {
    stats->is_directory = true;
  } else {
    stats->is_directory = false;
  }
}

static bool IsDir(const TF_Filesystem* filesystem, const char* path,
                  TF_Status* status) {
  TF_SetStatus(status, TF_OK, "");
  int rc;
  std::shared_ptr<struct stat> st(
      static_cast<struct stat*>(
          tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
      tensorflow::io::plugin_memory_free);
  auto chfs = static_cast<CHFS*>(filesystem->plugin_filesystem);

  rc = chfs->Stat(path, st, status);
  if (rc != 0 && errno == ENOENT) {
    TF_SetStatus(status, TF_NOT_FOUND, "");
    return false;
  }

  if (chfs->IsDir(st)) {
    return true;
  }
  return false;
}

static int64_t GetFileSize(const TF_Filesystem* filesystem, const char* path,
                           TF_Status* status) {
  size_t file_size;
  int rc;
  std::shared_ptr<struct stat> st(
      static_cast<struct stat*>(
          tensorflow::io::plugin_memory_allocate(sizeof(struct stat))),
      tensorflow::io::plugin_memory_free);
  auto chfs = static_cast<CHFS*>(filesystem->plugin_filesystem);

  rc = chfs->Stat(path, st, status);
  if (rc) {
    return rc;
  }
  if (chfs->IsDir(st)) {
    TF_SetStatus(status, TF_FAILED_PRECONDITION, "");
    return -1;
  }
  file_size = static_cast<size_t>(st->st_size);
  return file_size;
}

static char* TranslateName(const TF_Filesystem* filesystem, const char* uri) {
  return strdup(uri);
}

static int GetChildren(const TF_Filesystem* filesystem, const char* path,
                       char*** entries, TF_Status* status) {
  return 0;
}

}  // namespace tf_chfs_filesystem

void ProvideFilesystemSupportFor(TF_FilesystemPluginOps* ops, const char* uri) {
  TF_SetFilesystemVersionMetadata(ops);
  ops->scheme = strdup(uri);

  ops->random_access_file_ops = static_cast<TF_RandomAccessFileOps*>(
      plugin_memory_allocate(TF_RANDOM_ACCESS_FILE_OPS_SIZE));
  ops->random_access_file_ops->cleanup = tf_random_access_file::Cleanup;
  ops->random_access_file_ops->read = tf_random_access_file::Read;

  ops->writable_file_ops = static_cast<TF_WritableFileOps*>(
      plugin_memory_allocate(TF_WRITABLE_FILE_OPS_SIZE));
  ops->writable_file_ops->cleanup = tf_writable_file::Cleanup;
  ops->writable_file_ops->append = tf_writable_file::Append;
  ops->writable_file_ops->tell = tf_writable_file::Tell;
  ops->writable_file_ops->close = tf_writable_file::Close;

  ops->read_only_memory_region_ops = static_cast<TF_ReadOnlyMemoryRegionOps*>(
      plugin_memory_allocate(TF_READ_ONLY_MEMORY_REGION_OPS_SIZE));
  ops->read_only_memory_region_ops->cleanup =
      tf_read_only_memory_region::Cleanup;
  ops->read_only_memory_region_ops->data = tf_read_only_memory_region::Data;
  ops->read_only_memory_region_ops->length = tf_read_only_memory_region::Length;

  ops->filesystem_ops = static_cast<TF_FilesystemOps*>(
      plugin_memory_allocate(TF_FILESYSTEM_OPS_SIZE));
  ops->filesystem_ops->init = tf_chfs_filesystem::Init;
  ops->filesystem_ops->cleanup = tf_chfs_filesystem::Cleanup;
  ops->filesystem_ops->new_random_access_file =
      tf_chfs_filesystem::NewRandomAccessFile;
  ops->filesystem_ops->new_writable_file = tf_chfs_filesystem::NewWritableFile;
  ops->filesystem_ops->new_appendable_file =
      tf_chfs_filesystem::NewAppendableFile;
  ops->filesystem_ops->path_exists = tf_chfs_filesystem::PathExists;
  ops->filesystem_ops->create_dir = tf_chfs_filesystem::CreateDir;
  ops->filesystem_ops->delete_dir = tf_chfs_filesystem::DeleteDir;
  ops->filesystem_ops->recursively_create_dir =
      tf_chfs_filesystem::RecursivelyCreateDir;
  ops->filesystem_ops->is_directory = tf_chfs_filesystem::IsDir;
  ops->filesystem_ops->delete_recursively =
      tf_chfs_filesystem::DeleteRecursively;
  ops->filesystem_ops->get_file_size = tf_chfs_filesystem::GetFileSize;
  ops->filesystem_ops->delete_file = tf_chfs_filesystem::DeleteFile;
  ops->filesystem_ops->rename_file = tf_chfs_filesystem::RenameFile;
  ops->filesystem_ops->stat = tf_chfs_filesystem::Stat;
  ops->filesystem_ops->get_children = tf_chfs_filesystem::GetChildren;
  ops->filesystem_ops->translate_name = tf_chfs_filesystem::TranslateName;
}

}  // namespace chfs
}  // namespace io
}  // namespace tensorflow
