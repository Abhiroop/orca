#include <stdio.h>

/*
 * This programma merges a partitioned awari endgame database.
 * It does so by looping on the input files and extracting one entry
 * per input file and writing it to output. A complication is
 * that the size of an entry is not a whole number of bytes long.
 * A second complication is that we may not assume that we can keep
 * all input files open at the same time, nor do we want to read very
 * small amounts of data per read, as this slows down the process.
 *
 * Therefore, we keep one inputbuffer per input file, and extracts entries
 * from the inputbuffer. If the desired bits are not in the input buffer,
 * we open the file, read a block of data, close the file, and continue
 * processing. As an entry may be part of two consecutive blocks of data
 * we keep 2 bytes overlap (we move the last two bytes of the previous block,
 * and put the next block just after those two bytes).
 *
 * Output is not really buffered (by us, it is by the system). As soon as we
 * have accumulated 8 bits of data, we put it to output.
 */

/*
 * NAAMSIZE is the max length of a file name. 
 * BUFSIZE is the number of bytes in a block (actually, it is 2 less due to
 * the overlap).
 * MAXPROC is the maximum number of processors allowed. It is used to
 * allocate the buffers.
 */

#define NAAMSIZE	256
#define BUFSIZE	      100000
#define	MAXPROC	       256


/*
 * A buffer contains the filename, so we can open it repeatedly, the
 * offset in the file of the first byte in buf, which contains the
 * current block of data. Bitsgelezen is the number of bits already
 * processed of the file. Maxbit is the index in the file of the last bit
 * currently in the buffer.
 */

typedef struct {
	char naam[NAAMSIZE];
	int byteOffset;
	unsigned char buf[BUFSIZE+3];
	int bitsGelezen;
	int maxBit;
} BufferType;


/*
 * GetBirs() retrieves an entry from a given buffer. The size of the
 * entry is nBits, and the result is put in the low-order bytes of *result.
 *
 * First we check if we have the whole entry in the buffer, or that we
 * need to refresh the buffer.
 * Then we calculated the firstbyte in the buffer containing 1 or more
 * bits of the entry. We assume that at most three bytes are involved, thus
 * each entry should not be more than 17 bits long. In fact, this is
 * useless, since in main we assume that an entry does not exceed a byte.
 * And it won't for awari, unless we calculate the 128 stone database.
 *
 * Next, we extract the relevant bits by some shifting and anding.
 */

GetBits(unsigned int *result, int nBits, BufferType *buffer) {
	int firstByte;

	if (buffer->bitsGelezen + nBits > buffer->maxBit) {
		ReadBuffer(buffer);
	}
	firstByte = buffer->bitsGelezen/8 - buffer->byteOffset;
	*result = 0;
	*result = (buffer->buf[firstByte] << 16) |
		  (buffer->buf[firstByte+1] << 8) |
		  buffer->buf[firstByte+2];
	*result >>= (24 - ((buffer->bitsGelezen&7) + nBits));
	*result &= (1 << nBits) - 1;
	buffer->bitsGelezen += nBits;
}


/*
 * ReadBuffer is called to refresh a buffer, or, to fill the buffer for
 * the first time (if maxBit == 0).
 * First we open the file, then we perform an fseek to skip what we already
 * have read. Next, we either move the overlap bytes to the beginning, read
 * BUFSIZE-2 bytes (or less if it is the last block), and adjust the
 * byteoffset and maxBit fields.
 * If it is the first block, the offset remains at zero, and we read a
 * full block.
 */

ReadBuffer(BufferType *buffer) {
	FILE *fd;
	int nRead, i;

	fd = fopen(buffer->naam, "r");
	if (!fd) {
		printf("Cannot open %s for reading\n", buffer->naam);
		exit (1);
	}
	fseek(fd, buffer->maxBit/8, SEEK_SET);
	if (buffer->maxBit != 0) {
		buffer->buf[0] = buffer->buf[BUFSIZE-2];
		buffer->buf[1] = buffer->buf[BUFSIZE-1];
		nRead = fread(buffer->buf+2, 1, BUFSIZE-2, fd);
		buffer->byteOffset += BUFSIZE-2;
	} else {
		nRead = fread(buffer->buf, 1, BUFSIZE, fd);
	}
	buffer->maxBit += 8*nRead;
	fclose(fd);
}


/*
 * Binomium formule. Only used once, so not made efficient.
 */

Binom(int n, int k) {
	long long result = 1;
	int i;

	for (i = 1; i <= k; i++) {
		result *= n-k+i;
		result /= i;
	}
	return (int) result;
}


/*
 * Here we do the main job. First we make some checks to see if the
 * arguments are specified correctly. Then we determine the size of each entry,
 * the number of entries in the merged databases, and we initialize the
 * buffers.
 * Finally, we loop on the buffers, extract an entry each time, merge the
 * result with the previous result, and each time we have a whole byte,
 * we write it to output.
 */

BufferType inBuffers[MAXPROC];

main(int argc, char *argv[]) {
	unsigned result, oldResult;
	int savedBits, i, nProcs, size, totalEntries,
	    nBits, values;
	char output;

	if (argc < 3) {
		printf("usage: %s <size> <nProcs> file1 .. fileN\n", argv[0]);
		exit (1);
	}
	size = atoi(argv[1]);
	values = size*2 + 1;
	nBits = 0;
	while (values > 1) {
		values = (values+1) >> 1;
		nBits++;
	}
	totalEntries = Binom(12+size, 12);
	nProcs = atoi(argv[2]);
	if (argc != 3+nProcs) {
		printf("usage: %s <size> <nProcs> file1 .. fileN\n", argv[0]);
		exit (1);
	}
	for (i = 0; i < nProcs; i++) {
		inBuffers[i].byteOffset = 0;
		strcpy(inBuffers[i].naam, argv[3+i]);
		inBuffers[i].bitsGelezen = 0;
		inBuffers[i].maxBit = 0;
	}
	oldResult = 0;
	savedBits = 0;
	for (i = 0; i < totalEntries; i++) {
		GetBits(&result, nBits, &inBuffers[i%nProcs]);
		if (nBits+savedBits >= 8) {
			output = oldResult << (8-savedBits) |
				 result >> (nBits+savedBits - 8);
			putchar(output);
			savedBits = savedBits + nBits - 8;
			oldResult = result & ((1 << savedBits)-1);
		} else {
			oldResult <<= nBits;
			oldResult |= result;
			savedBits += nBits;
		}
	}
	if (savedBits > 0) {
		output = oldResult << (8-savedBits);
		putchar(output);
	}
}
