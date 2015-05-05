#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sqlite3.h>

#define DB_FILE "messages.sqlite3"

typedef enum { unknown, fax, voice, data } MsgType;

typedef struct {
	char days;
	char hours;
	char minutes;
} TimeStamp;

typedef struct {
	unsigned char idx;           ///< message reference number
	MsgType       type;          ///< type of message (unknuwn, fax, voice, data)
	unsigned char info;          ///< number of pages for fax message, length in seconds for voice message (0 = unknown)
	TimeStamp     timeStamp;     ///< time elasped between last +M clock reset and message reception
	char          senderId[21];  ///< fax ID of the sender or nothing for voice data
	char          fileName[255]; ///< name of file where message is stored
} HeaderField;

void print_usage(char *fileName) {
	printf("usage: %s DUMP_FILE, [ DUMP_DIR ]\n", fileName);
	printf("\tDUMP_FILE\tname of input dump file alredy cleared of chechsums\n");
	printf("\tDUMP_DIR\tname of directory where place single fax files\n");
	printf("if DUIMP_DIR is omitted default choice (./messages/) is taken\n\n");
}

int main(int argc, char *argv[]) {
/*
	FILE *fp     = NULL;
	char *outDir = NULL;
	if (argc < 2) {
		print_usage(argv[0]);
		return 1;
	}
	fp = fopen(argv[1], "r");
	if (!fp) {
		fprintf(stderr, "unable to open file %s for reading\n", argv[1]);
		return 1;
	}
	printf("input file: %s\n", fileName);
	if (argc > 2) {
		outDir = malloc(strlen(argv[2])+2);
		strcpy(outDir, argv[2]);
		int len = strlen(outDir);
		if (outDir[len-1] != '/')
			outDir[len] = '/';
		outDir[len+1] = '\0';
	} else {
		outDir = malloc(12);
		strcpy(outDir, "./messages/\0");
	}
	// create the output directory
	char *cmd = malloc(strlen("mkdir -p ") + strlen(outDir) + 1);
	strcpy(cmd, "mkdir -p\0");
	strcat(cmd, outDir);
	if (system(cmd) != 0) {
		fprintf(stderr, "unable to create output directory %s\n", outDir);
		free(cmd);
		fclose(fp);
		return 2;
	}
	free(cmd);
	
	free(outDir);
*/
	assert(argc == 2);
	FILE        *fp = fopen(argv[1], "r");
	sqlite3     *db = NULL;
	int         rc  = 0;
	HeaderField header;
	long        curr_addr = 0;
	long        next_msg_addr;

	rc = sqlite3_open(DB_FILE, &db);
	if (rc) {
		sqlite3_close(db);
		exit(10);
	}
	rc = sqlite3_exec(db, "create table msg (id integer, type integer, info integer, timestamp text, sender text, filename text);", NULL, NULL, NULL);
	if (rc) {
		sqlite3_close(db);
		exit(11);
	}
	
	while (1) {
		int tmp = fgetc(fp); curr_addr++;
		if (tmp == EOF) break; // end of file reached
		header.idx = (unsigned char) tmp;
		if (header.idx == 0xff) break; // no more messages, only 0xFF padding
		
		switch (fgetc(fp)) {
			case 1:  header.type = fax;     break;
			case 2:  header.type = voice;   break;
			case 3:  header.type = data;    break;
			default: header.type = unknown; break;
		}
		curr_addr++;
		header.info = fgetc(fp);  curr_addr++;
		fgetc(fp); curr_addr++; // drop "retrieval related management flags"
		fgetc(fp); curr_addr++; // drop "status of message reception"
		header.timeStamp.days    = fgetc(fp); curr_addr++;
		header.timeStamp.hours   = fgetc(fp); curr_addr++;
		header.timeStamp.minutes = fgetc(fp); curr_addr++;
		fgets(header.senderId, 21, fp); curr_addr+=20; // 20 chars plus the '\0'
		fgetc(fp); curr_addr++; // drop "address of the beginning previous message byte 1"
		fgetc(fp); curr_addr++; // drop "address of the beginning previous message byte 2"
		fgetc(fp); curr_addr++; // drop "address of the beginning previous message byte 3"
		next_msg_addr  = 32 * 1024 * (fgetc(fp) - 4); // page size is 32KB and user data starts at page 4
		next_msg_addr += (fgetc(fp) << 8); // high part of 16 bit offset address
		next_msg_addr += fgetc(fp); // low partof 16 bit offset address
		curr_addr += 3;
		
		// timestamp and file name not providen
		char sql[255];
		snprintf(sql, 255, "INSERT INTO msg (id, type, info, sender) VALUES ('%03d', '%d', '%03d', '%s');", header.idx, header.type, header.info, header.senderId);
		rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
		if (rc) {
			sqlite3_close(db);
			exit(12);
		}
		
		printf("message size is %ld bytes\n", next_msg_addr - curr_addr);
		int page = 1;
		int filename_length = 13+3+3+1;
		char *filename_format = "output.%03d-p%03d.fax";
		char filename[filename_length];
		snprintf(filename, filename_length, filename_format, header.idx, page);
		FILE *out = fopen(filename, "w");
		assert(out != NULL);
		unsigned char eop = 0; // end-of-page flag
		for (/* noop */ ; curr_addr < next_msg_addr; curr_addr++) {
			int c = fgetc(fp);
			assert(c != EOF);
			c = fputc(c, out);
			assert(c != EOF);
			
			if (c == 0x10) eop = 1;
			else if (c == 0x03 && eop == 1) {
				// found the second part of end-of-page token
				if (fclose(out) == EOF) {
					fprintf(stderr, "ERROR when trying to close %s\naborting...\n", filename);
					exit(1);
				}
				out = NULL;
				if (++page > header.info) {
					// no more pages to extract,
					// skip source file until
					// next_msg_addr
					while (++curr_addr < next_msg_addr) {
						c = fgetc(fp);
						assert(c != EOF);
					}
				} else {
					// open new output pagefile
					snprintf(filename, filename_length, filename_format, header.idx, page);
					out = fopen(filename, "w");
					assert(out != NULL);
				}
				eop = 0;
			} else eop = 0;
		}
		if (out != NULL) fclose(out);
	}
	
	sqlite3_close(db);
	fclose(fp);
	return 0;
}
