# -*- coding: utf-8 -*-

#   ZX Spectrum Emulator.
#   https://github.com/kosarev/zx
#
#   Copyright (C) 2017-2019 Ivan Kosarev.
#   ivan@kosarev.info
#
#   Published under the MIT license.


class Data(object):
    def __init__(self, fields):
        self._fields = fields

    def __contains__(self, id):
        return id in self._fields

    def __getitem__(self, id):
        return self._fields[id]

    def __repr__(self):
        return repr(self._fields)

    def __iter__(self):
        for id in self._fields:
            yield id

    def items(self):
        for field in self._fields.items():
            yield field


class FileFormat(object):
    pass
