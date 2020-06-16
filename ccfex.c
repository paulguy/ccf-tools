#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "zlib.h"

#define MAGIC (0x00464343)
#define ZLIB_CHUNK

typedef struct ccffile {
	char filename[20];
	unsigned int offset;
	unsigned int datasize;
	unsigned int filesize;
} ccffile;

int main(int argc, char **argv) {
	if(argc < 2) {
		fprintf(stderr, "USAGE: ccfex <ccffile>\n");
		return(EXIT_FAILURE);
	}

	fprintf(stderr, "Beginning extraction of %s.\n", argv[1]);
	FILE *infile = fopen(argv[1], "rb");
	if(infile == NULL) {
		fprintf(stderr, "File %s couldn't be opened.\n", argv[1]);
		return(EXIT_FAILURE);
	}

	unsigned int magic, chunksize, filecount, i;
	ccffile **fileentries;
	char temp[21] = "                    ";
	fread(&magic, 4, 1, infile);
	if(magic != MAGIC) {
		fprintf(stderr, "File %s isn't a valid CCF archive. %X\n", argv[1], magic);
		return(EXIT_FAILURE);
	}
	fseek(infile, 16, SEEK_SET); fread(&chunksize, 4, 1, infile);
	fseek(infile, 20, SEEK_SET); fread(&filecount, 4, 1, infile);
	if(filecount == 0) {
		fprintf(stderr, "File %s contains no files.\n", argv[1]);
		return(EXIT_FAILURE);
	}
	fileentries = (ccffile **)malloc(sizeof(ccffile *) * filecount);
	if(fileentries == NULL) {
		fprintf(stderr, "Couldn't allocate memory for file list.\n");
		return(EXIT_FAILURE);
	}
	fseek(infile, 0x20, SEEK_SET);
	for(i = 0; i < filecount; i++) {
		fileentries[i] = (ccffile *)malloc(sizeof(ccffile) * filecount);
		if(fileentries[i] == NULL) {
			fprintf(stderr, "Couldn't allocate memory for file list.\n");
			return(EXIT_FAILURE);
		}
		fread(fileentries[i], 32, 1, infile);
		memcpy(temp, fileentries[i]->filename, 20);
		fprintf(stderr, "%u  %s  %u %u %u\n", i, temp, fileentries[i]->offset, fileentries[i]->datasize, fileentries[i]->filesize);
	}

	char *databuffer, *filebuffer;
	int retval;
	FILE *outfile;
	for(i = 0; i < filecount; i++) {
		memcpy(temp, fileentries[i]->filename, 20);
		outfile = fopen(temp, "wb");
		if(outfile == NULL) {
			fprintf(stderr, "Couldn't open %s for writing.\n", temp);
			return(EXIT_FAILURE);
		}
		fseek(infile, fileentries[i]->offset * chunksize, SEEK_SET);
		databuffer = (char *)malloc(sizeof(char) * fileentries[i]->datasize);
		fread(databuffer, 1, fileentries[i]->datasize, infile);
		if(fileentries[i]->filesize == fileentries[i]->datasize) {
			fwrite(databuffer, 1, fileentries[i]->datasize, outfile);
		} else {
			filebuffer = (char *)malloc(sizeof(char) * fileentries[i]->filesize);
			long size = fileentries[i]->filesize;
			retval = uncompress(filebuffer, &size, databuffer, fileentries[i]->datasize);
			switch(retval) {
				case Z_OK:
					break;
				case Z_MEM_ERROR:
					fprintf(stderr, "Zlib out of memory error.\n");
					return(EXIT_FAILURE);
				case Z_BUF_ERROR:
					fprintf(stderr, "The output filesize is misreported.\n");
					return(EXIT_FAILURE);
				case Z_DATA_ERROR:
					fprintf(stderr, "The compressed data is corrupted.\n");
					return(EXIT_FAILURE);
			}
			fwrite(filebuffer, 1, size, outfile);
			free(filebuffer);
		}
		free(databuffer);
		fclose(outfile);
		fprintf(stderr, ".");
	}

	fprintf(stderr, "\n");
	fclose(infile);
	return(EXIT_SUCCESS);
}
