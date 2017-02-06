#include "utfconverter.h"
#include "struct.txt"

char filename[4096];
char ofilename[4096];
int v = 0, glyphs = 0, surrogate = 0, ofile = 0, fd2;
endianness conversion;
endianness source;
clock_t readreal = 0, readuser = 0, readsys = 0, conreal = 0, conuser = 0, consys = 0, writereal = 0, writeuser = 0, writesys = 0, a, b;
struct tms *sread, *sconvert, *swrite, *eread, *econvert, *ewrite;

int 
main(int argc, char** argv) {
	/* After calling parse_args(), filename and conversion should be set. */
	int fd, rv = 0, ascii = 0;
	float sper, aper;
	unsigned char buf[2] = {0};
	char *input, *output, hostname[4096], actualpath[4096]; 
	Glyph* glyph;	
	struct stat *in, *out;
	struct utsname *buff;
	/*void* memset_return;*/
	
	if ((rv = parse_args(argc, argv)) < 0) {
		return EXIT_FAILURE;
	} else if (rv > 0) {
		return EXIT_SUCCESS;
	}
	if ((fd = open(filename, O_RDONLY)) < 0) {
		fprintf(stderr, "Failed to open file.\n");
		quit_converter(NO_FD);
		return EXIT_FAILURE;
	} 
	if (ofile == 1) {
		if ((fd2 = open(ofilename, O_WRONLY | O_APPEND | O_CREAT, 0666)) < 0) {
			fprintf(stderr, "Failed to open output file.\n");
			close(STDERR_FILENO);
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(fd);
			return EXIT_FAILURE;
		}
	}
	in = malloc(sizeof(struct stat));
	if (in == NULL) {
		quit_converter(fd);
		return EXIT_FAILURE;
	}
	out = malloc(sizeof(struct stat));
	if (out == NULL) {
		quit_converter(fd);
		free(in);
		return EXIT_FAILURE;
	}
	fstat(fd, in);
	fstat(fd2, out);
	if (in->st_dev == out->st_dev && in->st_ino == out->st_ino) {
		fprintf(stderr, "Input and output files are identical.\n");
		free(in);
		free(out);
		quit_converter(fd);
		return EXIT_FAILURE;
	}
	free(in);
	free(out);
	glyph = malloc(sizeof(Glyph));
	if (glyph == NULL) {
		quit_converter(fd);
		return EXIT_FAILURE;	
	}
	
	/* Handle BOM bytes for UTF16 specially. 
         * Read our values into the first and second elements. */
	if((rv = read(fd, &buf[0], 1)) == 1 && 
			(rv = read(fd, &buf[1], 1)) == 1){
		glyphs++; 
		if(buf[0] == 0xff && buf[1] == 0xfe){
			/*file is big endian (swapped to little?)*/
			source = LITTLE; 
		} else if(buf[0] == 0xfe && buf[1] == 0xff){
			/*file is little endian (swapped to big?)*/
			source = BIG;
		} else if (buf[0] == 0xef && buf[1] == 0xbb) {
			if ((rv = read(fd, &buf[0], 1)) == 1 && (buf[0] == 0xbf)) {
				source = UTF8;
			} else {
				/*file has no BOM*/
				free(glyph); 
				fprintf(stderr, "File has no BOM.\n");
				quit_converter(fd); 
				return EXIT_FAILURE;
			}
		} else {
			/*file has no BOM*/
			free(glyph); 
			fprintf(stderr, "File has no BOM.\n");
			quit_converter(fd); 
			return EXIT_FAILURE;
		}
		memset(glyph, 0, sizeof(Glyph));
		/* if(memset_return == NULL){
			asm("movl $8, %esi\n\t"
			    "movl $.LC0, %edi\n\t"
			    "movl $0, %eax");
			memset(glyph, 0, sizeof(Glyph));
		}  */
	} else {
		free(glyph);
		quit_converter(fd);
		return EXIT_FAILURE;	
	}
	if (source == conversion) {
		fprintf(stderr, "File is already encoded in the selected UTF\n");
		free(glyph);
		quit_converter(fd);
		return EXIT_FAILURE;			
	}
	if (source == LITTLE || source == BIG) {
		/* Now deal with the rest of the bytes.*/
		while((rv = read(fd, &buf[0], 1)) == 1 && (rv = read(fd, &buf[1], 1)) == 1) {
			if ((source == BIG && buf[0] == 0x00 && buf[1] <= 0x7f) ||
			(source == LITTLE && buf[0] <= 0x7f && buf[1] == 0x00)) {
				ascii++;		
			}
			write_glyph(swap_endianness(fill_glyph(glyph, buf, source, &fd)));
			memset(glyph, 0, sizeof(Glyph));
		    /*if(memset_return == NULL){
			    asm("movl $8, %esi\n\t"
			        "movl $.LC0, %edi\n\t"
			        "movl $0, %eax");
			   memset(glyph, 0, sizeof(Glyph));
		    }*/
			glyphs++;
		}	
	} else if (source == UTF8) {
		while ((rv = read(fd, &buf[0], 1)) == 1) {
			if (buf[0] <= 0x7f) {
				ascii++;
			}
			write_glyph(convert(fill_glyph2(glyph, buf, source, &fd)));
			memset(glyph, 0, sizeof(Glyph));
			glyphs++;
		}
	}
	if (v > 0) {	
		switch (source) {
			case LITTLE:
				input = "UTF-16LE";
				break;
			case BIG:
				input = "UTF-16BE";
				break;
			case UTF8:
				input = "UTF-8";
				break;
		}
		switch (conversion) {
			case LITTLE:
				output = "UTF-16LE";
				break;
			case BIG:
				output = "UTF-16BE";
				break;
			case UTF8:
				output = "UTF-8";
				break;
		}
		gethostname(hostname, 4096);
		buff = malloc(sizeof(struct utsname));
		if (buff == NULL) {
			free(glyph);
			quit_converter(fd);
			return EXIT_FAILURE;	
		} 
		uname(buff);
		in = malloc(sizeof(struct stat));
		if (in == NULL) {
			free(glyph);
			free(buff);
			quit_converter(fd);
			return EXIT_FAILURE;
		}
		fstat(fd, in);
		realpath(filename, actualpath);
		sper = (float)surrogate/glyphs*100;
		aper = (float)ascii/glyphs*100;
	
		fprintf(stderr, "\n");
		fprintf(stderr, "Input file size: %f kb\n", (float)in->st_size/1024);	
		fprintf(stderr, "Input file path: %s\n", actualpath);
		fprintf(stderr, "Input file encoding: %s\n", input);
		fprintf(stderr, "Output encoding: %s\n", output);
		fprintf(stderr, "Hostmachine: %s\n", hostname);
		fprintf(stderr, "Operating System: %s\n", buff->sysname);
		free(buff);
		free(in);
	}	
	if (v > 1) {
		fprintf(stderr, "Reading: real=%.1f, user=%.1f, sys=%.1f\n", (float)readreal/CLOCKS_PER_SEC, (float)readuser/CLOCKS_PER_SEC, (float)readsys/CLOCKS_PER_SEC);
		fprintf(stderr, "Converting: real=%.1f, user=%.1f, sys=%.1f\n", (float)conreal/CLOCKS_PER_SEC, (float)conuser/CLOCKS_PER_SEC, (float)consys/CLOCKS_PER_SEC);
		fprintf(stderr, "Writing: real=%.1f, user=%.1f, sys=%.1f\n", (float)writereal/CLOCKS_PER_SEC, (float)writeuser/CLOCKS_PER_SEC, (float)writesys/CLOCKS_PER_SEC);
		fprintf(stderr, "ASCII: %d%%\n", (int)(aper + 0.5));
		fprintf(stderr, "Surrogates: %d%%\n", (int)(sper + 0.5));
		fprintf(stderr, "Glyphs: %d\n", glyphs);	
	}
	free(glyph);
	quit_converter(fd);
	return EXIT_SUCCESS;
}

