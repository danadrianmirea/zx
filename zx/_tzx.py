# -*- coding: utf-8 -*-

#   ZX Spectrum Emulator.
#   https://github.com/kosarev/zx
#
#   Copyright (C) 2017-2019 Ivan Kosarev.
#   ivan@kosarev.info
#
#   Published under the MIT license.


from ._binary import BinaryParser
from ._tape import *
import zx


class TZXFile(zx.SoundFile):
    _TICKS_FREQ = 3500000

    def __init__(self, fields):
        zx.SoundFile.__init__(self, TZXFileFormat, fields)

    def get_pulses(self):
        level = False
        for block in self['blocks']:
            id = block['id']
            if id == '10 (Standard Speed Data Block)':
                # The block itself.
                data = block['data']
                for pulse in get_standard_block_pulses(data):
                    yield (level, pulse)
                    level = not level

                # Pause.
                pause_duration = block['pause_after_block_in_ms']

                # Pause blocks of zero duration shall be ignored,
                # and then the output level remains the same.
                if pause_duration == 0:
                    continue

                # At the end of the non-zero-duration pause the
                # output level shall be low.
                ''' TODO: Despite the specification, this cases
                          tape loading errors.
                if level:
                    # Give the high pulse 1ms of time and drop it.
                    yield (level, self._TICKS_FREQ / 1000)
                    pause_duration -= 1
                    level = not level

                assert not level
                '''

                if pause_duration:
                    yield (level, pause_duration * self._TICKS_FREQ / 1000)
            elif id == '30 (Text Description)':
                print(block['text'])
            else:
                assert 0, block  # TODO


class TZXFileFormat(zx.SoundFileFormat):
    _NAME = 'TZX'

    def _parse_standard_speed_data_block(self, parser):
        block = parser.parse([('pause_after_block_in_ms', '<H'),
                              ('data_size', '<H')])
        block.update({'id': '10 (Standard Speed Data Block)',
                      'data': parser.extract_block(block['data_size'])})
        del block['data_size']
        return block

    def _parse_text_description(self, parser):
        size = parser.parse_field('B', 'text_size')
        text = parser.extract_block(size)
        return {'id': '30 (Text Description)',
                'text': text}

    _ARCHIVE_INFO_STRING_IDS = {
        0x00: 'Full title',
        0x01: 'Software house/publisher',
        0x02: 'Author(s)',
        0x03: 'Year of publication',
        0x04: 'Language',
        0x05: 'Game/utility type',
        0x06: 'Price',
        0x07: 'Protection scheme/loader',
        0x08: 'Origin',
        0xFF: 'Comment(s)',
    }

    def _parse_archive_info(self, parser):
        block_size = parser.parse_field('<H', 'block_size')
        num_of_strings = parser.parse_field('B', 'num_of_strings')
        for _ in range(num_of_strings):
            id = parser.parse_field('B', 'id')
            length = parser.parse_field('B', 'length')
            body = parser.extract_block(length)

            if id not in self._ARCHIVE_INFO_STRING_IDS:
                raise zx.Error('Unknown TZX archive info string id 0x%02x.' %
                               id)

            print('%s: %s' % (self._ARCHIVE_INFO_STRING_IDS[id], body))

    _BLOCK_PARSERS = {
        0x10: _parse_standard_speed_data_block,
        0x30: _parse_text_description,
        0x32: _parse_archive_info,
    }

    def _parse_block(self, parser):
        block_id = parser.parse_field('B', 'block_id')
        if block_id not in self._BLOCK_PARSERS:
            raise zx.Error('Unsupported TZX block id 0x%x.' % block_id)

        return self._BLOCK_PARSERS[block_id](self, parser)

    def parse(self, image):
        parser = BinaryParser(image)

        # Parse header.
        header = parser.parse([('signature', '8s'),
                               ('major_revision', 'B'),
                               ('minor_revision', 'B')])
        tzx_signature = b'ZXTape!\x1a'
        if header['signature'] != tzx_signature:
            raise zx.Error('Bad TZX file signature %r; expected %r.' % (
                              header['signature'], tzx_signature))

        # Parse blocks.
        blocks = []
        while not parser.is_eof():
            blocks.append(self._parse_block(parser))

        return TZXFile({**header, 'blocks': blocks})
