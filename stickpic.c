#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include <math.h>
#include <string.h>
#include <gd.h>
#include <gdfonts.h>
#include <gdfontt.h>
#include <gdfontl.h>

//fstat:
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

//#define BLOCKSIZE 1*1024
//#define TIMEOUT 30

#include "minilzo.h"

#define HEAP_ALLOC(var,size) \
    lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);




void getcolor (int *colorpointer, double value, int colormap, double minValue, double maxValue);

/*
inline int
random_number (double max)	// between zero and max
{
	return (int) ((max + 1) * random () / (RAND_MAX + 1.0));
}

void
bleachcolor (int *colorpointer, float factor)
{
        // factor = 0 causes identity operation
        colorpointer[0] = (int) (((float) colorpointer[0] + factor * 255.0) / (factor + 1.0));
        colorpointer[1] = (int) (((float) colorpointer[1] + factor * 255.0) / (factor + 1.0));
        colorpointer[2] = (int) (((float) colorpointer[2] + factor * 255.0) / (factor + 1.0));
}
*/

void
handle (const char *string, int error)
{
	if (error) {
		perror (string);
		exit (EXIT_FAILURE);
	}
}

void
check_block (unsigned char *buf, int buflen, double *mean, double *stddev)
{
	int i;
	double mean_ = 0;
	double tmp = 0;
	double stddev_ = 0;
	for (i = 0; i < buflen; i++) {
//              printf("(%d,%d)", i, buf[i]);
		mean_ += (int) buf[i];
	}
//      printf("\n");
	mean_ = mean_ / buflen;
//      printf("blockmean: %.3g\n", mean);

	//stddev:
	for (i = 0; i < buflen; i++) {
//              printf("(%d,%d)", i, buf[i]);
		stddev_ += ((double) buf[i] - mean_) * ((double) buf[i] - mean_);
	}
	stddev_ = stddev_ / buflen;
	*stddev = sqrt (stddev_);
//	printf ("blockmean: %.3g stddev: %.3g\n", mean, stddev);

	*mean = mean_;
}