Glyph* swap_endianness(Glyph* glyph) {
	/* Use XOR to be more efficient with how we swap values. */
	glyph->bytes[0] ^= glyph->bytes[1];
	glyph->bytes[1] ^= glyph->bytes[0];
	glyph->bytes[0] ^= glyph->bytes[1];
	if(glyph->surrogate){  /* If a surrogate pair, swap the next two bytes. */
		glyph->bytes[2] ^= glyph->bytes[3];
		glyph->bytes[3] ^= glyph->bytes[2];
		glyph->bytes[2] ^= glyph->bytes[3];
	}
	glyph->end = conversion;
	return glyph;
}

Glyph* convert(Glyph* glyph) {
	unsigned int cp = 0, n = 0;
	if (glyph->bytes[3] == 0x0 && glyph->bytes[2] == 0x0 && glyph->bytes[1] == 0x0) {
		if (conversion == BIG) {
			glyph->bytes[1] = glyph->bytes[0];
			glyph->bytes[0] = 0;
		}
	} else if (glyph->bytes[3] == 0x0 && glyph->bytes[2] == 0x0) {
		glyph->bytes[1] = glyph->bytes[1] << 2;
		cp |= glyph->bytes[1] >> 2;
		glyph->bytes[0] = glyph->bytes[0] << 3;
		n = glyph->bytes[0] >> 3;
		cp |= n << 6;
		if (conversion == BIG) {
			glyph->bytes[0] = cp >> 8;
			glyph->bytes[1] = cp;
		}
		if (conversion == LITTLE) {
			glyph->bytes[0] = cp;
			glyph->bytes[1] = cp >> 8;
		}
	} else if (glyph->bytes[3] == 0x0) {
		glyph->bytes[2] = glyph->bytes[2] << 2;
		cp |= glyph->bytes[2] >> 2;
		glyph->bytes[1] = glyph->bytes[1] << 2;
		n = glyph->bytes[1] >> 2;
		cp |= n << 6;
		glyph->bytes[0] = glyph->bytes[0] << 4;
		n = glyph->bytes[0] >> 4;
		cp |= n << 12;
		if (conversion == BIG) {
			glyph->bytes[0] = cp >> 8;
			glyph->bytes[1] = cp;
		}
		if (conversion == LITTLE) {
			glyph->bytes[0] = cp;
			glyph->bytes[1] = cp >> 8;
		}
	} else {
		glyph->bytes[3] = glyph->bytes[3] << 2;
		cp |= glyph->bytes[3] >> 2;
		glyph->bytes[2] = glyph->bytes[2] << 2;
		n = glyph->bytes[2] >> 2;
		cp |= n << 6;
		glyph->bytes[1] = glyph->bytes[1] << 2;
		n = glyph->bytes[1] >> 2;
		cp |= n << 12;
		glyph->bytes[0] = glyph->bytes[0] << 5;
		n = glyph->bytes[0] >> 5;
		cp |= n << 18;
		cp -= 0x10000;
		n = cp;
		n = n >> 10;
		n += 0xD800;
		cp &= 0x3FF;
		cp += 0xDC00;
		if (conversion == BIG) {
			glyph->bytes[0] = n >> 8;
			glyph->bytes[1] = n;		
			glyph->bytes[2] = cp >> 8;
			glyph->bytes[3] = cp;
		}
		if (conversion == LITTLE) {
			glyph->bytes[0] = n;
			glyph->bytes[1] = n >> 8;		
			glyph->bytes[2] = cp;
			glyph->bytes[3] = cp >> 8;
		}
	}
	glyph->end = conversion;
	return glyph;
}

