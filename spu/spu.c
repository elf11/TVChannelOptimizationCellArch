#include <spu_mfcio.h>
#include <stdio.h>
#include <libmisc.h>
#include <string.h>

#include "../common.h"

#define MY_TAG 5
#define DONE 1
#define NUM_ELEMENTS 8

#define waitag(t) mfc_write_tag_mask(1<<t); mfc_read_tag_status_all();
uint32_t tag_id[2];

/*
 * We use the same "strategy" as in the 2 lines solution, now we've got 8 lines
 * we must return with the mfc_putl function.
 */

void process_image_dmalist(struct image* img){
	unsigned char *input;
	unsigned int addr1, addr2, i, j, k;
	unsigned int r1, r2, r3, r4, r5, r6, r7, r8, g1, g2, g3, g4, g5, g6, g7, g8, b1, b2, b3, b4, b5, b6, b7, b8;
	unsigned char *out1, *out2, *out3, *out4, *out5, *out6, *out7, *out8;
	unsigned char *t1, *t2, *t3, *t4, *t5, *t6, *t7, *t8;
	int block_nr = img->block_nr;
	vector unsigned char *v1[8], *v2[8], *v3[8], *v4[8], *v5[8];


	// we need 2 dma lists, one of them we use for the input data
	// the other list we use for the output data, we get 4 elements in
	// the input dma-list and 8 elements in the output dma-list
	struct mfc_list_element dma_list_get[4] __attribute__((aligned(16)));
	struct mfc_list_element dma_list_put[8] __attribute__((aligned(16)));

	input = malloc_align(8 * NUM_CHANNELS * SCALE_FACTOR * img->width, 4);

	v1[0] = (vector unsigned char *) &input[0];
	v2[0] = (vector unsigned char *) &input[1 * img->width * NUM_CHANNELS];
	v3[0] = (vector unsigned char *) &input[2 * img->width * NUM_CHANNELS];
	v4[0] = (vector unsigned char *) &input[3 * img->width * NUM_CHANNELS];

	for (i = 0; i < 7; i += 1)
	{
		v1[i+1] = v1[i] + ((img->width * NUM_CHANNELS) / SCALE_FACTOR);
		v2[i+1] = v2[i] + ((img->width * NUM_CHANNELS) / SCALE_FACTOR);
		v3[i+1] = v3[i] + ((img->width * NUM_CHANNELS) / SCALE_FACTOR);
		v4[i+1] = v4[i] + ((img->width * NUM_CHANNELS) / SCALE_FACTOR);
	}

	out1 = malloc_align(8 * NUM_CHANNELS * img->width / SCALE_FACTOR, 16); // the output vector is made out of 8 rows and the distance between the rows is equal to the actual image
	// width * NUM_CHANNELS (a line in the actual image) divided by the scale factor
	out2 = out1 + ((img->width * NUM_CHANNELS) / SCALE_FACTOR);
	out3 = out2 + ((img->width * NUM_CHANNELS) / SCALE_FACTOR);
	out4 = out3 + ((img->width * NUM_CHANNELS) / SCALE_FACTOR);
	out5 = out4 + ((img->width * NUM_CHANNELS) / SCALE_FACTOR);
	out6 = out5 + ((img->width * NUM_CHANNELS) / SCALE_FACTOR);
	out7 = out6 + ((img->width * NUM_CHANNELS) / SCALE_FACTOR);
	out8 = out7 + ((img->width * NUM_CHANNELS) / SCALE_FACTOR);

	t1 = malloc_align(8 * NUM_CHANNELS * img->width, 4); // the temporary array were we will save the result of the vectorization, it's made out of 8 lines
	// the distance between the lines is equal to the length of a line from the original image img->widht * NUM_CHANNELS
	v5[0] = (vector unsigned char *)t1;
	t2 = t1 + (img->width * NUM_CHANNELS);
	v5[1] = (vector unsigned char *)t2;
	t3 = t2 + (img->width * NUM_CHANNELS);
	v5[2] = (vector unsigned char *)t3;
	t4 = t3 + (img->width * NUM_CHANNELS);
	v5[3] = (vector unsigned char *)t4;
	t5 = t4 + (img->width * NUM_CHANNELS);
	v5[4] = (vector unsigned char *)t5;
	t6 = t5 + (img->width * NUM_CHANNELS);
	v5[5] = (vector unsigned char *)t6;
	t7 = t6 + (img->width * NUM_CHANNELS);
	v5[6] = (vector unsigned char *)t7;
	t8 = t7 + (img->width * NUM_CHANNELS);
	v5[7] = (vector unsigned char *)t8;


	addr2 = (unsigned int)img->dst; //start of image
	addr2 += (block_nr / NUM_IMAGES_HEIGHT) * img->width * NUM_CHANNELS * 
		img->height / NUM_IMAGES_HEIGHT; //start line of spu block
	addr2 += (block_nr % NUM_IMAGES_WIDTH) * NUM_CHANNELS *
		img->width / NUM_IMAGES_WIDTH;

	for (i=0; i<img->height / (8 * SCALE_FACTOR); i++){
		//get 32 lines
		addr1 = ((unsigned int)img->src) + 8 * i * img->width * NUM_CHANNELS * SCALE_FACTOR;

		for (j = 0; j < 4; j ++)
		{
			dma_list_get[j].notify = 0;
			dma_list_get[j].size = 2 * img->width * NUM_CHANNELS * SCALE_FACTOR;
			dma_list_get[j].eal = addr1 + 2 * j * img->width * NUM_CHANNELS * SCALE_FACTOR;
		}

		mfc_getl(input, 0, dma_list_get, 8 * 4, MY_TAG, 0, 0); //get a list with 4 elements and each elements has 8 more elements
		mfc_write_tag_mask(1 << MY_TAG);
		mfc_read_tag_status_all();

		//compute the scaled line
		for (j = 0; j < img->width * NUM_CHANNELS / 16; j++){
			// for each of the 8 lines apply the same formula as for 1 line - used in the simple algorithm
			for (k = 0; k < 8; k ++)
			{
				v5[k][j] = spu_avg(spu_avg(v1[k][j], v2[k][j]), spu_avg(v3[k][j], v4[k][j]));
			}
		}

		//set up the output values
		for (j=0; j < img->width; j+=SCALE_FACTOR){
			r1 = g1 = b1 = 0;
			r2 = g2 = b2 = 0;
			r3 = g3 = b3 = 0;
			r4 = g4 = b4 = 0;
			r5 = g5 = b5 = 0;
			r6 = g6 = b6 = 0;
			r7 = g7 = b7 = 0;
			r8 = g8 = b8 = 0;

			for (k = j; k < j + SCALE_FACTOR; k++) {
				r1 += t1[k * NUM_CHANNELS + 0];
				g1 += t1[k * NUM_CHANNELS + 1];
				b1 += t1[k * NUM_CHANNELS + 2];

				r2 += t2[k * NUM_CHANNELS + 0];
				g2 += t2[k * NUM_CHANNELS + 1];
				b2 += t2[k * NUM_CHANNELS + 2];

				r3 += t3[k * NUM_CHANNELS + 0];
				g3 += t3[k * NUM_CHANNELS + 1];
				b3 += t3[k * NUM_CHANNELS + 2];

				r4 += t4[k * NUM_CHANNELS + 0];
				g4 += t4[k * NUM_CHANNELS + 1];
				b4 += t4[k * NUM_CHANNELS + 2];

				r5 += t5[k * NUM_CHANNELS + 0];
				g5 += t5[k * NUM_CHANNELS + 1];
				b5 += t5[k * NUM_CHANNELS + 2];

				r6 += t6[k * NUM_CHANNELS + 0];
				g6 += t6[k * NUM_CHANNELS + 1];
				b6 += t6[k * NUM_CHANNELS + 2];

				r7 += t7[k * NUM_CHANNELS + 0];
				g7 += t7[k * NUM_CHANNELS + 1];
				b7 += t7[k * NUM_CHANNELS + 2];

				r8 += t8[k * NUM_CHANNELS + 0];
				g8 += t8[k * NUM_CHANNELS + 1];
				b8 += t8[k * NUM_CHANNELS + 2];
			}
			r1 /= SCALE_FACTOR;
			b1 /= SCALE_FACTOR;
			g1 /= SCALE_FACTOR;

			out1[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r1;
			out1[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g1;
			out1[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b1;
			r2 /= SCALE_FACTOR;
			b2 /= SCALE_FACTOR;
			g2 /= SCALE_FACTOR;

			out2[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r2;
			out2[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g2;
			out2[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b2;
			r3 /= SCALE_FACTOR;
			b3 /= SCALE_FACTOR;
			g3 /= SCALE_FACTOR;

			out3[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r3;
			out3[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g3;
			out3[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b3;
			r4 /= SCALE_FACTOR;
			b4 /= SCALE_FACTOR;
			g4 /= SCALE_FACTOR;

			out4[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r4;
			out4[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g4;
			out4[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b4;
			r5 /= SCALE_FACTOR;
			b5 /= SCALE_FACTOR;
			g5 /= SCALE_FACTOR;

			out5[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r5;
			out5[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g5;
			out5[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b5;
			r6 /= SCALE_FACTOR;
			b6 /= SCALE_FACTOR;
			g6 /= SCALE_FACTOR;

			out6[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r6;
			out6[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g6;
			out6[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b6;
			r7 /= SCALE_FACTOR;
			b7 /= SCALE_FACTOR;
			g7 /= SCALE_FACTOR;

			out7[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r7;
			out7[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g7;
			out7[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b7;
			r8 /= SCALE_FACTOR;
			b8 /= SCALE_FACTOR;
			g8 /= SCALE_FACTOR;

			out8[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r8;
			out8[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g8;
			out8[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b8;
		}

		for (j = 0; j < 8; j ++)
		{
			dma_list_put[j].notify = 0;
			dma_list_put[j].size = (img->width * NUM_CHANNELS) / SCALE_FACTOR; // size of a scaled line that we must put back it's equal to size of a initial line divided by the scale factor
			dma_list_put[j].eal = addr2 + j * img->width * NUM_CHANNELS;
		}

		//put the scaled line back
		mfc_putl(out1, 0, dma_list_put, 8 * 8, MY_TAG, 0, 0);
		addr2 += 8 * img->width * NUM_CHANNELS; //line inside spu block
		mfc_write_tag_mask(1 << MY_TAG);
		mfc_read_tag_status_all();
	}

	free_align(t1);
	free_align(input);
	free_align(out1);

}

void process_image_double(struct image* img){
	unsigned char *input[2], *output, *temp;
	unsigned int addr1, addr2, i, j, k, r, g, b;
	int block_nr = img->block_nr;
	int buf, nxt_buf; 
	vector unsigned char *v1[2], *v2[2], *v3[2], *v4[2], *v5 ;


	input[0] = malloc_align(NUM_CHANNELS * SCALE_FACTOR * img->width, 4);
	input[1] = malloc_align(NUM_CHANNELS * SCALE_FACTOR * img->width, 4);
	output = malloc_align(NUM_CHANNELS * img->width / SCALE_FACTOR, 4);
	temp = malloc_align(NUM_CHANNELS * img->width, 4);

	v1[0] = (vector unsigned char *) &(input[0][0]);
	v2[0] = (vector unsigned char *) &(input[0][1 * img->width * NUM_CHANNELS]);
	v3[0] = (vector unsigned char *) &(input[0][2 * img->width * NUM_CHANNELS]);
	v4[0] = (vector unsigned char *) &(input[0][3 * img->width * NUM_CHANNELS]);

	v1[1] = (vector unsigned char *) &(input[1][0]);
	v2[1] = (vector unsigned char *) &(input[1][1 * img->width * NUM_CHANNELS]);
	v3[1] = (vector unsigned char *) &(input[1][2 * img->width * NUM_CHANNELS]);
	v4[1] = (vector unsigned char *) &(input[1][3 * img->width * NUM_CHANNELS]);

	v5 = (vector unsigned char *) temp;

	addr2 = (unsigned int)img->dst; //start of image
	addr2 += (block_nr / NUM_IMAGES_HEIGHT) * img->width * NUM_CHANNELS * 
		img->height / NUM_IMAGES_HEIGHT; //start line of spu block
	addr2 += (block_nr % NUM_IMAGES_WIDTH) * NUM_CHANNELS *
		img->width / NUM_IMAGES_WIDTH;

	buf = 0;

	addr1 = ((unsigned int)img->src);
	//get data for the first buffer
	mfc_get(input[buf], addr1, SCALE_FACTOR * img->width * NUM_CHANNELS, tag_id[buf], 0, 0);

	for (i=1; i<img->height / SCALE_FACTOR; i++){
		//get 4 lines - data for the next buffer
		nxt_buf = buf^1;
		addr1 = ((unsigned int)img->src) + i * img->width * NUM_CHANNELS * SCALE_FACTOR;
		mfc_get(input[nxt_buf], addr1, SCALE_FACTOR * img->width * NUM_CHANNELS, tag_id[nxt_buf], 0, 0);
		
		waitag(tag_id[buf]);

	
		for (j = 0; j < img->width * NUM_CHANNELS / 16; j++){
			v5[j] = spu_avg(spu_avg(v1[buf][j], v2[buf][j]), spu_avg(v3[buf][j], v4[buf][j]));
		}
		for (j=0; j < img->width; j+=SCALE_FACTOR){
			r = g = b = 0;
			for (k = j; k < j + SCALE_FACTOR; k++) {
				r += temp[k * NUM_CHANNELS + 0];
				g += temp[k * NUM_CHANNELS + 1];
				b += temp[k * NUM_CHANNELS + 2];
			}
			r /= SCALE_FACTOR;
			b /= SCALE_FACTOR;
			g /= SCALE_FACTOR;

			output[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r;
			output[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g;
			output[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b;
		}

		//put the scaled line back
		mfc_put(output, addr2, img->width / SCALE_FACTOR * NUM_CHANNELS, MY_TAG, 0, 0);
		addr2 += img->width * NUM_CHANNELS; //line inside spu block
		// switch buffers
		buf = nxt_buf;
		mfc_write_tag_mask(1 << MY_TAG);
		mfc_read_tag_status_all();
	}

	waitag(tag_id[buf]);

	for (j = 0; j < img->width * NUM_CHANNELS / 16; j++){
		v5[j] = spu_avg(spu_avg(v1[buf][j], v2[buf][j]), spu_avg(v3[buf][j], v4[buf][j]));
		}
		
	for (j=0; j < img->width; j+=SCALE_FACTOR){
		r = g = b = 0;
		for (k = j; k < j + SCALE_FACTOR; k++) {
			r += temp[k * NUM_CHANNELS + 0];
			g += temp[k * NUM_CHANNELS + 1];
			b += temp[k * NUM_CHANNELS + 2];
		}
		r /= SCALE_FACTOR;
		b /= SCALE_FACTOR;
		g /= SCALE_FACTOR;

		output[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r;
		output[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g;
		output[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b;
	}
	//put the scaled line back
	mfc_put(output, addr2, img->width / SCALE_FACTOR * NUM_CHANNELS, MY_TAG, 0, 0);
	
	mfc_write_tag_mask(1 << MY_TAG);
	mfc_read_tag_status_all();

	waitag(tag_id[buf]);
	free_align(temp);
	free_align(input[0]);
	free_align(input[1]);
	free_align(output);
}

void process_image_2lines(struct image* img){
	unsigned char *input, *output1, *temp1, *output2, *temp2;
	unsigned int addr1, addr2, i, j, k, r, g, b, r1, g1, b1;
	int block_nr = img->block_nr;
	vector unsigned char *v1, *v2, *v3, *v4, *v5,*v11, *v12, *v13, *v14,*v15 ;

	input = malloc_align(2 * NUM_CHANNELS * SCALE_FACTOR * img->width, 4);
	output1 = malloc_align(NUM_CHANNELS * img->width / SCALE_FACTOR, 4);
	output2 = malloc_align(NUM_CHANNELS * img->width / SCALE_FACTOR, 4);
	temp1 = malloc_align(NUM_CHANNELS * img->width, 4);
	temp2 = malloc_align(NUM_CHANNELS * img->width, 4);

	v1 = (vector unsigned char *) &input[0];
	v2 = (vector unsigned char *) &input[1 * img->width * NUM_CHANNELS];
	v3 = (vector unsigned char *) &input[2 * img->width * NUM_CHANNELS];
	v4 = (vector unsigned char *) &input[3 * img->width * NUM_CHANNELS];
	v11 = (vector unsigned char *) &input[4 * img->width * NUM_CHANNELS];
	v12 = (vector unsigned char *) &input[5 * img->width * NUM_CHANNELS];
	v13 = (vector unsigned char *) &input[6 * img->width * NUM_CHANNELS];
	v14 = (vector unsigned char *) &input[7 * img->width * NUM_CHANNELS];

	v5 = (vector unsigned char *) temp1;
	v15 = (vector unsigned char *) temp2;


	addr2 = (unsigned int)img->dst; //start of image
	addr2 += (block_nr / NUM_IMAGES_HEIGHT) * img->width * NUM_CHANNELS * 
		img->height / NUM_IMAGES_HEIGHT; //start line of spu block
	addr2 += (block_nr % NUM_IMAGES_WIDTH) * NUM_CHANNELS *
		img->width / NUM_IMAGES_WIDTH;

	for (i=0; i<img->height / (SCALE_FACTOR*2); i++){
		//get 8 lines
		addr1 = ((unsigned int)img->src) + i * img->width * NUM_CHANNELS*2 * SCALE_FACTOR;
		mfc_get(input, addr1, SCALE_FACTOR * img->width * NUM_CHANNELS *2, MY_TAG, 0, 0);
		mfc_write_tag_mask(1 << MY_TAG);
		mfc_read_tag_status_all();

		//compute the scaled line
		for (j = 0; j < img->width * NUM_CHANNELS / 16; j++){
			v5[j] = spu_avg(spu_avg(v1[j], v2[j]), spu_avg(v3[j], v4[j]));
			v15[j] = spu_avg(spu_avg(v11[j], v12[j]), spu_avg(v13[j], v14[j]));
		}
		for (j=0; j < img->width; j+=SCALE_FACTOR){
			r = g = b = 0;
			r1 = g1 = b1 = 0;
			for (k = j; k < j + SCALE_FACTOR; k++) {
				r += temp1[k * NUM_CHANNELS + 0];
				g += temp1[k * NUM_CHANNELS + 1];
				b += temp1[k * NUM_CHANNELS + 2];
				r1 += temp2[k * NUM_CHANNELS + 0];
				g1 += temp2[k * NUM_CHANNELS + 1];
				b1 += temp2[k * NUM_CHANNELS + 2];
			}
			r /= SCALE_FACTOR;
			b /= SCALE_FACTOR;
			g /= SCALE_FACTOR;

			r1 /= SCALE_FACTOR;
			b1 /= SCALE_FACTOR;
			g1 /= SCALE_FACTOR;

			output1[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r;
			output1[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g;
			output1[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b;
			output2[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r1;
			output2[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g1;
			output2[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b1;
		}


		//put the scaled line back
		mfc_put(output1, addr2, img->width / SCALE_FACTOR * NUM_CHANNELS, MY_TAG, 0, 0);
		addr2 += img->width * NUM_CHANNELS; //line inside spu block
		mfc_write_tag_mask(1 << MY_TAG);
		
		mfc_put(output2, addr2, img->width / SCALE_FACTOR * NUM_CHANNELS, MY_TAG, 0, 0);
		addr2 += img->width * NUM_CHANNELS; //line inside spu block

		mfc_write_tag_mask(1 << MY_TAG);
		mfc_read_tag_status_all();
	}

	free_align(temp2);
	free_align(output2);
	free_align(temp1);
	free_align(input);
	free_align(output1);
}

void process_image_simple(struct image* img){
	unsigned char *input, *output, *temp;
	unsigned int addr1, addr2, i, j, k, r, g, b;
	int block_nr = img->block_nr;
	vector unsigned char *v1, *v2, *v3, *v4, *v5 ;

	input = malloc_align(NUM_CHANNELS * SCALE_FACTOR * img->width, 4);
	output = malloc_align(NUM_CHANNELS * img->width / SCALE_FACTOR, 4);
	temp = malloc_align(NUM_CHANNELS * img->width, 4);

	v1 = (vector unsigned char *) &input[0];
	v2 = (vector unsigned char *) &input[1 * img->width * NUM_CHANNELS];
	v3 = (vector unsigned char *) &input[2 * img->width * NUM_CHANNELS];
	v4 = (vector unsigned char *) &input[3 * img->width * NUM_CHANNELS];
	v5 = (vector unsigned char *) temp;

	addr2 = (unsigned int)img->dst; //start of image
	addr2 += (block_nr / NUM_IMAGES_HEIGHT) * img->width * NUM_CHANNELS * 
		img->height / NUM_IMAGES_HEIGHT; //start line of spu block
	addr2 += (block_nr % NUM_IMAGES_WIDTH) * NUM_CHANNELS *
		img->width / NUM_IMAGES_WIDTH;

	for (i=0; i<img->height / SCALE_FACTOR; i++){
		//get 4 lines
		addr1 = ((unsigned int)img->src) + i * img->width * NUM_CHANNELS * SCALE_FACTOR;
		mfc_get(input, addr1, SCALE_FACTOR * img->width * NUM_CHANNELS, MY_TAG, 0, 0);
		mfc_write_tag_mask(1 << MY_TAG);
		mfc_read_tag_status_all();

		//compute the scaled line
		for (j = 0; j < img->width * NUM_CHANNELS / 16; j++){
			v5[j] = spu_avg(spu_avg(v1[j], v2[j]), spu_avg(v3[j], v4[j]));
		}
		for (j=0; j < img->width; j+=SCALE_FACTOR){
			r = g = b = 0;
			for (k = j; k < j + SCALE_FACTOR; k++) {
				r += temp[k * NUM_CHANNELS + 0];
				g += temp[k * NUM_CHANNELS + 1];
				b += temp[k * NUM_CHANNELS + 2];
			}
			r /= SCALE_FACTOR;
			b /= SCALE_FACTOR;
			g /= SCALE_FACTOR;

			output[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r;
			output[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g;
			output[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b;
		}

		//put the scaled line back
		mfc_put(output, addr2, img->width / SCALE_FACTOR * NUM_CHANNELS, MY_TAG, 0, 0);
		addr2 += img->width * NUM_CHANNELS; //line inside spu block
		mfc_write_tag_mask(1 << MY_TAG);
		mfc_read_tag_status_all();
	}

	free_align(temp);
	free_align(input);
	free_align(output);
}

int main(uint64_t speid, uint64_t argp, uint64_t envp){
	unsigned int data[NUM_STREAMS];
	unsigned int num_spus = (unsigned int)argp, i, num_images;
	struct image my_image __attribute__ ((aligned(16)));
	int mode = (int)envp;

	speid = speid; //get rid of warning

	//reserve a tag id for the double buffering
	tag_id[0] = mfc_tag_reserve();
	if (tag_id[0]==MFC_TAG_INVALID)
	{
		printf("SPU: ERROR can't allocate tag ID\n");
		return -1;
	}
	tag_id[1] = mfc_tag_reserve();
	if (tag_id[1]==MFC_TAG_INVALID)
	{
		printf("SPU: ERROR can't allocate tag ID\n");
		return -1;
	}

	while(1){
		num_images = 0;
		for (i = 0; i < NUM_STREAMS / num_spus; i++){
			//assume NUM_STREAMS is a multiple of num_spus
			while(spu_stat_in_mbox() == 0);
			data[i] = spu_read_in_mbox();
			if (!data[i])
				return 0;
			num_images++;
		}

		for (i = 0; i < num_images; i++){
			mfc_get(&my_image, data[i], sizeof(struct image), MY_TAG, 0, 0);
			mfc_write_tag_mask(1 << MY_TAG);
			mfc_read_tag_status_all();
			switch(mode){
				default:
				case MODE_SIMPLE:
					process_image_simple(&my_image);
					break;
				case MODE_2LINES:
					process_image_2lines(&my_image);
					break;
				case MODE_DOUBLE:
					process_image_double(&my_image);
					break;
				case MODE_DMALIST:
					process_image_dmalist(&my_image);
					break;
			}
		}	
		data[0] = DONE;
		spu_write_out_intr_mbox(data[0]);	
	}

	return 0;
}
