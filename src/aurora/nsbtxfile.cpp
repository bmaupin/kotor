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
 *  Treat Nintendo NSBTX files, which contain multiple textures as an
 *  archive of intermediate textures.
 */

/* Based heavily on the BTX0 reader found in the NDS file viewer and
 * and editor Tinke by pleoNeX (<https://github.com/pleonex/tinke>),
 * which is licensed under the terms of the GPLv3.
 *
 * Tinke in turn is based on the NSBTX documentation by lowlines
 * (<http://llref.emutalk.net/docs/?file=xml/btx0.xml>) and the
 * Nintendo DS technical information GBATEK by Martin Korth
 * (<http://problemkaputt.de/gbatek.htm>).
 */

#include "src/common/util.h"
#include "src/common/strutil.h"
#include "src/common/error.h"
#include "src/common/stream.h"
#include "src/common/file.h"
#include "src/common/encoding.h"

#include "src/aurora/nsbtxfile.h"

static const uint32 kXEOSID = MKTAG('X', 'E', 'O', 'S');
static const uint32 kITEXID = MKTAG('I', 'T', 'E', 'X');

static const uint32 kXEOSITEXHeaderSize       = 4 + 4 + 4 + 4 + 4 + 1 + 1 + 1 + 1 + 1;
static const uint32 kXEOSITEXMipMapHeaderSize = 4 + 4 + 4;

static const uint32 kBTX0ID = MKTAG('B', 'T', 'X', '0');
static const uint32 kTEX0ID = MKTAG('T', 'E', 'X', '0');