Glyph* fill_glyph(Glyph* glyph, unsigned char data[2], endianness end, int* fd) {
	unsigned int bits = 0; 
	glyph->bytes[0] = data[0];
	glyph->bytes[1] = data[1];
	
	bits |= (data[FIRST] + (data[SECOND] << 8));
	/* Check high surrogate pair using its special value range.*/
	/*if(bits > 0x000F && bits < 0xF8FF){  */
	if(bits >= 0xD800 && bits <= 0xDBFF){
		if(read(*fd, &data[FIRST], 1) == 1 && read(*fd, &data[SECOND], 1) == 1){
			bits = 0;  
			bits |= (data[FIRST] + (data[SECOND] << 8));
			/*if(bits > 0xDAAF && bits < 0x00FF){  Check low surrogate pair.*/
			if(bits >= 0xDC00 && bits <= 0xDFFF){
				glyph->surrogate = true;
				surrogate++;
			} else {
				/*lseek(*fd, -OFFSET, SEEK_CUR); */ 
				glyph->surrogate = false;
			}
		}
	}
	if(!glyph->surrogate){
		glyph->bytes[THIRD] = glyph->bytes[FOURTH] |= 0;
	} else {
		glyph->bytes[THIRD] = data[FIRST]; 
		glyph->bytes[FOURTH] = data[SECOND];
	}
	glyph->end = end;
	return glyph;
}

