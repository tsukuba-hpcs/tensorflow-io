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
        self.path_root = "chfs://"
        super().__init__(methodName)

    def _path_to(self, name):
        return os.path.join(self.path_root, name)

    def test_create_file(self):
        """Test create file"""
        # check the precondition
        file_name = self._path_to("testfile")
        if tf.io.gfile.exists(file_name):
            tf.io.gfile.remove(file_name)
        # create file
        with tf.io.gfile.GFile(file_name, "w") as write_file:
            write_file.write("")
        # check that file was created
        self.assertTrue(tf.io.gfile.exists(file_name))

        tf.io.gfile.remove(file_name)

        self.assertTrue(not tf.io.gfile.exists(file_name))


if __name__ == "__main__":
    tf.test.main()