int
main (int argc, char **argv)
{
	unsigned char *buffer, *cbuffer;
	int fd, retval;
	lzo_uint outlen;
	int inlen, blocksize;
	int scalefac;
	unsigned long numblocks, k, picx, picy;
	off64_t offset;
	long file_size;
	struct stat stbuf;
	double *data;
	FILE *f;
	double valmin, valmax;
	char* filename;

	setvbuf (stdout, NULL, _IONBF, 0);
	file_size = 0;

	filename = malloc(200);

	printf ("Stickpic makes a pictures based on device sector statistics...\n");

	if (argc < 2 || argc > 3) {
		printf ("Usage: stickpic <raw disk device>  [picnamebase]\n");
		exit (EXIT_SUCCESS);
	}
	if(3 == argc){
		printf("Called as: %s %s %s\n", argv[0], argv[1], argv[2]);
	} else if(2 == argc){
		printf("Called as: %s %s\n", argv[0], argv[1]);
	}

	fd = open (argv[1], O_RDONLY);
	handle ("open", fd < 0);

	fstat (fd, &stbuf);
	file_size = stbuf.st_size;
	printf ("Determined file_size by fstat: %lu \n", file_size);
	numblocks = file_size / 512;
	if(file_size % 512) numblocks++;

	if(file_size == 0){
		printf ("Cannot use the fstat on this (is it a device?) Trying ioctl now.\n");
		retval = ioctl (fd, BLKGETSIZE, &numblocks);
		handle ("ioctl", retval == -1);
		printf ("%s [%luMB] (%lu blocks)\n", argv[1], numblocks / 2048, numblocks);
	}


	//Initialize LZO
	if (lzo_init() != LZO_E_OK){
		printf("internal error - lzo_init() failed !!!\n");
		printf("(this usually indicates a compiler bug - try recompiling\nwithout optimizations, and enable '-DLZO_DEBUG' for diagnostics)\n");
		return 3;
	}


//      time(&start);
//      srand(start);
//      signal(SIGALRM, &done);
//      alarm(1);

//	picx = 1024;		//512kb
//	picx = 2048;		//1MB
//	picy = numblocks / picx;
//	if (numblocks % picx)
//		picy++;

	//find long side of picture, prefer ratio 4/3 or maximum of 1400

        //assume picture is ideally 1400x1050 (4/3), is 1.47Mpixel
        //assume picture is ideally 1600x1200 (4/3), is 1.92Mpixel
	scalefac = (int) ceil(numblocks/1920000.0);
	blocksize = 512 * scalefac;
	printf("blocksize should be %d (512 * %d)\n", blocksize, scalefac);

	//scalefac = blocksize / 512;
	picx = (int) sqrt((numblocks/scalefac) * 4.0/3.0);
	if(picx > 1600) picx = 1600;
	picy = (numblocks/scalefac) / picx;
	if ((numblocks/scalefac) % picx) picy++;



	buffer = calloc (blocksize, sizeof (unsigned char));
	cbuffer = calloc (2*blocksize, sizeof (unsigned char)); //conservative, for incompressible data; factor 2 is excessive

	inlen = blocksize;

	printf ("picx = %lu, picy = %lu, picx*picy = %lu\n", picx, picy, picx * picy);

	data = calloc (picx * picy, sizeof(double));

	int i, j, white, black, lightblue, blue, red, color1, color2;
	int x1, y1, x2, y2, jpegquality;
	float marker_x, marker_y;
	white = gdTrueColor (255, 255, 255);
	black = gdTrueColor (0, 0, 0);
	lightblue = gdTrueColor (140, 140, 255);
	blue = gdTrueColor (0, 0, 255);
	red = gdTrueColor (180, 0, 0);
	color1 = blue;
	color2 = red;
	FILE *out;
	double doubletmp, stddev, maxstddev;
	int *colorpointer;

	gdImagePtr im;

	colorpointer = calloc(3, sizeof(int));
//      im = gdImageCreate (imagewidth, imageheight);
//      im = gdImageCreate (1600, 1200);
	im = gdImageCreateTrueColor (picx, picy);

	//Truecolor images are always filled with black at creation time...
//        white = gdImageColorAllocate (im, 255, 255, 255);
	//      black = gdImageColorAllocate (im, 0, 0, 0);
	//    red = gdImageColorAllocate (im, 255, 0, 0);

	// set white background (initially black when using gdImageCreateTrueColor)
//        gdImageFilledRectangle(im, 0, 0, mapinfo.imagewidth, mapinfo.imageheight, white);
	gdImageFilledRectangle (im, 0, 0, picx, picy, blue);

	k = 0;
	maxstddev = 0;
	valmin = 1e9;
	valmax = 0.0;
	for (k = 0; k < picx* picy; k++) {
		retval = read (fd, buffer, blocksize);
//		printf("%d\n", retval);
//		doubletmp = print_block (&buffer[0]);
//              handle("read", retval < 0);
		if (retval <= 0) {
			printf ("\nread failed or EOF.                           \n");
//			goto write;
			break;
		}

//		check_block (buffer, blocksize, &doubletmp, &stddev);
//		if(stddev > maxstddev) maxstddev = stddev;
//		getcolor(colorpointer,  stddev, 1, 0, 100); //use block stddev

		lzo1x_1_compress(buffer, inlen, cbuffer, &outlen, wrkmem);			
		printf("%d compressed %d to %d, ratio %.3f          \r", k, inlen, outlen, (double) outlen/inlen);
		data[k] = (double) outlen/inlen;
		if(data[k] < valmin) valmin = data[k];
		if(data[k] > valmax) valmax = data[k];
	}

//      write:


	k = 0;
	for (j = 0; j < picy; j++) {
		for (i = 0; i < picx; i++) {
			getcolor(colorpointer,  data[k], 0, valmin, valmax); //colormap 0 b/w

//			getcolor(colorpointer, (short int) (100.0*stddev), 1, 0, 25500); //use block stddev
//			getcolor(colorpointer, (short int) (100.0*doubletmp), 1, 0, 25500); //use block mean value
			//color1 = gdTrueColor ((int) doubletmp, (int) doubletmp, (int) doubletmp);
			color1 = gdTrueColor (colorpointer[0], colorpointer[1], colorpointer[2]);
			gdImageSetPixel (im, i, j, color1);
			k++;

		}
	}


//      gdImageLine (im, x1, y1, x2, y2, color2);
//      gdImageSetPixel(im, x1, y1, color2);

// Now write the image and then clean up.
	if (1) {		// PNG
		if(3 == argc){
			sprintf(filename, "%s-stickpic0.png", argv[2]);
			out = fopen(filename, "wb");
		} else {
			out = fopen ("stickpic0.png", "wb");
		}
		gdImagePng (im, out);
		fclose (out);
	} else {		// JPG
		// Write JPEG using quality 90%
		// in interlaced (progressive) mode, which is better for modem connections
		out = fopen ("stickpic0.jpg", "wb");
		gdImageInterlace (im, 1);
		jpegquality = 90;
		gdImageJpeg (im, out, jpegquality);
		fclose (out);
	}
	k = 0;
	for (j = 0; j < picy; j++) {
		for (i = 0; i < picx; i++) {
			getcolor(colorpointer,  data[k], 1, valmin, valmax); //colormap hot

//			getcolor(colorpointer, (short int) (100.0*stddev), 1, 0, 25500); //use block stddev
//			getcolor(colorpointer, (short int) (100.0*doubletmp), 1, 0, 25500); //use block mean value
			//color1 = gdTrueColor ((int) doubletmp, (int) doubletmp, (int) doubletmp);
			color1 = gdTrueColor (colorpointer[0], colorpointer[1], colorpointer[2]);
			gdImageSetPixel (im, i, j, color1);
			k++;

		}
	}


//      gdImageLine (im, x1, y1, x2, y2, color2);
//      gdImageSetPixel(im, x1, y1, color2);

// Now write the image and then clean up.
	if (1) {		// PNG
		if(3 == argc){
			sprintf(filename, "%s-stickpic1.png", argv[2]);
			out = fopen(filename, "wb");
		} else {
			out = fopen ("stickpic1.png", "wb");
		}
		gdImagePng (im, out);
		fclose (out);
	} else {		// JPG
		// Write JPEG using quality 90%
		// in interlaced (progressive) mode, which is better for modem connections
		out = fopen ("stickpic1.jpg", "wb");
		gdImageInterlace (im, 1);
		jpegquality = 90;
		gdImageJpeg (im, out, jpegquality);
		fclose (out);
	}
	k = 0;
	for (j = 0; j < picy; j++) {
		for (i = 0; i < picx; i++) {
			getcolor(colorpointer,  data[k], 2, valmin, valmax); //use block stddev

//			getcolor(colorpointer, (short int) (100.0*stddev), 1, 0, 25500); //use block stddev
//			getcolor(colorpointer, (short int) (100.0*doubletmp), 1, 0, 25500); //use block mean value
			//color1 = gdTrueColor ((int) doubletmp, (int) doubletmp, (int) doubletmp);
			color1 = gdTrueColor (colorpointer[0], colorpointer[1], colorpointer[2]);
			gdImageSetPixel (im, i, j, color1);
			k++;

		}
	}


//      gdImageLine (im, x1, y1, x2, y2, color2);
//      gdImageSetPixel(im, x1, y1, color2);

// Now write the image and then clean up.
	if (1) {		// PNG
		if(3 == argc){
			sprintf(filename, "%s-stickpic2.png", argv[2]);
			out = fopen(filename, "wb");
		} else {
			out = fopen ("stickpic2.png", "wb");
		}
		gdImagePng (im, out);
		fclose (out);
	} else {		// JPG
		// Write JPEG using quality 90%
		// in interlaced (progressive) mode, which is better for modem connections
		out = fopen ("stickpic2.jpg", "wb");
		gdImageInterlace (im, 1);
		jpegquality = 90;
		gdImageJpeg (im, out, jpegquality);
		fclose (out);
	}
	printf("maxstddev = %g\n", maxstddev);
// Clean up
	gdImageDestroy (im);

	if(3 == argc){
		sprintf(filename, "%s-data.txt", argv[2]);
		f = fopen(filename, "wb");
	} else {
		f = fopen("dataout.txt", "wb");
	}
	for(k=0;k< picx*picy;k++){
		fprintf(f, "%f ", data[k]);
	}
	fprintf(f, "\n");
	fclose(f);

	return;

}

