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
 *  Tool to decompress "small" files, Nintendo DS LZSS (types 0x00 and 0x10), found in Sonic.
 */

#include <cstring>
#include <cstdio>

#include "src/common/version.h"
#include "src/common/ustring.h"
#include "src/common/util.h"
#include "src/common/strutil.h"
#include "src/common/error.h"
#include "src/common/readfile.h"
#include "src/common/writefile.h"

#include "src/aurora/smallfile.h"

void printUsage(FILE *stream, const char *name);
bool parseCommandLine(int argc, char **argv, int &returnValue, Common::UString &inFile,
                      Common::UString &outFile);

void desmall(const Common::UString &inFile, const Common::UString &outFile);

int main(int argc, char **argv) {
	int returnValue;
	Common::UString inFile, outFile;
	if (!parseCommandLine(argc, argv, returnValue, inFile, outFile))
		return returnValue;

	try {
		desmall(inFile, outFile);
	} catch (Common::Exception &e) {
		Common::printException(e);
		return -1;
	} catch (std::exception &e) {
		error("%s", e.what());
	}

	return 0;
}

bool parseCommandLine(int argc, char **argv, int &returnValue, Common::UString &inFile,
                      Common::UString &outFile) {

	std::vector<Common::UString> files;

	bool optionsEnd = false;
	for (int i = 1; i < argc; i++) {
		// A "--" marks an end to all options
		if (!strcmp(argv[i], "--")) {
			optionsEnd = true;
			continue;
		}

		// We're still handling options
		if (!optionsEnd) {
			// Help text
			if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
				printUsage(stdout, argv[0]);
				returnValue = 0;

				return false;
			}

			if (!strcmp(argv[i], "--version")) {
				printVersion();
				returnValue = 0;

				return false;
			}

			if (!strncmp(argv[i], "-", 1) || !strncmp(argv[i], "--", 2)) {
			  // An options, but we already checked for all known ones

				printUsage(stderr, argv[0]);
				returnValue = -1;

				return false;
			}
		}

		files.push_back(argv[i]);
	}

	if (files.size() != 2) {
		printUsage(stderr, argv[0]);
		returnValue = -1;

		return false;
	}

	inFile  = files[0];
	outFile = files[1];

	return true;
}

void printUsage(FILE *stream, const char *name) {
	std::fprintf(stream, "Nintendo DS LZSS (types 0x00 and 0x10) decompressor\n");
	std::fprintf(stream, "Usage: %s <input file> <output file>\n", name);
	std::fprintf(stream, "  -h      --help              This help text\n");
	std::fprintf(stream, "          --version           Display version information\n");
}

void desmall(const Common::UString &inFile, const Common::UString &outFile) {
	Common::ReadFile  in(inFile);
	Common::WriteFile out(outFile);

	Aurora::Small::decompress(in, out);
}
