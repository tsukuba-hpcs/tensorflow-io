

namespace tensorflow{
namespace io {
namespace chfs {

// Implementation for `TF_RandomAccessFile`
//
namespace tf_random_access_file {
} // namespace tf_random_access_file

// Implementation for `TF_WritableFile`
//
namespace tf_writable_file {
} // namespace tf_writable_file

// Implementation for `TF_ReadOnlyMemoryRegion`
//
namespace tf_read_only_memory_region {
} // namespace tf_read_only_memory_region

// Implementation for `TF_Filesystem`
//
namespace tf_chfs_filesystem {
} // namespace tf_chfs_filesystem

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
  ops->writable_file_ops->flush = tf_writable_file::Flush;
  ops->writable_file_ops->sync = tf_writable_file::Sync;
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
  ops->filesystem_ops->new_writable_file = tf_dfs_filesystem::NewWritableFile;
  ops->filesystem_ops->new_appendable_file =
      tf_chfs_filesystem::NewAppendableFile;
  ops->filesystem_ops->path_exists = tf_chfs_filesystem::PathExists;
  ops->filesystem_ops->create_dir = tf_chfs_filesystem::CreateDir;
  ops->filesystem_ops->delete_dir = tf_chfs_filesystem::DeleteSingleDir;
  ops->filesystem_ops->recursively_create_dir =
      tf_chfs_filesystem::RecursivelyCreateDir;
  ops->filesystem_ops->is_directory = tf_chfs_filesystem::IsDir;
  ops->filesystem_ops->delete_recursively =
      tf_chfs_filesystem::RecursivelyDeleteDir;
  ops->filesystem_ops->get_file_size = tf_chfs_filesystem::GetFileSize;
  ops->filesystem_ops->delete_file = tf_chfs_filesystem::DeleteFile;
  ops->filesystem_ops->rename_file = tf_chfs_filesystem::RenameFile;
  ops->filesystem_ops->stat = tf_chfs_filesystem::Stat;
  ops->filesystem_ops->get_children = tf_chfs_filesystem::GetChildren;
  ops->filesystem_ops->translate_name = tf_chfs_filesystem::TranslateName;
  ops->filesystem_ops->flush_caches = tf_chfs_filesystem::FlushCaches;
}

} // namespace chfs
} // namespace io
} // namespace tensorflow
