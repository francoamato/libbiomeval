/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <be_error_exception.h>
#include <be_memory_indexedbuffer.h>

using namespace BiometricEvaluation;
using namespace std;

void
printBuf(string name, Memory::IndexedBuffer &buf)
{
	cout << "Buffer Contents of " << name << endl;

	for (uint32_t i = 0; i != buf.getSize(); i++)
		cout << (buf.get()[i]) << " ";
	cout << endl;
} 

int
doTests(Memory::IndexedBuffer &buf)
{

	Memory::IndexedBuffer assign_copy;
	printBuf("ORIGINAL:", buf); cout << endl;

	cout << "Making a deep copy of the alphabet with COPY CONSTRUCTOR\n";
	Memory::IndexedBuffer copy = buf;
	printBuf("COPY:", copy); cout << endl;

	return (0);
}

int
main (int argc, char* argv[])
{
	char c;
	uint32_t i;

	cout << "Testing buffer with unmanaged memory: " << endl;
	cout << "-------------------------------------" << endl;
	uint8_t carr[26];
	for (c = 'a', i = 0; i < 26; i++, c++)
		carr[i] = c;
	Memory::IndexedBuffer buf2(carr, 26);
	if (doTests(buf2) != 0)
		return (EXIT_FAILURE);
	cout << "-------------------------------------" << endl;

	Memory::uint8Array buf(8);
	for (i = 0; i < buf.size(); i++)
		buf[i] = i + 1;
	Memory::IndexedBuffer buf3(buf);

	try {
		cout << "Getting buffer 8-bit values: " << endl;
		for (i = 0; i < buf3.getSize(); i++) {
			printf("0x%02x; ", buf3.scanU8Val());
		}
		cout << endl;

		buf3.setIndex(0);
		cout << "Getting buffer 16-bit values: " << endl;
		for (i = 0; i < buf3.getSize()/2; i++) {
			uint16_t val = buf3.scanU16Val();
			uint8_t *p = (uint8_t*)&val;
			printf("0x%04x (0x%02x%02x); ", val, p[0], p[1]);
		}
		cout << endl;

		buf3.setIndex(0);
		cout << "Getting buffer 32-bit values: " << endl;
		for (i = 0; i < buf3.getSize()/4; i++) {
			uint32_t val = buf3.scanU32Val();
			uint8_t *p = (uint8_t*)&val;
			printf("0x%08x (0x%02x%02x%02x%02x); ", val,
			    p[0], p[1], p[2], p[3]);
		}
		cout << endl;

		buf3.setIndex(0);
		cout << "Getting buffer 64-bit values: " << endl;
		for (i = 0; i < buf3.getSize()/8; i++) {
			uint64_t val = buf3.scanU64Val();
			uint8_t *p = (uint8_t*)&val;
			stringstream output;
			output << "0x" << hex << setfill('0') << setw(16) <<
			    val;
			output << " (0x";
			for (unsigned int j = 0; j < 8; j++)
				output << setw(2) << hex << (uint16_t)p[j];
			output << "); ";
			cout << output.str();
		}
		cout << endl;
	} catch (const Error::DataError &e) {
		cerr << "Caught " << e.what() << endl;
	}

	bool success = false;
	cout << "Attempt to read off end of buffer: ";
	try {
		cout << buf3.scanU8Val();
	} catch (const Error::DataError&) {
		success = true;
	}
	if (success)
		cout << "Success." << endl;
	else
		cout << "Failure. " << endl;

	return (EXIT_SUCCESS);
}
