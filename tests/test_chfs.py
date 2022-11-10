"""
Tests for Tensorflow-IO CHFS Plugin
"""
import os
import sys
import pytest

import tensorflow as tf
import tensorflow_io as tfio  # pylint: disable=unused-import

if sys.platform in ["darwin", "win32"]:
    pytest.skip("Incompatible", allow_module_level=True)


class CHFSTest(tf.test.TestCase):
    """Test Class for CHFS"""

    def __init__(self, methodName="runTest"):  # pylint: disable=invalid-name
        self.server = os.environ["CHFS_SERVER"]
        self.path_root = "chfs:///"
        super().__init__(methodName)

    def _path_to(self, name):
        return os.path.join(self.path_root, name)

    def test_create_file(self):
        """Test create file"""
        # check the precondition
        file_name = self._path_to("testfile")
        if tf.io.gfile.exists(file_name):
            tf.io.gfile.rmtree(file_name)
        # create file
        with tf.io.gfile.GFile(file_name, "w") as write_file:
            write_file.write("")
        # check that file was created
        self.assertTrue(tf.io.gfile.exists(file_name))

        tf.io.gfile.remove(file_name)
        self.assertTrue(not tf.io.gfile.exists(file_name))

    def test_write_read_file(self):
        """Test write/read file"""
        file_name = self._path_to("test_write_read")
        if tf.io.gfile.exists(file_name):
            tf.io.gfile.remove(file_name)

        with tf.io.gfile.GFile(file_name, "w") as write_file:
            write_file.write("Hello,\nworld!")

        with tf.io.gfile.GFile(file_name, "r") as read_file:
            data = read_file.read()
            self.assertEqual(data, "Hello,\nworld!")

        tf.io.gfile.remove(file_name)
        self.assertTrue(not tf.io.gfile.exists(file_name))

    def test_listdir(self):
        dir_name = self._path_to("listdir")
        if tf.io.gfile.exists(dir_name):
            # TODO: implement Recursive deleting
            if tf.io.gfile.exists(os.path.join(dir_name, "test1.txt")):
                tf.io.gfile.remove(os.path.join(dir_name, "test1.txt"))
            if tf.io.gfile.exists(os.path.join(dir_name, "test2.txt")):
                tf.io.gfile.remove(os.path.join(dir_name, "test2.txt"))
            if tf.io.gfile.exists(os.path.join(dir_name, "test_dir")):
                tf.io.gfile.rmtree(os.path.join(dir_name, "test_dir"))
            tf.io.gfile.rmtree(dir_name)
        tf.io.gfile.mkdir(dir_name)
        self.assertTrue(tf.io.gfile.exists(dir_name))
        # create files and a directory
        entries = ["test1.txt", "test2.txt"]
        inner_name = os.path.join(dir_name, "test_dir")
        if tf.io.gfile.exists(inner_name):
            tf.io.gfile.rmtree(inner_name)
        tf.io.gfile.mkdir(inner_name)
        self.assertTrue(tf.io.gfile.exists(inner_name))

        for entry in entries:
            file_path = self._path_to(f"listdir/{entry}")
            with tf.io.gfile.GFile(file_path, "w") as write_file:
                write_file.write("")
        entries.append("test_dir")

        results = tf.io.gfile.listdir(dir_name)

        entries.sort()
        results.sort()
        print("want:\t", entries)
        print("got:\t", results)
        self.assertTrue(entries == results)

    def test_is_directory(self):
        """Test is directory."""
        # Setup and check preconditions.
        parent = self._path_to("isdir")
        dir_name = self._path_to("isdir/1")
        file_name = self._path_to("7.txt")
        tf.io.gfile.mkdir(parent)
        with tf.io.gfile.GFile(file_name, "w") as write_file:
            write_file.write("")
        tf.io.gfile.mkdir(dir_name)
        # Check that directory is a directory.
        self.assertTrue(tf.io.gfile.isdir(dir_name))
        # Check that file is not a directory.
        self.assertFalse(tf.io.gfile.isdir(file_name))

    def test_make_dirs(self):
        """Test make dirs."""
        dir_name = self.path_root
        tf.io.gfile.mkdir(dir_name)
        self.assertTrue(tf.io.gfile.isdir(dir_name))

        parent = self._path_to("test")
        dir_name = self._path_to("test/directory")
        tf.io.gfile.mkdir(parent)
        tf.io.gfile.makedirs(dir_name)
        self.assertTrue(tf.io.gfile.isdir(dir_name))

    def test_copy(self):
        file_src = self._path_to("copy_src.txt")
        file_dest = self._path_to("copy_dest.txt")
        if tf.io.gfile.exists(file_src):
            tf.io.gfile.remove(file_src)
        if tf.io.gfile.exists(file_dest):
            tf.io.gfile.remove(file_dest)

        with tf.io.gfile.GFile(file_src, "w") as write_file:
            write_file.write("Hello,\nworld!")
        tf.io.gfile.copy(file_src, file_dest)
        self.assertTrue(tf.io.gfile.exists(file_dest))

        with tf.io.gfile.GFile(file_dest, "r") as read_file:
            data = read_file.read()
            self.assertTrue(data, "Hello,\nworld!")

    def test_remove(self):
        """Test remove."""
        file_name = self._path_to("file_to_be_removed")
        self.assertFalse(tf.io.gfile.exists(file_name))
        with tf.io.gfile.GFile(file_name, "w") as write_file:
            write_file.write("")
        self.assertTrue(tf.io.gfile.exists(file_name))
        tf.io.gfile.remove(file_name)
        self.assertFalse(tf.io.gfile.exists(file_name))

if __name__ == "__main__":
    tf.test.main()
