#
# This file is part of PPRZLINK.
# 
# PPRZLINK is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# PPRZLINK is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with PPRZLINK.  If not, see <https://www.gnu.org/licenses/>.
#

class RequestUIDFactory:
    _generator = None

    @classmethod
    def _unique_id_generator(cls):
        import os
        pid = os.getpid()

        sequence_number = 0
        while True:
            sequence_number = sequence_number + 1
            yield '{}_{}'.format(pid, sequence_number)

    @classmethod
    def generate_uid(cls):
        if cls._generator is None:
            cls._generator = cls._unique_id_generator()
        return next(cls._generator)
