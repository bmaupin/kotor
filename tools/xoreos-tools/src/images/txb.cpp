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
 *  TXB (another one of BioWare's own texture formats) loading.
 */

#include "src/common/util.h"
#include "src/common/error.h"
#include "src/common/memreadstream.h"

#include "src/images/txb.h"
#include "src/images/util.h"

static const byte kEncodingBGRA = 0x04;
static const byte kEncodingDXT1 = 0x0A;
static const byte kEncodingDXT5 = 0x0C;

namespace Images {

TXB::TXB(Common::SeekableReadStream &txb) : _dataSize(0), _txiData(0), _txiDataSize(0) {
	load(txb);

	// In xoreos-tools, we always want decompressed images
	decompress();
}

TXB::~TXB() {
	delete[] _txiData;
}

void TXB::load(Common::SeekableReadStream &txb) {
	try {

		bool needDeSwizzle = false;

		readHeader(txb, needDeSwizzle);
		readData  (txb, needDeSwizzle);

		txb.seek(_dataSize + 128);

		readTXIData(txb);

	} catch (Common::Exception &e) {
		e.add("Failed reading TXB file");
		throw;
	}
}

Common::SeekableReadStream *TXB::getTXI() const {
	if (!_txiData || (_txiDataSize == 0))
		return 0;

	return new Common::MemoryReadStream(_txiData, _txiDataSize);
}

void TXB::readHeader(Common::SeekableReadStream &txb, bool &needDeSwizzle) {
	// Number of bytes for the pixel data in one full image
	uint32 dataSize = txb.readUint32LE();

	_dataSize = dataSize;

	txb.skip(4); // Some float

	// Image dimensions
	uint32 width  = txb.readUint16LE();
	uint32 height = txb.readUint16LE();

	if ((width >= 0x8000) || (height >= 0x8000))
		throw Common::Exception("Unsupported image dimensions (%ux%u)", width, height);

	// How's the pixel data encoded?
	byte encoding    = txb.readByte();
	// Number of mip maps in the image
	byte mipMapCount = txb.readByte();

	txb.skip(2); // Unknown (Always 0x0101 on 0x0A and 0x0C types, 0x0100 on 0x09?)
	txb.skip(4); // Some float
	txb.skip(108); // Reserved

	needDeSwizzle = false;

	uint32 minDataSize, mipMapSize;
	if        (encoding == kEncodingBGRA) {
		// Raw BGRA

		needDeSwizzle = true;

		_format = kPixelFormatB8G8R8A8;

		minDataSize = 4;
		mipMapSize  = width * height * 4;

	} else if (encoding == kEncodingDXT1) {
		// S3TC DXT1

		_format = kPixelFormatDXT1;

		minDataSize = 8;
		mipMapSize  = width * height / 2;

	} else if (encoding == kEncodingDXT5) {
		// S3TC DXT5

		_format = kPixelFormatDXT5;

		minDataSize = 16;
		mipMapSize  = width * height;

	} else if (encoding == 0x09)
		// TODO: This seems to be some compression with 8bit per pixel. No min
		//       data size; 2*2 and 1*1 mipmaps seem to be just that big.
		//       Image data doesn't seem to be simple grayscale, paletted,
		//       RGB2222 or RGB332 data either.
		throw Common::Exception("Unsupported TXB encoding 0x09");
	 else
		throw Common::Exception("Unknown TXB encoding 0x%02X (%dx%d, %d, %d)",
				encoding, width, height, mipMapCount, dataSize);

	if (!hasValidDimensions(_format, width, height))
		throw Common::Exception("Invalid dimensions (%dx%d) for format %d", width, height, _format);

	const size_t fullImageDataSize = getDataSize(_format, width, height);
	if (dataSize < fullImageDataSize)
		throw Common::Exception("Image wouldn't fit into data");

	_mipMaps.reserve(mipMapCount);
	for (uint32 i = 0; i < mipMapCount; i++) {
		MipMap *mipMap = new MipMap;

		mipMap->width  = MAX<uint32>(width,  1);
		mipMap->height = MAX<uint32>(height, 1);

		if (((mipMap->width < 4) || (mipMap->height < 4)) && (mipMap->width != mipMap->height)) {
			// Invalid mipmap dimensions
			delete mipMap;
			break;
		}

		mipMap->size = MAX<uint32>(mipMapSize, minDataSize);

		mipMap->data = 0;

		const size_t mipMapDataSize = getDataSize(_format, mipMap->width, mipMap->height);

		if ((dataSize < mipMap->size) || (mipMap->size < mipMapDataSize)) {
			// Wouldn't fit
			delete mipMap;
			break;
		}

		dataSize -= mipMap->size;

		_mipMaps.push_back(mipMap);

		width      >>= 1;
		height     >>= 1;
		mipMapSize >>= 2;

		if ((width < 1) && (height < 1))
			break;
	}

	if ((mipMapCount != 0) && (_mipMaps.empty()))
		throw Common::Exception("Couldn't read any mip maps");
}

void TXB::deSwizzle(byte *dst, const byte *src, uint32 width, uint32 height) {
	for (uint32 y = 0; y < height; y++) {
		for (uint32 x = 0; x < width; x++) {
			const uint32 offset = deSwizzleOffset(x, y, width, height) * 4;

			*dst++ = src[offset + 0];
			*dst++ = src[offset + 1];
			*dst++ = src[offset + 2];
			*dst++ = src[offset + 3];
		}
	}
}

void TXB::readData(Common::SeekableReadStream &txb, bool needDeSwizzle) {
	for (std::vector<MipMap *>::iterator mipMap = _mipMaps.begin(); mipMap != _mipMaps.end(); ++mipMap) {

		// If the texture width is a power of two, the texture memory layout is "swizzled"
		const bool widthPOT = ((*mipMap)->width & ((*mipMap)->width - 1)) == 0;
		const bool swizzled = needDeSwizzle && widthPOT;

		(*mipMap)->data = new byte[(*mipMap)->size];

		if (swizzled) {
			std::vector<byte> tmp((*mipMap)->size);

			if (txb.read(&tmp[0], (*mipMap)->size) != (*mipMap)->size)
				throw Common::Exception(Common::kReadError);

			deSwizzle((*mipMap)->data, &tmp[0], (*mipMap)->width, (*mipMap)->height);

		} else {
			if (txb.read((*mipMap)->data, (*mipMap)->size) != (*mipMap)->size)
				throw Common::Exception(Common::kReadError);
		}

	}
}

void TXB::readTXIData(Common::SeekableReadStream &txb) {
	// TXI data for the rest of the TXB
	_txiDataSize = txb.size() - txb.pos();

	if (_txiDataSize == 0)
		return;

	_txiData = new byte[_txiDataSize];

	if (txb.read(_txiData, _txiDataSize) != _txiDataSize)
		throw Common::Exception(Common::kReadError);
}

} // End of namespace Images