Glyph* fill_glyph2(Glyph* glyph, unsigned char data[2], endianness end, int* fd) {
	unsigned char bits = 0;

	glyph->bytes[0] = data[0];
	glyph->surrogate = false;
	bits = data[FIRST];
	if ((bits >> 7) == 0) {
		glyph->bytes[SECOND] = glyph->bytes[THIRD] = glyph->bytes[FOURTH] |= 0;
	} else if ((bits >> 5) == 0x06) {
		if (read(*fd, &data[FIRST], 1) == 1) {
			glyph->bytes[SECOND] = data[FIRST];
			glyph->bytes[THIRD] = glyph->bytes[FOURTH] |= 0;
		}
	} else if ((bits >> 4) == 0x0E) {
		if (read(*fd, &data[FIRST], 1) == 1 && read(*fd, &data[SECOND], 1) == 1) {
			glyph->bytes[SECOND] = data[FIRST];
			glyph->bytes[THIRD] = data[SECOND];
			glyph->bytes[FOURTH] |= 0;
		}
	} else if ((bits >> 3) == 0x1E) {
		if (read(*fd, &data[FIRST], 1) == 1 && read(*fd, &data[SECOND], 1) == 1) {
			glyph->bytes[SECOND] = data[FIRST];
			glyph->bytes[THIRD] = data[SECOND];
			if (read(*fd, &data[FIRST], 1) == 1) {
				glyph->bytes[FOURTH] = data[FIRST];
				glyph->surrogate = true;
			}
		}
	} else {
		fprintf(stderr, "Invalid UTF-8 encoding\n");
	}
	glyph->end = end;
	return glyph;
}

void write_glyph(Glyph* glyph) {
	static int BOM = 0;
	unsigned char bytes[2];
	if(BOM == 0) {
		switch(conversion) {
			case LITTLE:
				bytes[0] = 0xff;
				bytes[1] = 0xfe;
				break;
			case BIG:
				bytes[0] = 0xfe;
				bytes[1] = 0xff;
				break;
			case UTF8:
				return;
		}
		if (ofile == 1) {
			write(fd2, bytes, 2);
		} else {
			write(STDOUT_FILENO, bytes, 2);
		}
		BOM++;
	}
	if(glyph->surrogate){
		if (ofile == 1) {
			write(fd2, glyph->bytes, SURROGATE_SIZE);
		} else {
			write(STDOUT_FILENO, glyph->bytes, SURROGATE_SIZE);
		}	
	} else {
		if (ofile == 1) {
			write(fd2, glyph->bytes, NON_SURROGATE_SIZE);
		} else {
			write(STDOUT_FILENO, glyph->bytes, NON_SURROGATE_SIZE);
		}
	}
}

int parse_args(int argc, char** argv) {
	int option_index, c;
	char* endian_convert = NULL;

	/* If getopt() returns with a valid (its working correctly) 
	 * return code, then process the args! */
	while((c = getopt_long(argc, argv, "hvu:", long_options, &option_index)) 
			!= -1){
		switch(c){ 
			case 'h':
				print_help();
				return 1;
				break;
			case 'v':
				v++;
				break;
			case 'u':
				endian_convert = optarg;
				break;
			default:
				fprintf(stderr, "Unrecognized argument.\n");
				quit_converter(NO_FD);
				return -1;
				break;
		}

	}
	if(optind < argc){
		strcpy(filename, argv[optind]);
	} else {
		fprintf(stderr, "Filename not given.\n");
		print_help();
		return -1;
	}
	if (optind + 1 < argc && (strcmp(argv[optind+1], "stdout") != 0)) {
		ofile = 1;
		strcpy(ofilename, argv[optind+1]);
	}
	if(endian_convert == NULL){
		fprintf(stderr, "Conversion mode not given.\n");
		print_help();
		return -1;
	}

	if(strcmp(endian_convert, "16LE") == 0){ 
		conversion = LITTLE;
	} else if(strcmp(endian_convert, "16BE") == 0){
		conversion = BIG;
	} else {
		print_help();
		return -1;
	}
	return 0;
}

void print_help(void) {
	int i;
	for(i = 0; i < 5; i++){
		printf("%s", USAGE[i]); 
	}
	quit_converter(NO_FD);
}

void quit_converter(int fd) {
	close(STDERR_FILENO);
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	if(fd != NO_FD) {
		close(fd);
		if (ofile == 1) {
			close(fd2);
		}
	}
}

