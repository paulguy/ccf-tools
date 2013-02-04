#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "zlib.h"

const int ZERO = 0;
const int HEADER[8] = {
	0x00464343, 0x00000000, 0x00000000, 0x00000000,
	0x00000020, 0x00000000, 0x00000000, 0x00000000};

typedef struct ccffile {
	char filename[20];
	unsigned int offset;
	unsigned int datasize;
	unsigned int filesize;
} ccffile;

int main(int argc, char **argv) {
	ccffile **fileentries;
	FILE *infile, *outfile;
	int retval;
	unsigned int files, size, curpos, i, j;
	char *indata, *outdata;

	if(argc < 2) {
		fprintf(stderr, "USAGE: ccfex <infile1> [infile2] ...\n");
		return(EXIT_FAILURE);
	}

	fprintf(stderr, "Beginning compression of out.ccf.\n");
	outfile = fopen("out.ccf", "wb");
	if(outfile == NULL) {
		fprintf(stderr, "File out.ccf couldn't be opened.\n");
		return(EXIT_FAILURE);
	}

	//prepare file entries as far as we can for now
	files = argc - 1; //files to be inserted in to the archive
	curpos = files + 1; //current position in 32 byte blocks, after header and file entries have been written (start of data region)
	fileentries = (ccffile **)malloc(sizeof(ccffile *) * files);
	if(fileentries == NULL) {
		fprintf(stderr, "Couldn't allocate enough memory!\n");
		return(EXIT_FAILURE);
	}
	for(i = 0; i < files; i++) {
		fprintf(stderr, "%u %s ", i, argv[i + 1]);
		fileentries[i] = (ccffile *)malloc(sizeof(ccffile));
		if(fileentries[i] == NULL) {
			fprintf(stderr, "Couldn't allocate enough memory!\n");
			return(EXIT_FAILURE);
		}
		int t = strlen(argv[i + 1]);
		if(t > 20) {
			t = 20;
		}
		memcpy(fileentries[i]->filename, argv[i + 1], t);
		infile = fopen(argv[i + 1], "rb");
		fseek(infile, 0, SEEK_END);
		size = ftell(infile);
		fileentries[i]->filesize = size;
		fileentries[i]->datasize = size * 2 + 12; //needed for zlib, will have the proper size later on
		fclose(infile);
		fprintf(stderr, "%u bytes\n", fileentries[i]->filesize);
	}

	//write header
	fwrite(&HEADER, 4, 8, outfile);
	fseek(outfile, 20, SEEK_SET);
	fwrite(&files, 4, 1, outfile);
	fseek(outfile, 32, SEEK_SET);

	//write placeholders for proper headers
	for(i = 0; i < files; i++) {
		fwrite(&HEADER, 4, 8, outfile);
	}

	//compress and store files, then update the file entries
	for(i = 0; i < files; i++) {
		fprintf(stderr, ".");
		indata = (char *)malloc(sizeof(char) * fileentries[i]->filesize);
		if(indata == NULL) {
			fprintf(stderr, "Couldn't allocate enough memory!\n");
			return(EXIT_FAILURE);
		}
		outdata = (char *)malloc(sizeof(char) * fileentries[i]->datasize);
		if(outdata == NULL) {
			fprintf(stderr, "Couldn't allocate enough memory!\n");
			return(EXIT_FAILURE);
		}

		infile = fopen(argv[i + 1], "rb");
		if(infile == NULL) {
			fprintf(stderr, "Couldn't open file %s\n", argv[i + 1]);
			return(EXIT_FAILURE);
		}
		fread(indata, 1, fileentries[i]->filesize, infile);
		fclose(infile);

		long datasize = (long)fileentries[i]->datasize;
		retval = compress(outdata, &datasize, indata, fileentries[i]->filesize);
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

		fileentries[i]->datasize = datasize & 0xFFFFFFFF;
		if(fileentries[i]->datasize > fileentries[i]->filesize) {
			fileentries[i]->datasize = fileentries[i]->filesize;
			fwrite(indata, 1, fileentries[i]->filesize, outfile);
		} else {
			fwrite(outdata, 1, fileentries[i]->datasize, outfile);
		}
		fileentries[i]->offset = curpos;
		curpos += fileentries[i]->datasize / 32 + 1;
		fseek(outfile, 32 * i + 32, SEEK_SET);
		fwrite(fileentries[i], sizeof(ccffile), 1, outfile);
		fseek(outfile, 0, SEEK_END);
		int end = ftell(outfile);
		for(j = 0; j < 32 - (end % 32); j++) { //pad out to next block
			fwrite(&ZERO, 1, 1, outfile);
		}
		free(indata);
		free(outdata);
	}

	fprintf(stderr, "\n");
	fclose(outfile);
	return(EXIT_SUCCESS);
}
