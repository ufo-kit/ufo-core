import os
import tempfile
import shutil
import contextlib


def disable(func):
    def empty():
        pass

    return empty


class TestDir(object):
    def __init__(self, path):
        self.root = path

    def path(self, name):
        return os.path.join(self.root, name)


@contextlib.contextmanager
def tempdir():
    root = tempfile.mkdtemp()
    yield TestDir(root)
    shutil.rmtree(root)