// compliant filesize function from
//https://www.securecoding.cert.org/confluence/display/seccode/FIO19-C.+Do+not+use+fseek%28%29+and+ftell%28%29+to+compute+the+size+of+a+file
/*
FILE *fp;
long file_size;
char *buffer;
struct stat stbuf;
int fd;

fd = open("foo.bin", O_RDONLY);
if (fd == -1) {
  // Handle Error 
}

fp = fdopen(fd, "rb");
if (fp == NULL) {
  // Handle Error 
}

if (fstat(fd, &stbuf) == -1) {
  // Handle Error 
}

file_size = stbuf.st_size;

buffer = (char*)malloc(file_size);
if (buffer == NULL) {
  // Handle Error 
}
*/

void
getcolor (int *colorpointer, double value, int colormap, double minValue, double maxValue)
{
	int color = 0;
	int r = 0;
	int g = 0;
	int b = 0;
	int select = 1;
	double tmp = value - minValue;
	//cout << "Landscape::getcolor()  min_h = " << min_h << ", max_h = " << max_h << endl;
	//printf("x%d, %d, %d\n", colorpointer[0], colorpointer[1], colorpointer[2]);
	if (maxValue - minValue == 0) {	// Avoid floating point exception (div by zero)
		//printf ("Will be single colour. Probably ocean. Exiting.\n");
		//exit (1);
		colorpointer[0] = 255;
		colorpointer[1] = 255;
		colorpointer[2] = 255;
		return;
	}
	if(colormap == 0){ // b/w
		color = (int) (255 * tmp / (maxValue - minValue));
		if (color > 255) {
			color = 255;
		}
		r = color;
		g = color;
		b = color;
	}
	if(colormap == 1){ //density ("hot")
		color = (int) (3 * 255 * tmp /(maxValue - minValue));
//		if (color > 3*255) {
//			color = 3*255;
//		}

		if (color > 3 * 255) {	// what should we do when exceeding the colormap?
			select = 5;	// '4' makes it blue (very obnoxious), '5' unsuspiciously white.
			goto switchlabel_1;
		}
		if (color > 2 * 255) {
			select = 3;
			goto switchlabel_1;
		}
		if (color > 255) {
			select = 2;
			goto switchlabel_1;
		}
		select = 1;
		goto switchlabel_1;

	switchlabel_1:
		switch (select) {
		case 5:
			{
				r = 255;	//make it uniform WHITE, e.g. when exceeding limit...
				g = 255;
				b = 255;
				break;
			}
		case 4:
			{
				r = 0;	//make it uniform blue, e.g. when exceeding limit...
				g = 0;
				b = 255;
				break;
			}
		case 3:
			{
				r = 255;
				g = 255;
				b = color - 2 * 255;
				break;
			}
		case 2:
			{
				r = 255;
				g = color - 255;
				b = 0;
				break;
			}
		case 1:
			{
				r = color;
				g = 0;
				b = 0;
				break;
			}
		}	//switch

	}
	if(colormap == 2){ // ("jet")
		color = (int) ((4 * 256 -1) * tmp /(maxValue - minValue));
		if (color > 4*256 - 1) {
//			color = 4*256 - 1;
			select = 6;
			goto switchlabel_2;
		}

		if (color > 3 * 256 + 127) {
			select = 5;
			goto switchlabel_2;
		}
		if (color > 2 * 256 + 127) {
			select = 4;
			goto switchlabel_2;
		}
		if (color > 256 + 127) {
			select = 3;
			goto switchlabel_2;
		}
		if (color > 127) {
			select = 2;
			goto switchlabel_2;
		}
		select = 1;
		goto switchlabel_2;

	switchlabel_2:
		switch (select) {
		case 6:
			{
				r = 255;
				g = 255;
				b = 255;
				break;
			}
		case 5:
			{
				r = 255 - color + 3*256 + 128;
				g = 0;
				b = 0;
				break;
			}
		case 4:
			{
				r = 255;
				g = 255 - color + 2*256 + 128;
				b = 0;
				break;
			}
		case 3:
			{
				r = color - 256 - 128;
				g = 255;
				b = 255 - color + 256 + 128;
				break;
			}
		case 2:
			{
				r = 0;
				g = color - 128;
				b = 255;
				break;
			}
		case 1:
			{
				r = 0;
				g = 0;
				b = 128 + color;
				break;
			}
		}	//switch

	}
	colorpointer[0] = r;
	colorpointer[1] = g;
	colorpointer[2] = b;
}
