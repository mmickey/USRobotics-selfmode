#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/** length in bytes of one modem memory page (32KB) **/
#define PAGE_LENGTH         (32 * 1024)

/** string representing the end of the dumpfile **/
#define DUMPFILE_END        "\r\nOK\r\n"
#define DUMPFILE_END_LENGTH 6

void print_usage(char *executable_name) {
	printf("usage: %s INPUT [ OUTPUT ]\n", executable_name);
	printf("\tINPUT\tdump of modem memory as received with commands at+mtm or at+mtp=n\n");
	printf("\tOUTPUT\tfilename where write dump without checksum chars\n");
	printf("if output file is not providen only verifies checksums\n\n");
}

int main(int argc, char *argv[]) {
	FILE          *fp      = NULL; // input file
	FILE          *output  = NULL; // output file
	unsigned char oracle   = 0;    // received checksum
	unsigned char sum      = 0;    // computed checksum
	int           c, c1;           // temporaries
	unsigned int  n;
	unsigned char num_page = 0;
	
	if (argc < 2) {
		print_usage(argv[0]);
		return 1;
	}

	fp = fopen(argv[1], "r");
	if (!fp) {
		fprintf(stderr, "unable to open file %s for reading\n", argv[1]);
		return 1;
	}
	printf("input file: %s\n", argv[1]);
	if (argc > 2) {
		output = fopen(argv[2], "w");
		if (!output) {
			fprintf(stderr, "ERROR unable to open file %s for writing\n", argv[2]);
			fclose(fp);
			return 2;
		}
		printf("output file: %s\n", argv[2]);
	}
	
	while (1) {
		sum = 0;
		oracle = 0;
		c1 = 0;
		
		c = fgetc(fp);
		
		if (c == EOF) {
			if (output) fclose(output);
			fclose(fp);
			printf("dump analysis compete\n");
			return 0;
		}
		
		// convert upper nibble
		if ('A' <= c && c <= 'F') oracle |= c - 'A' + 10;
		else oracle |= c - '0';
		
		oracle = oracle << 4; // shift it in right position
		
		// convert lower nibble
		c = fgetc(fp);
		if ('A' <= c && c <= 'F') oracle |= c - 'A' + 10;
		else oracle |= c - '0';
		
		// get data
		for (n = 0; n < PAGE_LENGTH; n++) {
			c = fgetc(fp);
			assert(c != EOF);
			if (c == 0x10) {
				// toggle 0x10 and 0x00 on c1 flag
				if (c1 == 0x10) c1 = 0x00;
				else            c1 = 0x10;
			} else if (c == 0x03) {
				if (c1 == 0x10) {
					// end of page
					if (output) c1 = fputc(c1, output);
					assert(c1 != EOF); // this means fputc has encountered an error
					c1 = 0x00;
				}
			}
			if (c1 == 0x00) {
				sum ^= c;
				if (output) c = fputc(c, output);
				assert(c != EOF); // this means fputc has encountered an error
			}
		}
		if (oracle != sum) {
			fprintf(stderr, "WARNING page %d: checksum expected is 0x%02X, but checksum calculated is 0x%02X\n", num_page, oracle, sum);
		}
		num_page++;
	}
}