namespace Aurora {

NSBTXFile::ReadContext::ReadContext(const Texture &t, Common::WriteStream &s) :
	texture(&t), palette(0), nsbtx(0), stream(&s) {
}

NSBTXFile::ReadContext::~ReadContext() {
	delete nsbtx;
	delete[] palette;
}


NSBTXFile::NSBTXFile(const Common::UString &fileName) : _fileName(fileName) {
	load();
}

NSBTXFile::~NSBTXFile() {
}

void NSBTXFile::clear() {
	_resources.clear();
}

const Archive::ResourceList &NSBTXFile::getResources() const {
	return _resources;
}

uint32 NSBTXFile::getITEXSize(const Texture &texture) {
	return kXEOSITEXHeaderSize + kXEOSITEXMipMapHeaderSize + texture.width * texture.height * 4;
}

uint32 NSBTXFile::getResourceSize(uint32 index) const {
	if (index >= _textures.size())
		throw Common::Exception("Texture index out of range (%d/%d)", index, _textures.size());

	return getITEXSize(_textures[index]);
}

void NSBTXFile::writeITEXHeader(const ReadContext &ctx) {
	ctx.stream->writeUint32BE(kXEOSID);
	ctx.stream->writeUint32BE(kITEXID);
	ctx.stream->writeUint32LE(0); // Version
	ctx.stream->writeUint32LE(4); // Pixel format / bytes per pixel

	ctx.stream->writeByte((uint8) ctx.texture->wrapX);
	ctx.stream->writeByte((uint8) ctx.texture->wrapY);
	ctx.stream->writeByte((uint8) ctx.texture->flipX);
	ctx.stream->writeByte((uint8) ctx.texture->flipY);
	ctx.stream->writeByte((uint8) ctx.texture->coordTransform);

	ctx.stream->writeUint32LE(1); // Number of mip maps

	ctx.stream->writeUint32LE(ctx.texture->width);
	ctx.stream->writeUint32LE(ctx.texture->height);
	ctx.stream->writeUint32LE(ctx.texture->width * ctx.texture->height * 4);
}

void NSBTXFile::writePixel(const ReadContext &ctx, byte r, byte g, byte b, byte a) {
	ctx.stream->writeByte(b);
	ctx.stream->writeByte(g);
	ctx.stream->writeByte(r);
	ctx.stream->writeByte(a);
}

void NSBTXFile::getTexture2bpp(const ReadContext &ctx) {
	for (uint32 y = 0; y < ctx.texture->height; y++) {
		for (uint32 x = 0; x < ctx.texture->width; ) {

			uint8 pixels = ctx.nsbtx->readByte();
			for (uint32 n = 0; n < 4; n++, x++, pixels >>= 2) {
				const uint8 pixel = pixels & 3;

				const byte r = ctx.palette[pixel * 3 + 0];
				const byte g = ctx.palette[pixel * 3 + 1];
				const byte b = ctx.palette[pixel * 3 + 2];

				const byte a = (ctx.texture->alpha && (pixel == 0)) ? 0x00 : 0xFF;

				writePixel(ctx, r, g, b, a);
			}

		}
	}
}

void NSBTXFile::getTexture4bpp(const ReadContext &ctx) {
	for (uint32 y = 0; y < ctx.texture->height; y++) {
		for (uint32 x = 0; x < ctx.texture->width; ) {

			uint8 pixels = ctx.nsbtx->readByte();
			for (uint32 n = 0; n < 2; n++, x++, pixels >>= 4) {
				const uint8 pixel = pixels & 0xF;

				const byte r = ctx.palette[pixel * 3 + 0];
				const byte g = ctx.palette[pixel * 3 + 1];
				const byte b = ctx.palette[pixel * 3 + 2];

				const byte a = (ctx.texture->alpha && (pixel == 0)) ? 0x00 : 0xFF;

				writePixel(ctx, r, g, b, a);
			}

		}
	}
}

void NSBTXFile::getTexture8bpp(const ReadContext &ctx) {
	for (uint32 y = 0; y < ctx.texture->height; y++) {
		for (uint32 x = 0; x < ctx.texture->width; x++) {
			const uint8 pixel = ctx.nsbtx->readByte();

			const byte r = ctx.palette[pixel * 3 + 0];
			const byte g = ctx.palette[pixel * 3 + 1];
			const byte b = ctx.palette[pixel * 3 + 2];

			const byte a = (ctx.texture->alpha && (pixel == 0)) ? 0x00 : 0xFF;

			writePixel(ctx, r, g, b, a);
		}
	}
}

void NSBTXFile::getTexture16bpp(const ReadContext &ctx) {
	for (uint32 y = 0; y < ctx.texture->height; y++) {
		for (uint32 x = 0; x < ctx.texture->width; x++) {
			const uint16 pixel = ctx.nsbtx->readUint16();

			const byte r = ( pixel        & 0x1F) << 3;
			const byte g = ((pixel >>  5) & 0x1F) << 3;
			const byte b = ((pixel >> 10) & 0x1F) << 3;

			const byte a = ((pixel >> 15) == 0) ? 0x00 : 0xFF;

			writePixel(ctx, r, g, b, a);
		}
	}
}

void NSBTXFile::getTextureA3I5(const ReadContext &ctx) {
	for (uint32 y = 0; y < ctx.texture->height; y++) {
		for (uint32 x = 0; x < ctx.texture->width; x++) {
			const uint8 pixel = ctx.nsbtx->readByte();

			const uint8 index = pixel & 0x1F;

			const byte r = ctx.palette[index * 3 + 0];
			const byte g = ctx.palette[index * 3 + 1];
			const byte b = ctx.palette[index * 3 + 2];

			const byte a = (((pixel >> 5) << 2) + (pixel >> 6)) << 3;

			writePixel(ctx, r, g, b, a);
		}
	}
}

void NSBTXFile::getTextureA5I3(const ReadContext &ctx) {
	for (uint32 y = 0; y < ctx.texture->height; y++) {
		for (uint32 x = 0; x < ctx.texture->width; x++) {
			const uint8 pixel = ctx.nsbtx->readByte();

			const uint8 index = pixel & 0x07;

			const byte r = ctx.palette[index * 3 + 0];
			const byte g = ctx.palette[index * 3 + 1];
			const byte b = ctx.palette[index * 3 + 2];

			const byte a = (pixel >> 3) << 3;

			writePixel(ctx, r, g, b, a);
		}
	}
}

const NSBTXFile::Palette *NSBTXFile::findPalette(const Texture &texture) const {
	std::vector<Common::UString> palNames;

	palNames.push_back(texture.name);
	palNames.push_back(texture.name + "_pl");
	palNames.push_back(texture.name + "_p");
	palNames.push_back(texture.name + "_");

	for (std::vector<Common::UString>::iterator n = palNames.begin(); n != palNames.end(); ++n)
		for (Palettes::const_iterator p = _palettes.begin(); p != _palettes.end(); ++p)
			if (p->name == *n)
				return &*p;

	return 0;
}

void NSBTXFile::getPalette(ReadContext &ctx) const {
	static const uint16 kPaletteSize[] = { 0, 32, 4, 16, 256, 256, 8,  0 };

	const uint16 size = kPaletteSize[(int)ctx.texture->format] * 3;
	if (size == 0)
		return;

	const Palette *palette = findPalette(*ctx.texture);
	if (!palette)
		throw Common::Exception("Couldn't find a palette for texture \"%s\"", ctx.texture->name.c_str());

	byte *palData = new byte[size];

	try {
		ctx.nsbtx->seek(palette->offset);

		for (uint16 i = 0; i < size; i += 3) {
			const uint16 pixel = ctx.nsbtx->readUint16();

			palData[i + 0] = ( pixel        & 0x1F) << 3;
			palData[i + 1] = ((pixel >>  5) & 0x1F) << 3;
			palData[i + 2] = ((pixel >> 10) & 0x1F) << 3;
		}

	} catch (...) {
		delete palData;
		throw;
	}

	ctx.palette = palData;
}

void NSBTXFile::getTexture(const ReadContext &ctx) {
	ctx.nsbtx->seek(ctx.texture->offset);

	switch (ctx.texture->format) {
		case kFormat2bpp:
			getTexture2bpp(ctx);
			break;

		case kFormat4bpp:
			getTexture4bpp(ctx);
			break;

		case kFormat8bpp:
			getTexture8bpp(ctx);
			break;

		case kFormat16bpp:
			getTexture16bpp(ctx);
			break;

		case kFormatA3I5:
			getTextureA3I5(ctx);
			break;

		case kFormatA5I3:
			getTextureA5I3(ctx);
			break;

		default:
			throw Common::Exception("Unsupported texture format %d", (int) ctx.texture->format);
			break;
	}
}

Common::SeekableReadStream *NSBTXFile::getResource(uint32 index) const {
	if (index >= _textures.size())
		throw Common::Exception("Texture index out of range (%d/%d)", index, _textures.size());

	Common::MemoryWriteStreamDynamic stream(false, getITEXSize(_textures[index]));

	try {
		ReadContext ctx(_textures[index], stream);
		writeITEXHeader(ctx);

		ctx.nsbtx = open();

		getPalette(ctx);
		getTexture(ctx);

	} catch (...) {
		delete[] stream.getData();
		throw;
	}

	return new Common::MemoryReadStream(stream.getData(), stream.size(), true);
}

void NSBTXFile::load() {
	Common::SeekableSubReadStreamEndian *nsbtx = open();

	try {

		readHeader(*nsbtx);
		readTextures(*nsbtx);
		readPalettes(*nsbtx);

		createResourceList();

	} catch (Common::Exception &e) {
		delete nsbtx;

		e.add("Failed reading NSBTX file");
		throw;
	}

	delete nsbtx;
}

Common::SeekableSubReadStreamEndian *NSBTXFile::open() const {
	Common::File *nsbtx = new Common::File(_fileName);

	bool bigEndian = false;
	try {
		const uint32 tag = nsbtx->readUint32BE();
		if (tag != kBTX0ID)
			throw Common::Exception("Invalid NSBTX file (0x%08X)", tag);

		const uint16 bom = nsbtx->readUint16BE();
		if ((bom != 0xFFFE) && (bom != 0xFEFF))
			throw Common::Exception("Invalid BOM: %u", (uint) bom);

		bigEndian = bom == 0xFEFF;

	} catch (...) {
		delete nsbtx;
		throw;
	}

	return new Common::SeekableSubReadStreamEndian(nsbtx, 0, nsbtx->size(), bigEndian, true);
}

void NSBTXFile::readHeader(Common::SeekableSubReadStreamEndian &nsbtx) {
	readFileHeader(nsbtx);
	readInfoHeader(nsbtx);
}

void NSBTXFile::readFileHeader(Common::SeekableSubReadStreamEndian &nsbtx) {
	const uint32 tag = nsbtx.readUint32BE();
	if (tag != kBTX0ID)
		throw Common::Exception("Invalid NSBTX file (%s)", Common::debugTag(tag).c_str());

	const uint16 bom = nsbtx.readUint16();
	if (bom != 0xFEFF)
		throw Common::Exception("Invalid BOM: %u", bom);

	const uint16 version = nsbtx.readUint16();
	if (version != 1)
		throw Common::Exception("Unsupported version %u", version);

	const uint32 fileSize = nsbtx.readUint32();
	if (fileSize > (uint32)nsbtx.size())
		throw Common::Exception("Size too large (%u > %u)", fileSize, nsbtx.size());

	const uint16 headerSize = nsbtx.readUint16();
	if (headerSize != 16)
		throw Common::Exception("Invalid header size (%u)", headerSize);

	const uint16 sectionCount = nsbtx.readUint16();
	if (sectionCount != 1)
		throw Common::Exception("Invalid number of sections (%u)", sectionCount);

	_textureOffset = nsbtx.readUint32();
}

void NSBTXFile::readInfoHeader(Common::SeekableSubReadStreamEndian &nsbtx) {
	nsbtx.seek(_textureOffset);

	const uint32 tag = nsbtx.readUint32BE();
	if (tag != kTEX0ID)
		throw Common::Exception("Invalid NSBTX texture (%s)", Common::debugTag(tag).c_str());

	nsbtx.skip(4 + 4 + 2); // Section size + padding + data size

	_textureInfoOffset = _textureOffset + nsbtx.readUint16();

	nsbtx.skip(4); // Padding

	_textureDataOffset = _textureOffset + nsbtx.readUint32();

	nsbtx.skip(4);     // Padding
	nsbtx.skip(2 + 2); // Compressed data size and info offset
	nsbtx.skip(4);     // Padding
	nsbtx.skip(4 + 4); // Compressed data offset and info data offset
	nsbtx.skip(4);     // Padding

	nsbtx.skip(4); // Palette data size

	_paletteInfoOffset = _textureOffset + nsbtx.readUint32();
	_paletteDataOffset = _textureOffset + nsbtx.readUint32();
}

void NSBTXFile::readTextures(Common::SeekableSubReadStreamEndian &nsbtx) {
	nsbtx.seek(_textureInfoOffset);

	nsbtx.skip(1); // Unknown

	const uint8 textureCount = nsbtx.readByte();

	nsbtx.skip(2); // Section size
	nsbtx.skip(2 + 2 + 4 + textureCount * (2 + 2)); // Unknown

	nsbtx.skip(2 + 2); // Header size + section size

	_textures.resize(textureCount);
	for (Textures::iterator t = _textures.begin(); t != _textures.end(); ++t) {
		t->offset = _textureDataOffset + nsbtx.readUint16() * 8;

		const uint16 flags = nsbtx.readUint16();

		nsbtx.skip(1); // Unknown

		const uint8 unknown = nsbtx.readByte();

		nsbtx.skip(2); // Unknown

		t->width  = 8 << ((flags >> 4) & 7);
		t->height = 8 << ((flags >> 7) & 7);

		t->format = (Format) ((flags >> 10) & 7);

		t->wrapX = ( flags        & 1) != 0;
		t->wrapY = ((flags >>  1) & 1) != 0;
		t->flipX = ((flags >>  2) & 1) != 0;
		t->flipY = ((flags >>  3) & 1) != 0;
		t->alpha = ((flags >> 13) & 1) != 0;

		t->coordTransform = (Transform) (flags >> 14);

		if (t->width == 0x00) {
			switch (unknown & 0x3) {
				case 2:
					t->width = 0x200;
					break;
				default:
					t->width = 0x100;
					break;
			}
		}

		if (t->height == 0x00) {
			switch ((unknown >> 4) & 0x3) {
				case 2:
					t->height = 0x200;
					break;
				default:
					t->height = 0x100;
					break;
			}
		}
	}

	for (Textures::iterator t = _textures.begin(); t != _textures.end(); ++t)
		t->name = Common::readStringFixed(nsbtx, Common::kEncodingASCII, 16).toLower();
}

void NSBTXFile::readPalettes(Common::SeekableSubReadStreamEndian &nsbtx) {
	nsbtx.seek(_paletteInfoOffset);

	nsbtx.skip(1); // Unknown

	const uint8 paletteCount = nsbtx.readByte();

	nsbtx.skip(2); // Section size
	nsbtx.skip(2 + 2 + 4 + paletteCount * (2 + 2)); // Unknown

	nsbtx.skip(2 + 2); // Header size + section size

	_palettes.resize(paletteCount);
	for (Palettes::iterator p = _palettes.begin(); p != _palettes.end(); ++p) {
		const uint16 offset = nsbtx.readUint16() & 0x1FFF;
		const uint16 flags  = nsbtx.readUint16();

		const uint8 paletteStep = ((flags & 1) != 0) ? 16 : 8;

		p->offset = _paletteDataOffset + offset * paletteStep;
	}

	for (Palettes::iterator p = _palettes.begin(); p != _palettes.end(); ++p)
		p->name = Common::readStringFixed(nsbtx, Common::kEncodingASCII, 16).toLower();
}

void NSBTXFile::createResourceList() {
	_resources.resize(_textures.size());

	ResourceList::iterator res = _resources.begin();
	Textures::iterator     tex = _textures.begin();

	uint32 index = 0;
	for ( ; (res != _resources.end()) && (tex != _textures.end()); ++res, ++tex, ++index) {
		res->name  = tex->name;
		res->type  = kFileTypeXEOSITEX;
		res->index = index;
	}
}

} // End of namespace Aurora
