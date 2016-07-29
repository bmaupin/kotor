/* xoreos-tools - Tools to help with xoreos development
 *
 * xoreos-tools is the legal property of its developers, whose names
 * can be found in the AUTHORS file distributed with this source
 * distribution.
 *
 * xoreos-tools is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * xoreos-tools is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with xoreos-tools. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 *  Implementing the writing stream interfaces for memory blocks.
 */

/* Based on ScummVM (<http://scummvm.org>) code, which is released
 * under the terms of version 2 or later of the GNU General Public
 * License.
 *
 * The original copyright note in ScummVM reads as follows:
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <cstring>

#include "src/common/types.h"
#include "src/common/memwritestream.h"

namespace Common {

MemoryWriteStream::MemoryWriteStream(byte *buf, size_t len) : _ptr(buf), _bufSize(len), _pos(0) {
}

MemoryWriteStream::~MemoryWriteStream() {
}

size_t MemoryWriteStream::write(const void *dataPtr, size_t dataSize) {
	// Write at most as many bytes as are still available...
	if (dataSize > _bufSize - _pos)
		dataSize = _bufSize - _pos;

	std::memcpy(_ptr, dataPtr, dataSize);

	_ptr += dataSize;
	_pos += dataSize;

	return dataSize;
}

size_t MemoryWriteStream::pos() const {
	return _pos;
}

size_t MemoryWriteStream::size() const {
	return _bufSize;
}


MemoryWriteStreamDynamic::MemoryWriteStreamDynamic(bool disposeMemory, size_t capacity) :
	_data(0), _disposeMemory(disposeMemory), _ptr(0), _pos(0), _capacity(0), _size(0) {

	reserve(capacity);
}

MemoryWriteStreamDynamic::~MemoryWriteStreamDynamic() {
	if (_disposeMemory)
		delete[] _data;
}

void MemoryWriteStreamDynamic::reserve(size_t s) {
	if (s <= _capacity)
		return;

	byte *oldData = _data;

	while (_capacity < s)
		_capacity = MAX<size_t>(2, _capacity * 2);

	_data = new byte[_capacity];
	_ptr = _data + _pos;

	if (oldData) {
		// Copy old data
		std::memcpy(_data, oldData, _size);
		delete[] oldData;
	}
}

void MemoryWriteStreamDynamic::ensureCapacity(size_t newLen) {
	if (newLen <= _capacity)
		return;

	reserve(newLen);

	_size = newLen;
}

size_t MemoryWriteStreamDynamic::write(const void *dataPtr, size_t dataSize) {
	ensureCapacity(_pos + dataSize);

	std::memcpy(_ptr, dataPtr, dataSize);

	_ptr += dataSize;
	_pos += dataSize;

	if ((size_t)_pos > _size)
		_size = _pos;

	return dataSize;
}

void MemoryWriteStreamDynamic::dispose() {
	delete[] _data;

	_data     = 0;
	_ptr      = 0;
	_pos      = 0;
	_size     = 0;
	_capacity = 0;
}

size_t MemoryWriteStreamDynamic::pos() const {
	return _pos;
}

size_t MemoryWriteStreamDynamic::size() const {
	return _size;
}

byte *MemoryWriteStreamDynamic::getData() {
	return _data;
}

} // End of namespace Common
