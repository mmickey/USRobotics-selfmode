#include <stdio.h>   /* Standard input/output definitions  */
#include <string.h>  /* String function definitions        */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions           */
#include <errno.h>   /* Error number definitions           */
#include <termios.h> /* POSIX terminal control definitions */
#include <assert.h>
#include <stdlib.h>

// End Of Stream strings
const char *EOS[2] = { "\r\nOK\r\n" , "\r\nERROR\r\n" };

int write_cmd(int fd, char *cmd) {
	char c   = 0;
	int  len = strlen(cmd);
	int  n, i;
	
	if (write(fd, cmd, len) < len) return -1; // cannot write entire command
	
	/* clear the buffer from the echo string (see man 2 write) */
	for (i = 0; i < len; i++) {
		do { n = read(fd, &c, 1); } while (n == 0);
		assert(c == cmd[i]);
	}
	return 0;
}

/* send commands to modem
 * first param is the file descriptor of the modem
 * second param is the command string terminated with \r\0, i.e. 'AT\r\0'
 * third parameter is a pointer allocated by the method where the result is set
 * return 0 if eveything was OK
 */
int send_cmd(int fd, char *cmd, char **ans) {
	char buf[255];
	int  n;
	
	n = write_cmd(fd, cmd);
	if (n != 0) return n; // error

	do {
		n = read(fd, buf, 254);
		buf[n] = '\0';
		fputs(buf, stdout);
	} while (n > 0);
	
	
	return 0;
}

int get_modem_memory(int fd, char *fileName, char pageNum) {
	char *terminator = "\r\nOK\r\n";
	int  pos_term    = 0;
	int  output;          // fd for output file
	char cmd[11];
	char n, ch;
	
	if (pageNum == -1) strncpy(cmd, "at+mtm\r\0", 11);
	else if (pageNum > 63) return 1;
	else {
		char num[3] = "\0\0\0";
		if (pageNum < 10) num[0] = '0' + pageNum;
		else {
			num[1] = '0' + (pageNum % 10);
			num[0] = '0' + (pageNum / 10);
		}
		strncpy(cmd, "at+mtp=\0", 11);
		strcat(cmd, num);
		strcat(cmd, "\r\0");
	}
	
	printf("get_modem_memory: command to send: %s\n", cmd);
	fflush(stdout);

	int errId = write_cmd(fd, cmd);
	if (errId < 0) {
		fprintf(stderr, "write_cmd error %d\n", errId);
		return 2;
	}
	output = creat(fileName, 00644); // create output file
	// byte per byte reading until \r\nOK\r\n
	do {
		n = read(fd, &ch, 1);
		assert(n != 0); // not EOF
		
		if (ch == terminator[pos_term]) pos_term++;
		else if (ch == terminator[0]) {
			n = write(output, terminator, pos_term); // flush buffer in output file, without writing the just-read character equal to terminator[0]
			if (n != pos_term) {
				fprintf(stderr, "errore durante la scrittura del file di dump\n");
				close(output);
				return 3;
			}
			pos_term = 1; // restart end of stream search
		} else {
			if (pos_term > 0) {
				n = write(output, terminator, pos_term);
				if (n != pos_term) {
					fprintf(stderr, "errore durante la scrittura del file di dump\n");
					close(output);
					return 4;
				}
				pos_term = 0;
			}
			n = write(output, &ch, 1);
			if (n != 1) {
				fprintf(stderr, "errore durante la scrittura del file di dump\n");
				close(output);
				return 4;
			}
		}
	} while (pos_term < 6);
	
	n = read(fd, &ch, 1);
	assert(n == 0); // EOF reached

	close(output);
	
	return 0;
}

int init_modem(int fd) {
	char *cmd   = "at\r\0";
	char *ans   = "\r\nOK\r\n\0";
	int  ansPos = 0;
	char c;
	int  n;

	/* send test command */
	n = write(fd, cmd, strlen(cmd));
	if (n < strlen(cmd)) return -1;
	
	printf("AT command sent\nanswer: ");
	
	while (ansPos < strlen(ans)) {
		do { n = read(fd, &c, 1); } while (n == 0);
		
		if (c == ans[ansPos]) ansPos++;
		else if (c == ans[0]) ansPos = 1;
		else ansPos = 0;
		
		if (c == '\r') printf("\\r");
		else if (c == '\n') printf("\\n");
		else printf("%c", c);
	}
	printf("\n");
	return 0;
}

int init_port(char *port) {
	int            fd;
	struct termios options;
	
	/* open the port */
	fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
	fcntl(fd, F_SETFL, 0);
	
	/* get the current options */
	tcgetattr(fd, &options);
	
	/* set raw input, 1 second timeout */
	options.c_cflag     |= (CLOCAL | CREAD);
	options.c_lflag     &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_oflag     &= ~OPOST;
	options.c_cc[VMIN]   = 0;
	options.c_cc[VTIME]  = 10;
	
	/* set the options */
	tcsetattr(fd, TCSANOW, &options);

	return fd;
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("usage: %s PAGE DUMPFILE\n", argv[0]);
		printf("\tPAGE\t\t -1 for all memory dump, 0-63 for single page dump\n");
		printf("\tDUMPFILE\tfile where write memory dump\n");
		return 1;
	}
	int fd = init_port("/dev/ttyS0");
	int res = 0;
	
	res = init_modem(fd);
	if (res != 0) return res;
	
	res = get_modem_memory(fd, argv[2], atoi(argv[1]));
	if (res != 0) return res;
	
	close(fd);
	
	return 0;
}
