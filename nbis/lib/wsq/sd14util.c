/*******************************************************************************

License: 
This software and/or related materials was developed at the National Institute
of Standards and Technology (NIST) by employees of the Federal Government
in the course of their official duties. Pursuant to title 17 Section 105
of the United States Code, this software is not subject to copyright
protection and is in the public domain. 

This software and/or related materials have been determined to be not subject
to the EAR (see Part 734.3 of the EAR for exact details) because it is
a publicly available technology and software, and is freely distributed
to any interested party with no licensing requirements.  Therefore, it is 
permissible to distribute this software as a free download from the internet.

Disclaimer: 
This software and/or related materials was developed to promote biometric
standards and biometric technology testing for the Federal Government
in accordance with the USA PATRIOT Act and the Enhanced Border Security
and Visa Entry Reform Act. Specific hardware and software products identified
in this software were used in order to perform the software development.
In no case does such identification imply recommendation or endorsement
by the National Institute of Standards and Technology, nor does it imply that
the products and equipment identified are necessarily the best available
for the purpose.

This software and/or related materials are provided "AS-IS" without warranty
of any kind including NO WARRANTY OF PERFORMANCE, MERCHANTABILITY,
NO WARRANTY OF NON-INFRINGEMENT OF ANY 3RD PARTY INTELLECTUAL PROPERTY
or FITNESS FOR A PARTICULAR PURPOSE or for any purpose whatsoever, for the
licensed product, however used. In no event shall NIST be liable for any
damages and/or costs, including but not limited to incidental or consequential
damages of any kind, including economic damage or injury to property and lost
profits, regardless of whether NIST shall be advised, have reason to know,
or in fact shall know of the possibility.

By using this software, you agree to bear all risk relating to quality,
use and performance of the software and/or related materials.  You agree
to hold the Government harmless from any claim arising from your use
of the software.

*******************************************************************************/


/***********************************************************************
      LIBRARY: WSQ - Grayscale Image Compression

      FILE:    SD14UTIL.C
      AUTHOR:  Craig Watson
      DATE:    12/15/2000
      UPDATED:  02/24/2005 by MDG

      Contains routines responsible for decoding and converting an
      old image format used to WSQ-compress fingerprints in NIST
      Special Database 14.  This old format should be considered
      obsolete.

      ROUTINES:
#cat: biomeval_nbis_wsq14_decode_file - Decompresses a WSQ-compressed datastream encoded
#cat:           according to an old image format used in NIST Special
#cat:           Database 14.  This routine should be used to decompress
#cat:           legacy data only.  This old format should be considered
#cat:           obsolete.
#cat: biomeval_nbis_wsq14_2_wsq - Converts a WSQ-compressed datastream encoded according
#cat:           to an old image format used in NIST Special Database 14
#cat:           to the WSQ format compliant with the FBI's WSQ Gray-Scale
#cat:           Fingerprint Image Compression Specification.

***********************************************************************/

/***********************************************************/
/* NOTE: The routines in this file are provided to support */
/*       the decoding and converting of files exclusively  */
/*       distributed with NIST Special Database 14 (SD14). */
/*       The format of SD14 files is not compliant with    */
/*       the FBI's WSQ Gray-Scale Fingerprint Image        */
/*       Compression Specification.                        */
/*                                                         */
/*       ALL future WSQ developments should NOT use these  */
/*       routines, but rather use the FBI certifiable      */
/*       routines provided in the remainder of this        */
/*       library distribution.                             */
/***********************************************************/

#include <stdio.h>
#include <wsq.h>
#include <dataio.h>

/* Old format global trees. */
static Q_TREE biomeval_nbis_q_tree_wsq14[Q_TREELEN];
/*
static W_TREE biomeval_nbis_w_tree_wsq14[W_TREELEN];
*/

/* Prototypes for functions in this file. */
int biomeval_nbis_wsq14_decode_file(unsigned char **, int *, int *, int *, int *, FILE *);
int biomeval_nbis_wsq14_2_wsq(unsigned char **, int *, FILE *);
static int biomeval_nbis_read_table_wsq14(unsigned short, DTT_TABLE *, DQT_TABLE *,
                      DHT_TABLE *, FILE *);
static int biomeval_nbis_read_huff_table_wsq14(DHT_TABLE *, FILE *);
static void biomeval_nbis_build_wsbiomeval_nbis_q_trees_wsq14(W_TREE [], const int, Q_TREE [], const int,
                      const int, const int);
static void biomeval_nbis_build_shuffle_trees_wsq14(W_TREE [], const int, Q_TREE [],
                      const int, Q_TREE [], const int, const int, const int);
static int huffman_decode_data_file_wsq14(short *, DTT_TABLE *, DQT_TABLE *,
                      DHT_TABLE *, FILE *);
static int unbiomeval_nbis_shuffle_wsq14(short **, const DQT_TABLE *, Q_TREE [], const int,
                      short *, const int, const int);
static int biomeval_nbis_shuffle_wsq14(short **, int *, const DQT_TABLE, Q_TREE [],
                      const int, short *, const int, const int);
static int biomeval_nbis_shuffle_dqt_wsq14(DQT_TABLE *);
static void biomeval_nbis_build_w_tree_wsq14(W_TREE [], const int, const int);
static void biomeval_nbis_build_q_tree_wsq14(W_TREE [], Q_TREE []);
static void biomeval_nbis_w_tree4_wsq14(W_TREE [], int, int, int, int, int, int, int);
static void biomeval_nbis_q_tree16_wsq14(Q_TREE [], int, int, int, int, int);
static void biomeval_nbis_q_tree4_wsq14(Q_TREE [], int, int, int, int, int);


/*************************************************************************/
/* WSQ14 Decoder routine.  Takes an open WSQ14 compressed file (old      */
/* format used with SD14) and reads in the encoded data, reformats the   */
/* Wavelet tree, and then calls the certifiable WSQ decoder to           */
/* reconstruct the pixmap.  This routine should ONLY be used to read     */
/* files distributed with the SD14 database.                             */
/*************************************************************************/
int biomeval_nbis_wsq14_decode_file(unsigned char **odata, int *owidth, int *oheight,
                      int *odepth, int *lossyflag, FILE *infp)
{
   int ret;
   unsigned short marker;
   int num_pix;
   int width, height;
   unsigned char *cdata;
   float *fdata;
   short *qdata;

   /* Added by MDG on 02-24-05 */
   biomeval_nbis_init_wsq_decoder_resources();

   /* Read the SOI_WSQ marker. */
   if((ret = biomeval_nbis_read_marker_wsq(&marker, SOI_WSQ, infp))){
      biomeval_nbis_free_wsq_decoder_resources();
      return(ret);
   }

   /* Read in supporting tables up to the SOF_WSQ marker. */
   if((ret = biomeval_nbis_read_marker_wsq(&marker, TBLS_N_SOF, infp))){
      biomeval_nbis_free_wsq_decoder_resources();
      return(ret);
   }
   while(marker != SOF_WSQ) {
      if((ret = biomeval_nbis_read_table_wsq14(marker, &biomeval_nbis_dtt_table, &biomeval_nbis_dqt_table, biomeval_nbis_dht_table, infp))){
         biomeval_nbis_free_wsq_decoder_resources();
         return(ret);
      }
      if((ret = biomeval_nbis_read_marker_wsq(&marker, TBLS_N_SOF, infp))){
         biomeval_nbis_free_wsq_decoder_resources();
         return(ret);
      }
   }

   /* Read in the Frame Header. */
   if((ret = biomeval_nbis_read_frame_header_wsq(&biomeval_nbis_frm_header_wsq, infp))){
      biomeval_nbis_free_wsq_decoder_resources();
      return(ret);
   }
   width = biomeval_nbis_frm_header_wsq.width;
   height = biomeval_nbis_frm_header_wsq.height;
   num_pix = width * height;

   if(debug > 0)
      fprintf(stderr, "SOI_WSQ, tables, and frame header read\n\n");

   /* Build WSQ decomposition trees. */
   biomeval_nbis_build_wsbiomeval_nbis_q_trees_wsq14(biomeval_nbis_w_tree, W_TREELEN, biomeval_nbis_q_tree, Q_TREELEN, width, height);

   if(debug > 0)
      fprintf(stderr, "Tables for wavelet decomposition finished\n\n");

   /* Allocate working memory. */
   qdata = (short *) malloc(num_pix * sizeof(short));
   if(qdata == (short *)NULL) {
      fprintf(stderr,"ERROR: wsq_decode_1 : malloc : qdata1\n");
      biomeval_nbis_free_wsq_decoder_resources();
      return(-20);
   }

   /* Decode the Huffman encoded data blocks. */
   if((ret = huffman_decode_data_file_wsq14(qdata, &biomeval_nbis_dtt_table, &biomeval_nbis_dqt_table,
                                           biomeval_nbis_dht_table, infp))){
      free(qdata);
      biomeval_nbis_free_wsq_decoder_resources();
      return(ret);
   }

   if(debug > 0)
      fprintf(stderr,
         "Quantized WSQ subband data blocks read and Huffman decoded\n\n");

   /* Decode the biomeval_nbis_quantize wavelet subband data. */
   if((ret = unbiomeval_nbis_quantize(&fdata, &biomeval_nbis_dqt_table, biomeval_nbis_q_tree, Q_TREELEN,
                         qdata, width, height))){
      free(qdata);
      biomeval_nbis_free_wsq_decoder_resources();
      return(ret);
   }

   if(debug > 0)
      fprintf(stderr, "WSQ subband data blocks unbiomeval_nbis_quantized\n\n");

   /* Done with biomeval_nbis_quantized wavelet subband data. */
   free(qdata);

   if((ret = biomeval_nbis_wsq_reconstruct(fdata, width, height, biomeval_nbis_w_tree, W_TREELEN,
                              &biomeval_nbis_dtt_table))){
      free(fdata);
      biomeval_nbis_free_wsq_decoder_resources();
      return(ret);
   }

   if(debug > 0)
      fprintf(stderr, "WSQ reconstruction of image finished\n\n");

   cdata = (unsigned char *)malloc(num_pix * sizeof(unsigned char));
   if(cdata == (unsigned char *)NULL) {
      free(fdata);
      biomeval_nbis_free_wsq_decoder_resources();
      fprintf(stderr,"ERROR: wsq_decode_1 : malloc : cdata\n");
      return(-21);
   }

   /* Convert floating point pixels to unsigned char pixels. */
   biomeval_nbis_conv_img_2_uchar(cdata, fdata, width, height,
                      biomeval_nbis_frm_header_wsq.m_shift, biomeval_nbis_frm_header_wsq.r_scale);

   /* Done with floating point pixels. */
   free(fdata);

   /* Added by MDG on 02-24-05 */
   biomeval_nbis_free_wsq_decoder_resources();

  if(debug > 0)
      fprintf(stderr, "Doubleing point pixels converted to unsigned char\n\n");


   /* Assign reconstructed pixmap and attributes to output pointers. */
   *odata = cdata;
   *owidth = width;
   *oheight = height;
   *odepth = 8;
   *lossyflag = 1;

   /* Return normally. */
   return(0);
}


/***************************************************************************/
/* WSQ14 Converter routine.  Takes an open WSQ14 compressed file (old      */
/* format used with SD14) and converts the file format to be compatible    */
/* with an FBI certifiable WSQ decoder.                                    */
/* NOTE:  Due to image bits already lossed, the resulting data is not      */
/* certifiable, but can be successfully decoded using a certifiable        */
/* decoder.                                                                */
/***************************************************************************/
int biomeval_nbis_wsq14_2_wsq(unsigned char **odata, int *olen, FILE *infp)
{
   int ret, i;
   unsigned short marker;                 /* WSQ marker */
   int num_pix;                   /* image size and counter */
   int width, height;             /* image parameters */
   short *fdata;                  /* image pointers */
   short *qdata;                  /* image pointers */
   unsigned char *wsq_data, *huff_buf;
   int wsq_alloc, wsq_len;
   int block_sizes[2];
   unsigned char *huffbits, *huffvalues; /* huffman code parameters     */
   HUFFCODE *hufftable;          /* huffcode table              */
   int hsize, hsize1, hsize2, hsize3; /* Huffman coded blocks sizes */
   int qsize, qsize1, qsize2, qsize3; /* biomeval_nbis_quantized block sizes */


/**********************************/
/* 1. READ OLD SD14 FORMAT IN ... */
/**********************************/

   /* Read the SOI_WSQ marker. */
   if((ret = biomeval_nbis_read_marker_wsq(&marker, SOI_WSQ, infp))){
      return(ret);
   }

   /* Read in supporting tables up to the SOF_WSQ marker. */
   if((ret = biomeval_nbis_read_marker_wsq(&marker, TBLS_N_SOF, infp))){
      return(ret);
   }
   while(marker != SOF_WSQ) {
      if((ret = biomeval_nbis_read_table_wsq14(marker, &biomeval_nbis_dtt_table, &biomeval_nbis_dqt_table, biomeval_nbis_dht_table, infp))){
         return(ret);
      }
      if((ret = biomeval_nbis_read_marker_wsq(&marker, TBLS_N_SOF, infp))){
         return(ret);
      }
   }

   /* Read in the Frame Header. */
   if((ret = biomeval_nbis_read_frame_header_wsq(&biomeval_nbis_frm_header_wsq, infp))){
      return(ret);
   }
   width = biomeval_nbis_frm_header_wsq.width;
   height = biomeval_nbis_frm_header_wsq.height;
   num_pix = width * height;

   if(debug > 0)
      fprintf(stderr, "SOI_WSQ, tables, and frame header read\n\n");

   /* Build WSQ decomposition trees. */
   biomeval_nbis_build_shuffle_trees_wsq14(biomeval_nbis_w_tree, W_TREELEN, biomeval_nbis_q_tree, Q_TREELEN,
                            biomeval_nbis_q_tree_wsq14, Q_TREELEN, width, height);

   if(debug > 0)
      fprintf(stderr, "Tables for wavelet decomposition finished\n\n");

   /* Allocate working memory. */
   qdata = (short *) malloc(num_pix * sizeof(short));
   if(qdata == (short *)NULL) {
      fprintf(stderr,"ERROR: wsq_decode_1 : malloc : qdata1\n");
      return(-20);
   }

   /* Decode the Huffman encoded data blocks. */
   if((ret = huffman_decode_data_file_wsq14(qdata, &biomeval_nbis_dtt_table, &biomeval_nbis_dqt_table, biomeval_nbis_dht_table,
                                  infp))){
      free(qdata);
      return(ret);
   }

   if(debug > 0)
      fprintf(stderr,
         "Quantized WSQ subband data blocks read and Huffman decoded\n\n");

/*************************************/
/* 2. CONVERT OLD FORMATTED DATA ... */
/*************************************/

   if((ret = unbiomeval_nbis_shuffle_wsq14(&fdata, &biomeval_nbis_dqt_table, biomeval_nbis_q_tree, Q_TREELEN,
                           qdata, width, height))){
      free(qdata);
      return(ret);
   }
   free(qdata);

   if((ret = biomeval_nbis_shuffle_dqt_wsq14(&biomeval_nbis_dqt_table)))
      return(ret);

   if((ret = biomeval_nbis_shuffle_wsq14(&qdata, &qsize, biomeval_nbis_dqt_table, biomeval_nbis_q_tree_wsq14, Q_TREELEN,
                         fdata, width, height))){
      free(fdata);
      return(ret);
   }
   free(fdata);

/***********************************/
/* 3. WRITE NEW FORMATTED DATA ... */
/***********************************/

   for(i = 0; i < MAX_SUBBANDS; i++) {
      biomeval_nbis_quant_vals.qbss[i] = biomeval_nbis_dqt_table.q_bin[i];
      biomeval_nbis_quant_vals.qzbs[i] = biomeval_nbis_dqt_table.z_bin[i];
   }

   /* Compute biomeval_nbis_quantized WSQ subband block sizes */
   biomeval_nbis_quant_block_sizes(&qsize1, &qsize2, &qsize3, &biomeval_nbis_quant_vals,
                           biomeval_nbis_w_tree, W_TREELEN, biomeval_nbis_q_tree, Q_TREELEN);

   if(qsize != qsize1+qsize2+qsize3){
      fprintf(stderr,
              "ERROR : wsq_encode_1 : problem w/quantization block sizes\n");
      return(-11);
   }

   /* Allocate a WSQ-encoded output buffer.  Allocate this buffer */
   /* to be the size of the original pixmap.  If the encoded data */
   /* exceeds this buffer size, then throw an error because we do */
   /* not want our compressed data to be larger than the original */
   /* image data.                                                 */
   wsq_data = (unsigned char *)malloc(num_pix);
   if(wsq_data == (unsigned char *)NULL){
      free(qdata);
      fprintf(stderr, "ERROR : wsq_encode_1 : malloc : wsq_data\n");
      return(-12);
   }
   wsq_alloc = num_pix;
   wsq_len = 0;

   /* Add a Start Of Image (SOI_WSQ) marker to the WSQ buffer. */
   if((ret = biomeval_nbis_putc_ushort(SOI_WSQ, wsq_data, wsq_alloc, &wsq_len))){
      free(qdata);
      free(wsq_data);
      return(ret);
   }

   /* Store the Wavelet filter taps to the WSQ buffer. */
   if((ret = biomeval_nbis_putc_transform_table(biomeval_nbis_lofilt, MAX_LOFILT,
                                 biomeval_nbis_hifilt, MAX_HIFILT,
                                 wsq_data, wsq_alloc, &wsq_len))){
      free(qdata);
      free(wsq_data);
      return(ret);
   }

   /* Store the quantization parameters to the WSQ buffer. */
   if((ret = biomeval_nbis_putc_quantization_table(&biomeval_nbis_quant_vals,
                                    wsq_data, wsq_alloc, &wsq_len))){
      free(qdata);
      free(wsq_data);
      return(ret);
   }

   /* Store a frame header to the WSQ buffer. */
   if((ret = biomeval_nbis_putc_frame_header_wsq(width, height, biomeval_nbis_frm_header_wsq.m_shift, biomeval_nbis_frm_header_wsq.r_scale,
                              wsq_data, wsq_alloc, &wsq_len))){
      free(qdata);
      free(wsq_data);
      return(ret);
   }

   if(debug > 0)
      fprintf(stderr, "SOI_WSQ, tables, and frame header written\n\n");

   /* Allocate a temporary buffer for holding compressed block data.    */
   /* This buffer is allocated to the size of the original input image, */
   /* and it is "assumed" that the compressed blocks will not exceed    */
   /* this buffer size.                                                 */
   huff_buf = (unsigned char *)malloc(num_pix);
   if(huff_buf == (unsigned char *)NULL) {
      free(qdata);
      free(wsq_data);
      fprintf(stderr, "ERROR : wsq_encode_1 : malloc : huff_buf\n");
      return(-13);
   }

   /******************/
   /* ENCODE Block 1 */
   /******************/
   /* Compute Huffman table for Block 1. */
   if((ret = biomeval_nbis_gen_hufftable_wsq(&hufftable, &huffbits, &huffvalues,
                            qdata, &qsize1, 1))){
      free(qdata);
      free(wsq_data);
      free(huff_buf);
      return(ret);
   }

   /* Store Huffman table for Block 1 to WSQ buffer. */
   if((ret = biomeval_nbis_putc_huffman_table(DHT_WSQ, 0, huffbits, huffvalues,
                               wsq_data, wsq_alloc, &wsq_len))){
      free(qdata);
      free(wsq_data);
      free(huff_buf);
      free(huffbits);
      free(huffvalues);
      free(hufftable);
      return(ret);
   }
   free(huffbits);
   free(huffvalues);

   if(debug > 0)
      fprintf(stderr, "Huffman code Table 1 generated and written\n\n");

   /* Compress Block 1 data. */
   if((ret = biomeval_nbis_compress_block(huff_buf, &hsize1, qdata, qsize1,
                           MAX_HUFFCOEFF, MAX_HUFFZRUN, hufftable))){
      free(qdata);
      free(wsq_data);
      free(huff_buf);
      free(hufftable);
      return(ret);
   }
   /* Done with current Huffman table. */
   free(hufftable);

   /* Accumulate number of bytes compressed. */
   hsize = hsize1;

   /* Store Block 1's header to WSQ buffer. */
   if((ret = biomeval_nbis_putc_block_header(0, wsq_data, wsq_alloc, &wsq_len))){
      free(qdata);
      free(wsq_data);
      free(huff_buf);
      return(ret);
   }

   /* Store Block 1's compressed data to WSQ buffer. */
   if((ret = biomeval_nbis_putc_bytes(huff_buf, hsize1, wsq_data, wsq_alloc, &wsq_len))){
      free(qdata);
      free(wsq_data);
      free(huff_buf);
      return(ret);
   }

   if(debug > 0)
      fprintf(stderr, "Block 1 compressed and written\n\n");

   /******************/
   /* ENCODE Block 2 */
   /******************/
   /* Compute  Huffman table for Blocks 2 & 3. */
   block_sizes[0] = qsize2;
   block_sizes[1] = qsize3;
   if((ret = biomeval_nbis_gen_hufftable_wsq(&hufftable, &huffbits, &huffvalues,
                          qdata+qsize1, block_sizes, 2))){
      free(qdata);
      free(wsq_data);
      free(huff_buf);
      return(ret);
   }

   /* Store Huffman table for Blocks 2 & 3 to WSQ buffer. */
   if((ret = biomeval_nbis_putc_huffman_table(DHT_WSQ, 1, huffbits, huffvalues,
                               wsq_data, wsq_alloc, &wsq_len))){
      free(qdata);
      free(wsq_data);
      free(huff_buf);
      free(huffbits);
      free(huffvalues);
      free(hufftable);
      return(ret);
   }
   free(huffbits);
   free(huffvalues);

   if(debug > 0)
      fprintf(stderr, "Huffman code Table 2 generated and written\n\n");

   /* Compress Block 2 data. */
   if((ret = biomeval_nbis_compress_block(huff_buf, &hsize2, qdata+qsize1, qsize2,
                           MAX_HUFFCOEFF, MAX_HUFFZRUN, hufftable))){
      free(qdata);
      free(wsq_data);
      free(huff_buf);
      free(hufftable);
      return(ret);
   }

   /* Accumulate number of bytes compressed. */
   hsize += hsize2;

   /* Store Block 2's header to WSQ buffer. */
   if((ret = biomeval_nbis_putc_block_header(1, wsq_data, wsq_alloc, &wsq_len))){
      free(qdata);
      free(wsq_data);
      free(huff_buf);
      free(hufftable);
      return(ret);
   }

   /* Store Block 2's compressed data to WSQ buffer. */
   if((ret = biomeval_nbis_putc_bytes(huff_buf, hsize2, wsq_data, wsq_alloc, &wsq_len))){
      free(qdata);
      free(wsq_data);
      free(huff_buf);
      free(hufftable);
      return(ret);
   }

   if(debug > 0)
      fprintf(stderr, "Block 2 compressed and written\n\n");

   /******************/
   /* ENCODE Block 3 */
   /******************/
   /* Compress Block 3 data. */
   if((ret = biomeval_nbis_compress_block(huff_buf, &hsize3, qdata+qsize1+qsize2, qsize3,
                           MAX_HUFFCOEFF, MAX_HUFFZRUN, hufftable))){
      free(qdata);
      free(wsq_data);
      free(huff_buf);
      free(hufftable);
      return(ret);
   }
   /* Done with current Huffman table. */
   free(hufftable);

   /* Done with biomeval_nbis_quantized image buffer. */
   free(qdata);

   /* Accumulate number of bytes compressed. */
   hsize += hsize3;

   /* Store Block 3's header to WSQ buffer. */
   if((ret = biomeval_nbis_putc_block_header(1, wsq_data, wsq_alloc, &wsq_len))){
      free(wsq_data);
      free(huff_buf);
      return(ret);
   }

   /* Store Block 3's compressed data to WSQ buffer. */
   if((ret = biomeval_nbis_putc_bytes(huff_buf, hsize3, wsq_data, wsq_alloc, &wsq_len))){
      free(wsq_data);
      free(huff_buf);
      return(ret);
   }

   if(debug > 0)
      fprintf(stderr, "Block 3 compressed and written\n\n");

   /* Done with huffman compressing blocks, so done with buffer. */
   free(huff_buf);

   /* Add a End Of Image (EOI_WSQ) marker to the WSQ buffer. */
   if((ret = biomeval_nbis_putc_ushort(EOI_WSQ, wsq_data, wsq_alloc, &wsq_len))){
      free(wsq_data);
      return(ret);
   }

   if(debug > 0) {
      fprintf(stderr,
              "hsize1 = %d :: hsize2 = %d :: hsize3 = %d\n", hsize1, hsize2, hsize3);
      fprintf(stderr,"@ complen = %d :: ratio = %.1f\n", hsize,
                      (float)(num_pix)/(float)hsize);
   }

   *odata = wsq_data;
   *olen = wsq_len;

   /* Return normally. */
   return(0);
}


/************************************/
/* Routine to read specified table. */
/************************************/
static int biomeval_nbis_read_table_wsq14(
   unsigned short marker,         /* WSQ marker */
   DTT_TABLE *biomeval_nbis_dtt_table,  /* transform table structure */
   DQT_TABLE *biomeval_nbis_dqt_table,  /* quantization table structure */
   DHT_TABLE *biomeval_nbis_dht_table,  /* huffman table structure */
   FILE *infp)            /* input file */
{
   int ret;
   unsigned char *comment;

   switch(marker){
   case DTT_WSQ:
      if((ret = biomeval_nbis_read_transform_table(biomeval_nbis_dtt_table, infp)))
         return(ret);
      break;
   case DQT_WSQ:
      if((ret = biomeval_nbis_read_quantization_table(biomeval_nbis_dqt_table, infp)))
         return(ret);
      break;
   case DHT_WSQ:
      if((ret = biomeval_nbis_read_huff_table_wsq14(biomeval_nbis_dht_table, infp)))
         return(ret);
      break;
   case COM_WSQ:
      if((ret = biomeval_nbis_read_comment(&comment, infp)))
         return(ret);
#ifdef PRINT_COMMENT
      fprintf(stderr, "COMMENT: %s\n", comment);
#endif
      free(comment);
      break;
   default:
      fprintf(stderr,"ERROR: read_table : Invalid table defined -> {%u}\n",
              marker);
      return(-75);
   }

   return(0);
}


/************************************************/
/* Routine to read in huffman table parameters. */
/************************************************/
static int biomeval_nbis_read_huff_table_wsq14(
   DHT_TABLE *biomeval_nbis_dht_table,  /* huffman table structure */
   FILE *infp)            /* input file */
{
   int ret;
   unsigned short hdr_size;       /* header size */
   unsigned short cnt, bytes_cnt; /* counters */
   unsigned short num_hufvals;    /* number of huffvalues */
   unsigned char table;           /* huffman table indicator */
   unsigned char char_dat;

   if(debug > 0)
      fprintf(stderr, "Reading huffman table.\n");

   bytes_cnt = 0;
   if((ret = biomeval_nbis_read_ushort(&hdr_size, infp)))
      return(ret);
   bytes_cnt += 2;

   while(bytes_cnt != hdr_size) {
      if((ret = biomeval_nbis_read_byte(&table, infp)))
         return(ret);

      if(debug > 2)
         fprintf(stderr, "table = %d\n", table);

      num_hufvals = 0;
      bytes_cnt += 33;
      for(cnt = 0; cnt < 16; cnt++) {
/* WSQ14 OLD SPEC USED 16bits NEW 8bits */
         if((ret = biomeval_nbis_read_byte(&char_dat, infp)))
            return(ret);
         if((ret = biomeval_nbis_read_byte(&char_dat, infp)))
            return(ret);
         (biomeval_nbis_dht_table+table)->huffbits[cnt] = char_dat;

         if(debug > 2)
            fprintf(stderr,
                    "huffbits[%d] = %d\n",
                    cnt, (biomeval_nbis_dht_table+table)->huffbits[cnt]);

         num_hufvals += (biomeval_nbis_dht_table+table)->huffbits[cnt];
      }

      if(num_hufvals > MAX_HUFFCOUNTS_WSQ+1){
         fprintf(stderr, "ERROR : biomeval_nbis_read_huff_table_wsq14 : ");
         fprintf(stderr, "num_hufvals (%d) is larger than", num_hufvals);
         fprintf(stderr, " MAX_HUFFCOUNTS_WSQ (%d)\n", MAX_HUFFCOUNTS_WSQ+1);
         return(-2);
      }
      bytes_cnt += 2 * num_hufvals;

      for(cnt = 0; cnt < num_hufvals; cnt++) {
/* WSQ14 OLD SPEC USED 16bits NEW 8bits */
         if((ret = biomeval_nbis_read_byte(&char_dat, infp)))
            return(ret);
         if((ret = biomeval_nbis_read_byte(&char_dat, infp)))
            return(ret);
         (biomeval_nbis_dht_table+table)->huffvalues[cnt] = char_dat;

         if(debug > 2)
            fprintf(stderr,
                    "huffvalues[%d] = %d\n",
                    cnt, (biomeval_nbis_dht_table+table)->huffvalues[cnt]);

      }

      (biomeval_nbis_dht_table+table)->tabdef = 1;
   }

   if(debug > 0)
      fprintf(stderr,
              "Finished reading huffman table.\n\n");

    return(0);
}


/************************************************************************/
/* Build WSQ decomposition trees.                                       */
/************************************************************************/
static void biomeval_nbis_build_wsbiomeval_nbis_q_trees_wsq14(W_TREE biomeval_nbis_w_tree[], const int biomeval_nbis_w_treelen,
                     Q_TREE biomeval_nbis_q_tree[], const int biomeval_nbis_q_treelen,
                     const int width, const int height)
{
   int i;
   W_TREE biomeval_nbis_w_tree_wsq14[W_TREELEN];

   /* This builds the old format of trees. */
   /* Build a W-TREE structure for the image. */
   biomeval_nbis_build_w_tree_wsq14(biomeval_nbis_w_tree_wsq14, width, height);
   /* Build a Q-TREE structure for the image. */
   biomeval_nbis_build_q_tree_wsq14(biomeval_nbis_w_tree_wsq14, biomeval_nbis_q_tree);

   /* This builds the new certifiable trees. */
   for(i = 0; i < biomeval_nbis_w_treelen; i++) {
      biomeval_nbis_w_tree[i].x = biomeval_nbis_w_tree_wsq14[i].x;
      biomeval_nbis_w_tree[i].y = biomeval_nbis_w_tree_wsq14[i].y;
      biomeval_nbis_w_tree[i].lenx = biomeval_nbis_w_tree_wsq14[i].lenx;
      biomeval_nbis_w_tree[i].leny = biomeval_nbis_w_tree_wsq14[i].leny;
      biomeval_nbis_w_tree[i].inv_rw = 0;
      biomeval_nbis_w_tree[i].inv_cl = 0;
   }
/*
      for(i = 0; i < biomeval_nbis_w_treelen; i++)
         fprintf(stderr,
         "t%d -> x = %d  y = %d : dx = %d  dy = %d : ir = %d  ic = %d\n",
         i, biomeval_nbis_w_tree[i].x, biomeval_nbis_w_tree[i].y, biomeval_nbis_w_tree[i].lenx, biomeval_nbis_w_tree[i].leny,
         biomeval_nbis_w_tree[i].inv_rw, biomeval_nbis_w_tree[i].inv_cl);
      fprintf(stderr, "\n\n");
      for(i = 0; i < NUM_SUBBANDS; i++)
         fprintf(stderr, "t%d -> x = %d  y = %d : lx = %d  ly = %d\n",
         i, biomeval_nbis_q_tree[i].x, biomeval_nbis_q_tree[i].y, biomeval_nbis_q_tree[i].lenx, biomeval_nbis_q_tree[i].leny);
      fprintf(stderr, "\n\n");
*/
}


/*********************************************************/
/* Build new format Wavelet tree, and new and old format */
/* Quantization trees.                                   */
/*********************************************************/
static void biomeval_nbis_build_shuffle_trees_wsq14(W_TREE biomeval_nbis_w_tree[], const int biomeval_nbis_w_treelen,
                     Q_TREE biomeval_nbis_q_tree[], const int biomeval_nbis_q_treelen,
                     Q_TREE biomeval_nbis_q_tree_wsq14[], const int biomeval_nbis_q_treelen_wsq14,
                     const int width, const int height)
{
   int i;

   /* These build the certifiable versions of the trees. */
   /* Build a W-TREE structure for the image. */
   biomeval_nbis_build_w_tree(biomeval_nbis_w_tree, width, height);
   /* Build a Q-TREE structure for the image. */
   biomeval_nbis_build_q_tree(biomeval_nbis_w_tree, biomeval_nbis_q_tree);

   /* These build the old format versions of the trees. */
   for(i = 0; i < 7; i++) {
      biomeval_nbis_q_tree_wsq14[i].x = biomeval_nbis_q_tree[i].x;
      biomeval_nbis_q_tree_wsq14[i].y = biomeval_nbis_q_tree[i].y;
      biomeval_nbis_q_tree_wsq14[i].lenx = biomeval_nbis_q_tree[i].lenx;
      biomeval_nbis_q_tree_wsq14[i].leny = biomeval_nbis_q_tree[i].leny;
   }

   biomeval_nbis_q_tree_wsq14[7].x = biomeval_nbis_q_tree[8].x;
   biomeval_nbis_q_tree_wsq14[7].y = biomeval_nbis_q_tree[8].y;
   biomeval_nbis_q_tree_wsq14[7].lenx = biomeval_nbis_q_tree[8].lenx;
   biomeval_nbis_q_tree_wsq14[7].leny = biomeval_nbis_q_tree[8].leny;
   biomeval_nbis_q_tree_wsq14[8].x = biomeval_nbis_q_tree[7].x;
   biomeval_nbis_q_tree_wsq14[8].y = biomeval_nbis_q_tree[7].y;
   biomeval_nbis_q_tree_wsq14[8].lenx = biomeval_nbis_q_tree[7].lenx;
   biomeval_nbis_q_tree_wsq14[8].leny = biomeval_nbis_q_tree[7].leny;
   biomeval_nbis_q_tree_wsq14[9].x = biomeval_nbis_q_tree[10].x;
   biomeval_nbis_q_tree_wsq14[9].y = biomeval_nbis_q_tree[10].y;
   biomeval_nbis_q_tree_wsq14[9].lenx = biomeval_nbis_q_tree[10].lenx;
   biomeval_nbis_q_tree_wsq14[9].leny = biomeval_nbis_q_tree[10].leny;
   biomeval_nbis_q_tree_wsq14[10].x = biomeval_nbis_q_tree[9].x;
   biomeval_nbis_q_tree_wsq14[10].y = biomeval_nbis_q_tree[9].y;
   biomeval_nbis_q_tree_wsq14[10].lenx = biomeval_nbis_q_tree[9].lenx;
   biomeval_nbis_q_tree_wsq14[10].leny = biomeval_nbis_q_tree[9].leny;

   biomeval_nbis_q_tree_wsq14[11].x = biomeval_nbis_q_tree[13].x;
   biomeval_nbis_q_tree_wsq14[11].y = biomeval_nbis_q_tree[13].y;
   biomeval_nbis_q_tree_wsq14[11].lenx = biomeval_nbis_q_tree[13].lenx;
   biomeval_nbis_q_tree_wsq14[11].leny = biomeval_nbis_q_tree[13].leny;
   biomeval_nbis_q_tree_wsq14[12].x = biomeval_nbis_q_tree[14].x;
   biomeval_nbis_q_tree_wsq14[12].y = biomeval_nbis_q_tree[14].y;
   biomeval_nbis_q_tree_wsq14[12].lenx = biomeval_nbis_q_tree[14].lenx;
   biomeval_nbis_q_tree_wsq14[12].leny = biomeval_nbis_q_tree[14].leny;
   biomeval_nbis_q_tree_wsq14[13].x = biomeval_nbis_q_tree[11].x;
   biomeval_nbis_q_tree_wsq14[13].y = biomeval_nbis_q_tree[11].y;
   biomeval_nbis_q_tree_wsq14[13].lenx = biomeval_nbis_q_tree[11].lenx;
   biomeval_nbis_q_tree_wsq14[13].leny = biomeval_nbis_q_tree[11].leny;
   biomeval_nbis_q_tree_wsq14[14].x = biomeval_nbis_q_tree[12].x;
   biomeval_nbis_q_tree_wsq14[14].y = biomeval_nbis_q_tree[12].y;
   biomeval_nbis_q_tree_wsq14[14].lenx = biomeval_nbis_q_tree[12].lenx;
   biomeval_nbis_q_tree_wsq14[14].leny = biomeval_nbis_q_tree[12].leny;

   biomeval_nbis_q_tree_wsq14[15].x = biomeval_nbis_q_tree[18].x;
   biomeval_nbis_q_tree_wsq14[15].y = biomeval_nbis_q_tree[18].y;
   biomeval_nbis_q_tree_wsq14[15].lenx = biomeval_nbis_q_tree[18].lenx;
   biomeval_nbis_q_tree_wsq14[15].leny = biomeval_nbis_q_tree[18].leny;
   biomeval_nbis_q_tree_wsq14[16].x = biomeval_nbis_q_tree[17].x;
   biomeval_nbis_q_tree_wsq14[16].y = biomeval_nbis_q_tree[17].y;
   biomeval_nbis_q_tree_wsq14[16].lenx = biomeval_nbis_q_tree[17].lenx;
   biomeval_nbis_q_tree_wsq14[16].leny = biomeval_nbis_q_tree[17].leny;
   biomeval_nbis_q_tree_wsq14[17].x = biomeval_nbis_q_tree[16].x;
   biomeval_nbis_q_tree_wsq14[17].y = biomeval_nbis_q_tree[16].y;
   biomeval_nbis_q_tree_wsq14[17].lenx = biomeval_nbis_q_tree[16].lenx;
   biomeval_nbis_q_tree_wsq14[17].leny = biomeval_nbis_q_tree[16].leny;
   biomeval_nbis_q_tree_wsq14[18].x = biomeval_nbis_q_tree[15].x;
   biomeval_nbis_q_tree_wsq14[18].y = biomeval_nbis_q_tree[15].y;
   biomeval_nbis_q_tree_wsq14[18].lenx = biomeval_nbis_q_tree[15].lenx;
   biomeval_nbis_q_tree_wsq14[18].leny = biomeval_nbis_q_tree[15].leny;

   biomeval_nbis_q_tree_wsq14[19].x = biomeval_nbis_q_tree[23].x;
   biomeval_nbis_q_tree_wsq14[19].y = biomeval_nbis_q_tree[23].y;
   biomeval_nbis_q_tree_wsq14[19].lenx = biomeval_nbis_q_tree[23].lenx;
   biomeval_nbis_q_tree_wsq14[19].leny = biomeval_nbis_q_tree[23].leny;
   biomeval_nbis_q_tree_wsq14[20].x = biomeval_nbis_q_tree[24].x;
   biomeval_nbis_q_tree_wsq14[20].y = biomeval_nbis_q_tree[24].y;
   biomeval_nbis_q_tree_wsq14[20].lenx = biomeval_nbis_q_tree[24].lenx;
   biomeval_nbis_q_tree_wsq14[20].leny = biomeval_nbis_q_tree[24].leny;
   biomeval_nbis_q_tree_wsq14[21].x = biomeval_nbis_q_tree[25].x;
   biomeval_nbis_q_tree_wsq14[21].y = biomeval_nbis_q_tree[25].y;
   biomeval_nbis_q_tree_wsq14[21].lenx = biomeval_nbis_q_tree[25].lenx;
   biomeval_nbis_q_tree_wsq14[21].leny = biomeval_nbis_q_tree[25].leny;
   biomeval_nbis_q_tree_wsq14[22].x = biomeval_nbis_q_tree[26].x;
   biomeval_nbis_q_tree_wsq14[22].y = biomeval_nbis_q_tree[26].y;
   biomeval_nbis_q_tree_wsq14[22].lenx = biomeval_nbis_q_tree[26].lenx;
   biomeval_nbis_q_tree_wsq14[22].leny = biomeval_nbis_q_tree[26].leny;

   biomeval_nbis_q_tree_wsq14[23].x = biomeval_nbis_q_tree[20].x;
   biomeval_nbis_q_tree_wsq14[23].y = biomeval_nbis_q_tree[20].y;
   biomeval_nbis_q_tree_wsq14[23].lenx = biomeval_nbis_q_tree[20].lenx;
   biomeval_nbis_q_tree_wsq14[23].leny = biomeval_nbis_q_tree[20].leny;
   biomeval_nbis_q_tree_wsq14[24].x = biomeval_nbis_q_tree[19].x;
   biomeval_nbis_q_tree_wsq14[24].y = biomeval_nbis_q_tree[19].y;
   biomeval_nbis_q_tree_wsq14[24].lenx = biomeval_nbis_q_tree[19].lenx;
   biomeval_nbis_q_tree_wsq14[24].leny = biomeval_nbis_q_tree[19].leny;
   biomeval_nbis_q_tree_wsq14[25].x = biomeval_nbis_q_tree[22].x;
   biomeval_nbis_q_tree_wsq14[25].y = biomeval_nbis_q_tree[22].y;
   biomeval_nbis_q_tree_wsq14[25].lenx = biomeval_nbis_q_tree[22].lenx;
   biomeval_nbis_q_tree_wsq14[25].leny = biomeval_nbis_q_tree[22].leny;
   biomeval_nbis_q_tree_wsq14[26].x = biomeval_nbis_q_tree[21].x;
   biomeval_nbis_q_tree_wsq14[26].y = biomeval_nbis_q_tree[21].y;
   biomeval_nbis_q_tree_wsq14[26].lenx = biomeval_nbis_q_tree[21].lenx;
   biomeval_nbis_q_tree_wsq14[26].leny = biomeval_nbis_q_tree[21].leny;

   biomeval_nbis_q_tree_wsq14[27].x = biomeval_nbis_q_tree[33].x;
   biomeval_nbis_q_tree_wsq14[27].y = biomeval_nbis_q_tree[33].y;
   biomeval_nbis_q_tree_wsq14[27].lenx = biomeval_nbis_q_tree[33].lenx;
   biomeval_nbis_q_tree_wsq14[27].leny = biomeval_nbis_q_tree[33].leny;
   biomeval_nbis_q_tree_wsq14[28].x = biomeval_nbis_q_tree[34].x;
   biomeval_nbis_q_tree_wsq14[28].y = biomeval_nbis_q_tree[34].y;
   biomeval_nbis_q_tree_wsq14[28].lenx = biomeval_nbis_q_tree[34].lenx;
   biomeval_nbis_q_tree_wsq14[28].leny = biomeval_nbis_q_tree[34].leny;
   biomeval_nbis_q_tree_wsq14[29].x = biomeval_nbis_q_tree[31].x;
   biomeval_nbis_q_tree_wsq14[29].y = biomeval_nbis_q_tree[31].y;
   biomeval_nbis_q_tree_wsq14[29].lenx = biomeval_nbis_q_tree[31].lenx;
   biomeval_nbis_q_tree_wsq14[29].leny = biomeval_nbis_q_tree[31].leny;
   biomeval_nbis_q_tree_wsq14[30].x = biomeval_nbis_q_tree[32].x;
   biomeval_nbis_q_tree_wsq14[30].y = biomeval_nbis_q_tree[32].y;
   biomeval_nbis_q_tree_wsq14[30].lenx = biomeval_nbis_q_tree[32].lenx;
   biomeval_nbis_q_tree_wsq14[30].leny = biomeval_nbis_q_tree[32].leny;

   biomeval_nbis_q_tree_wsq14[31].x = biomeval_nbis_q_tree[30].x;
   biomeval_nbis_q_tree_wsq14[31].y = biomeval_nbis_q_tree[30].y;
   biomeval_nbis_q_tree_wsq14[31].lenx = biomeval_nbis_q_tree[30].lenx;
   biomeval_nbis_q_tree_wsq14[31].leny = biomeval_nbis_q_tree[30].leny;
   biomeval_nbis_q_tree_wsq14[32].x = biomeval_nbis_q_tree[29].x;
   biomeval_nbis_q_tree_wsq14[32].y = biomeval_nbis_q_tree[29].y;
   biomeval_nbis_q_tree_wsq14[32].lenx = biomeval_nbis_q_tree[29].lenx;
   biomeval_nbis_q_tree_wsq14[32].leny = biomeval_nbis_q_tree[29].leny;
   biomeval_nbis_q_tree_wsq14[33].x = biomeval_nbis_q_tree[28].x;
   biomeval_nbis_q_tree_wsq14[33].y = biomeval_nbis_q_tree[28].y;
   biomeval_nbis_q_tree_wsq14[33].lenx = biomeval_nbis_q_tree[28].lenx;
   biomeval_nbis_q_tree_wsq14[33].leny = biomeval_nbis_q_tree[28].leny;
   biomeval_nbis_q_tree_wsq14[34].x = biomeval_nbis_q_tree[27].x;
   biomeval_nbis_q_tree_wsq14[34].y = biomeval_nbis_q_tree[27].y;
   biomeval_nbis_q_tree_wsq14[34].lenx = biomeval_nbis_q_tree[27].lenx;
   biomeval_nbis_q_tree_wsq14[34].leny = biomeval_nbis_q_tree[27].leny;

   biomeval_nbis_q_tree_wsq14[35].x = biomeval_nbis_q_tree[43].x;
   biomeval_nbis_q_tree_wsq14[35].y = biomeval_nbis_q_tree[43].y;
   biomeval_nbis_q_tree_wsq14[35].lenx = biomeval_nbis_q_tree[43].lenx;
   biomeval_nbis_q_tree_wsq14[35].leny = biomeval_nbis_q_tree[43].leny;
   biomeval_nbis_q_tree_wsq14[36].x = biomeval_nbis_q_tree[44].x;
   biomeval_nbis_q_tree_wsq14[36].y = biomeval_nbis_q_tree[44].y;
   biomeval_nbis_q_tree_wsq14[36].lenx = biomeval_nbis_q_tree[44].lenx;
   biomeval_nbis_q_tree_wsq14[36].leny = biomeval_nbis_q_tree[44].leny;
   biomeval_nbis_q_tree_wsq14[37].x = biomeval_nbis_q_tree[45].x;
   biomeval_nbis_q_tree_wsq14[37].y = biomeval_nbis_q_tree[45].y;
   biomeval_nbis_q_tree_wsq14[37].lenx = biomeval_nbis_q_tree[45].lenx;
   biomeval_nbis_q_tree_wsq14[37].leny = biomeval_nbis_q_tree[45].leny;
   biomeval_nbis_q_tree_wsq14[38].x = biomeval_nbis_q_tree[46].x;
   biomeval_nbis_q_tree_wsq14[38].y = biomeval_nbis_q_tree[46].y;
   biomeval_nbis_q_tree_wsq14[38].lenx = biomeval_nbis_q_tree[46].lenx;
   biomeval_nbis_q_tree_wsq14[38].leny = biomeval_nbis_q_tree[46].leny;

   biomeval_nbis_q_tree_wsq14[39].x = biomeval_nbis_q_tree[48].x;
   biomeval_nbis_q_tree_wsq14[39].y = biomeval_nbis_q_tree[48].y;
   biomeval_nbis_q_tree_wsq14[39].lenx = biomeval_nbis_q_tree[48].lenx;
   biomeval_nbis_q_tree_wsq14[39].leny = biomeval_nbis_q_tree[48].leny;
   biomeval_nbis_q_tree_wsq14[40].x = biomeval_nbis_q_tree[47].x;
   biomeval_nbis_q_tree_wsq14[40].y = biomeval_nbis_q_tree[47].y;
   biomeval_nbis_q_tree_wsq14[40].lenx = biomeval_nbis_q_tree[47].lenx;
   biomeval_nbis_q_tree_wsq14[40].leny = biomeval_nbis_q_tree[47].leny;
   biomeval_nbis_q_tree_wsq14[41].x = biomeval_nbis_q_tree[50].x;
   biomeval_nbis_q_tree_wsq14[41].y = biomeval_nbis_q_tree[50].y;
   biomeval_nbis_q_tree_wsq14[41].lenx = biomeval_nbis_q_tree[50].lenx;
   biomeval_nbis_q_tree_wsq14[41].leny = biomeval_nbis_q_tree[50].leny;
   biomeval_nbis_q_tree_wsq14[42].x = biomeval_nbis_q_tree[49].x;
   biomeval_nbis_q_tree_wsq14[42].y = biomeval_nbis_q_tree[49].y;
   biomeval_nbis_q_tree_wsq14[42].lenx = biomeval_nbis_q_tree[49].lenx;
   biomeval_nbis_q_tree_wsq14[42].leny = biomeval_nbis_q_tree[49].leny;

   biomeval_nbis_q_tree_wsq14[43].x = biomeval_nbis_q_tree[37].x;
   biomeval_nbis_q_tree_wsq14[43].y = biomeval_nbis_q_tree[37].y;
   biomeval_nbis_q_tree_wsq14[43].lenx = biomeval_nbis_q_tree[37].lenx;
   biomeval_nbis_q_tree_wsq14[43].leny = biomeval_nbis_q_tree[37].leny;
   biomeval_nbis_q_tree_wsq14[44].x = biomeval_nbis_q_tree[38].x;
   biomeval_nbis_q_tree_wsq14[44].y = biomeval_nbis_q_tree[38].y;
   biomeval_nbis_q_tree_wsq14[44].lenx = biomeval_nbis_q_tree[38].lenx;
   biomeval_nbis_q_tree_wsq14[44].leny = biomeval_nbis_q_tree[38].leny;
   biomeval_nbis_q_tree_wsq14[45].x = biomeval_nbis_q_tree[35].x;
   biomeval_nbis_q_tree_wsq14[45].y = biomeval_nbis_q_tree[35].y;
   biomeval_nbis_q_tree_wsq14[45].lenx = biomeval_nbis_q_tree[35].lenx;
   biomeval_nbis_q_tree_wsq14[45].leny = biomeval_nbis_q_tree[35].leny;
   biomeval_nbis_q_tree_wsq14[46].x = biomeval_nbis_q_tree[36].x;
   biomeval_nbis_q_tree_wsq14[46].y = biomeval_nbis_q_tree[36].y;
   biomeval_nbis_q_tree_wsq14[46].lenx = biomeval_nbis_q_tree[36].lenx;
   biomeval_nbis_q_tree_wsq14[46].leny = biomeval_nbis_q_tree[36].leny;

   biomeval_nbis_q_tree_wsq14[47].x = biomeval_nbis_q_tree[42].x;
   biomeval_nbis_q_tree_wsq14[47].y = biomeval_nbis_q_tree[42].y;
   biomeval_nbis_q_tree_wsq14[47].lenx = biomeval_nbis_q_tree[42].lenx;
   biomeval_nbis_q_tree_wsq14[47].leny = biomeval_nbis_q_tree[42].leny;
   biomeval_nbis_q_tree_wsq14[48].x = biomeval_nbis_q_tree[41].x;
   biomeval_nbis_q_tree_wsq14[48].y = biomeval_nbis_q_tree[41].y;
   biomeval_nbis_q_tree_wsq14[48].lenx = biomeval_nbis_q_tree[41].lenx;
   biomeval_nbis_q_tree_wsq14[48].leny = biomeval_nbis_q_tree[41].leny;
   biomeval_nbis_q_tree_wsq14[49].x = biomeval_nbis_q_tree[40].x;
   biomeval_nbis_q_tree_wsq14[49].y = biomeval_nbis_q_tree[40].y;
   biomeval_nbis_q_tree_wsq14[49].lenx = biomeval_nbis_q_tree[40].lenx;
   biomeval_nbis_q_tree_wsq14[49].leny = biomeval_nbis_q_tree[40].leny;
   biomeval_nbis_q_tree_wsq14[50].x = biomeval_nbis_q_tree[39].x;
   biomeval_nbis_q_tree_wsq14[50].y = biomeval_nbis_q_tree[39].y;
   biomeval_nbis_q_tree_wsq14[50].lenx = biomeval_nbis_q_tree[39].lenx;
   biomeval_nbis_q_tree_wsq14[50].leny = biomeval_nbis_q_tree[39].leny;

   biomeval_nbis_q_tree_wsq14[51].x = biomeval_nbis_q_tree[51].x;
   biomeval_nbis_q_tree_wsq14[51].y = biomeval_nbis_q_tree[51].y;
   biomeval_nbis_q_tree_wsq14[51].lenx = biomeval_nbis_q_tree[51].lenx;
   biomeval_nbis_q_tree_wsq14[51].leny = biomeval_nbis_q_tree[51].leny;

   biomeval_nbis_q_tree_wsq14[52].x = biomeval_nbis_q_tree[53].x;
   biomeval_nbis_q_tree_wsq14[52].y = biomeval_nbis_q_tree[53].y;
   biomeval_nbis_q_tree_wsq14[52].lenx = biomeval_nbis_q_tree[53].lenx;
   biomeval_nbis_q_tree_wsq14[52].leny = biomeval_nbis_q_tree[53].leny;
   biomeval_nbis_q_tree_wsq14[53].x = biomeval_nbis_q_tree[52].x;
   biomeval_nbis_q_tree_wsq14[53].y = biomeval_nbis_q_tree[52].y;
   biomeval_nbis_q_tree_wsq14[53].lenx = biomeval_nbis_q_tree[52].lenx;
   biomeval_nbis_q_tree_wsq14[53].leny = biomeval_nbis_q_tree[52].leny;
   biomeval_nbis_q_tree_wsq14[54].x = biomeval_nbis_q_tree[55].x;
   biomeval_nbis_q_tree_wsq14[54].y = biomeval_nbis_q_tree[55].y;
   biomeval_nbis_q_tree_wsq14[54].lenx = biomeval_nbis_q_tree[55].lenx;
   biomeval_nbis_q_tree_wsq14[54].leny = biomeval_nbis_q_tree[55].leny;
   biomeval_nbis_q_tree_wsq14[55].x = biomeval_nbis_q_tree[54].x;
   biomeval_nbis_q_tree_wsq14[55].y = biomeval_nbis_q_tree[54].y;
   biomeval_nbis_q_tree_wsq14[55].lenx = biomeval_nbis_q_tree[54].lenx;
   biomeval_nbis_q_tree_wsq14[55].leny = biomeval_nbis_q_tree[54].leny;

   biomeval_nbis_q_tree_wsq14[56].x = biomeval_nbis_q_tree[58].x;
   biomeval_nbis_q_tree_wsq14[56].y = biomeval_nbis_q_tree[58].y;
   biomeval_nbis_q_tree_wsq14[56].lenx = biomeval_nbis_q_tree[58].lenx;
   biomeval_nbis_q_tree_wsq14[56].leny = biomeval_nbis_q_tree[58].leny;
   biomeval_nbis_q_tree_wsq14[57].x = biomeval_nbis_q_tree[59].x;
   biomeval_nbis_q_tree_wsq14[57].y = biomeval_nbis_q_tree[59].y;
   biomeval_nbis_q_tree_wsq14[57].lenx = biomeval_nbis_q_tree[59].lenx;
   biomeval_nbis_q_tree_wsq14[57].leny = biomeval_nbis_q_tree[59].leny;
   biomeval_nbis_q_tree_wsq14[58].x = biomeval_nbis_q_tree[56].x;
   biomeval_nbis_q_tree_wsq14[58].y = biomeval_nbis_q_tree[56].y;
   biomeval_nbis_q_tree_wsq14[58].lenx = biomeval_nbis_q_tree[56].lenx;
   biomeval_nbis_q_tree_wsq14[58].leny = biomeval_nbis_q_tree[56].leny;
   biomeval_nbis_q_tree_wsq14[59].x = biomeval_nbis_q_tree[57].x;
   biomeval_nbis_q_tree_wsq14[59].y = biomeval_nbis_q_tree[57].y;
   biomeval_nbis_q_tree_wsq14[59].lenx = biomeval_nbis_q_tree[57].lenx;
   biomeval_nbis_q_tree_wsq14[59].leny = biomeval_nbis_q_tree[57].leny;

   biomeval_nbis_q_tree_wsq14[60].x = biomeval_nbis_q_tree[63].x;
   biomeval_nbis_q_tree_wsq14[60].y = biomeval_nbis_q_tree[63].y;
   biomeval_nbis_q_tree_wsq14[60].lenx = biomeval_nbis_q_tree[63].lenx;
   biomeval_nbis_q_tree_wsq14[60].leny = biomeval_nbis_q_tree[63].leny;
   biomeval_nbis_q_tree_wsq14[61].x = biomeval_nbis_q_tree[62].x;
   biomeval_nbis_q_tree_wsq14[61].y = biomeval_nbis_q_tree[62].y;
   biomeval_nbis_q_tree_wsq14[61].lenx = biomeval_nbis_q_tree[62].lenx;
   biomeval_nbis_q_tree_wsq14[61].leny = biomeval_nbis_q_tree[62].leny;
   biomeval_nbis_q_tree_wsq14[62].x = biomeval_nbis_q_tree[61].x;
   biomeval_nbis_q_tree_wsq14[62].y = biomeval_nbis_q_tree[61].y;
   biomeval_nbis_q_tree_wsq14[62].lenx = biomeval_nbis_q_tree[61].lenx;
   biomeval_nbis_q_tree_wsq14[62].leny = biomeval_nbis_q_tree[61].leny;
   biomeval_nbis_q_tree_wsq14[63].x = biomeval_nbis_q_tree[60].x;
   biomeval_nbis_q_tree_wsq14[63].y = biomeval_nbis_q_tree[60].y;
   biomeval_nbis_q_tree_wsq14[63].lenx = biomeval_nbis_q_tree[60].lenx;
   biomeval_nbis_q_tree_wsq14[63].leny = biomeval_nbis_q_tree[60].leny;

/*
      for(i = 0; i < biomeval_nbis_q_treelen; i++) {
         fprintf(stderr, "t%d -> x = %d  y = %d : lx = %d  ly = %d\n",
         i, biomeval_nbis_q_tree[i].x, biomeval_nbis_q_tree[i].y, biomeval_nbis_q_tree[i].lenx, biomeval_nbis_q_tree[i].leny);
         fprintf(stderr, "t%d -> x = %d  y = %d : lx = %d  ly = %d\n",
         i, biomeval_nbis_q_tree_wsq14[i].x, biomeval_nbis_q_tree_wsq14[i].y, biomeval_nbis_q_tree_wsq14[i].lenx, biomeval_nbis_q_tree_wsq14[i].leny);
      }
      fprintf(stderr, "\n\n");
*/
}


/********************************************************************/
/* Routine to decode an entire "block" of encoded data from a file. */
/********************************************************************/
static int huffman_decode_data_file_wsq14(
   short *ip,             /* image pointer */
   DTT_TABLE *biomeval_nbis_dtt_table,  /*transform table pointer */
   DQT_TABLE *biomeval_nbis_dqt_table,  /* quantization table */
   DHT_TABLE *biomeval_nbis_dht_table,  /* huffman table */
   FILE *infp)            /* input file */
{
   int ret;
   int blk = 0;           /* block number */
   unsigned short marker;         /* WSQ markers */
   int bit_count;         /* bit count for biomeval_nbis_nextbits_wsq routine */
   int n;                 /* zero run count */
   int nodeptr;           /* pointers for decoding */
   int last_size;         /* last huffvalue */
   unsigned char hufftable_id;    /* huffman table number */
   HUFFCODE *hufftable;   /* huffman code structure */
   int maxcode[MAX_HUFFBITS+1];       /* used in decoding data */
   int mincode[MAX_HUFFBITS+1];       /* used in decoding data */
   int valptr[MAX_HUFFBITS+1];        /* used in decoding data */
   unsigned short tbits;


   if((ret = biomeval_nbis_read_marker_wsq(&marker, TBLS_N_SOB, infp)))
      return(ret);

   bit_count = 0;

   while(marker != EOI_WSQ) {

      if(marker != 0) {
         blk++;
         while(marker != SOB_WSQ) {
            if((ret = biomeval_nbis_read_table_wsq14(marker, biomeval_nbis_dtt_table, biomeval_nbis_dqt_table,
                                biomeval_nbis_dht_table, infp)))
               return(ret);
            if((ret = biomeval_nbis_read_marker_wsq(&marker, TBLS_N_SOB, infp)))
               return(ret);
         }
         if((ret = biomeval_nbis_read_block_header(&hufftable_id, infp)))
            return(ret);

         if((biomeval_nbis_dht_table+hufftable_id)->tabdef != 1) {
            fprintf(stderr, "ERROR : huffman_decode_data_file : ");
            fprintf(stderr, "huffman table {%d} undefined.\n", hufftable_id);
            return(-53);
         }

         /* the next two routines reconstruct the huffman tables */
         if((ret = biomeval_nbis_build_huffsizes(&hufftable, &last_size,
                       (biomeval_nbis_dht_table+hufftable_id)->huffbits, MAX_HUFFCOUNTS_WSQ)))
            return(ret);
         biomeval_nbis_build_huffcodes(hufftable);
         ret = biomeval_nbis_check_huffcodes_wsq(hufftable, last_size);

         /* this routine builds a set of three tables used in decoding */
         /* the compressed data*/
         biomeval_nbis_gen_decode_table(hufftable, maxcode, mincode, valptr,
                          (biomeval_nbis_dht_table+hufftable_id)->huffbits);
         free(hufftable);
         bit_count = 0;
         marker = 0;
      }

      /* get next huffman category code from compressed input data stream */
      if((ret = biomeval_nbis_decode_data_file(&nodeptr, mincode, maxcode, valptr,
                            (biomeval_nbis_dht_table+hufftable_id)->huffvalues,
                            infp, &bit_count, &marker)))
         return(ret);

      if(nodeptr == -1)
         continue;

      if(nodeptr <= 100)
         for(n = 0; n < nodeptr; n++) {
            *ip++ = 0; /* z run */
         }
      else if(nodeptr > 106)
         *ip++ = nodeptr - 180;
      else if(nodeptr == 101){
         if((ret = biomeval_nbis_nextbits_wsq(&tbits, &marker, infp, &bit_count, 8)))
            return(ret);
         *ip++ = tbits;
      }
      else if(nodeptr == 102){
         if((ret = biomeval_nbis_nextbits_wsq(&tbits, &marker, infp, &bit_count, 8)))
            return(ret);
         *ip++ = -tbits;
      }
      else if(nodeptr == 103){
         if((ret = biomeval_nbis_nextbits_wsq(&tbits, &marker, infp, &bit_count, 16)))
            return(ret);
         *ip++ = tbits;
      }
      else if(nodeptr == 104){
         if((ret = biomeval_nbis_nextbits_wsq(&tbits, &marker, infp, &bit_count, 16)))
            return(ret);
         *ip++ = -tbits;
      }
      else if(nodeptr == 105) {
         if((ret = biomeval_nbis_nextbits_wsq(&tbits, &marker, infp, &bit_count, 8)))
            return(ret);
         n = tbits;
         while(n--)
            *ip++ = 0;
      }
      else if(nodeptr == 106) {
         if((ret = biomeval_nbis_nextbits_wsq(&tbits, &marker, infp, &bit_count, 16)))
            return(ret);
         n = tbits;
         while(n--)
            *ip++ = 0;
      }
      else {
         fprintf(stderr, 
                "ERROR: huffman_decode_data_file : Invalid code %d (%x).\n",
                nodeptr, nodeptr);
         return(-54);
      }
   }

   return(0);
}


/****************************************************/
/* Routine to unshuffle WSQ14 (old format) Subbands */
/****************************************************/
static int unbiomeval_nbis_shuffle_wsq14(
   short **ofip,         /* floating point image pointer         */
   const DQT_TABLE *biomeval_nbis_dqt_table, /* quantization table structure   */
   Q_TREE biomeval_nbis_q_tree[],      /* quantization table structure         */
   const int biomeval_nbis_q_treelen,  /* size of biomeval_nbis_q_tree                       */
   short *sip,           /* biomeval_nbis_quantized image pointer              */
   const int width,      /* image width                          */
   const int height)     /* image height                         */
{
   short *fip;    /* floating point image */
   int row, col;  /* cover counter and row/column counters */
   short *fptr;   /* image pointers */
   short *sptr;
   int cnt;       /* subband counter */

   if((fip = (short *) calloc(width*height, sizeof(short))) == NULL) {
      fprintf(stderr,"ERROR : unbiomeval_nbis_quantize : calloc : fip\n");
      return(-2);
   }
   if(biomeval_nbis_dqt_table->dqt_def != 1) {
      fprintf(stderr,
      "ERROR: unbiomeval_nbis_shuffle_wsq14 : quantization table parameters not defined!\n");
      return(-3);
   }

   sptr = sip;
   for(cnt = 0; cnt < NUM_SUBBANDS; cnt++)
      if(biomeval_nbis_dqt_table->q_bin[cnt] != 0.0) {
         fptr = fip + (biomeval_nbis_q_tree[cnt].y * width) + biomeval_nbis_q_tree[cnt].x;

         for(row = 0; row < biomeval_nbis_q_tree[cnt].leny; row++, fptr += width - biomeval_nbis_q_tree[cnt].lenx)
            for(col = 0; col < biomeval_nbis_q_tree[cnt].lenx; col++)
               *fptr++ = *sptr++;
      }

   *ofip = fip;
   return(0);
}


/***************************************************************/
/* Routine shuffles biomeval_nbis_quantized subbands from old to new format. */
/***************************************************************/
static int biomeval_nbis_shuffle_wsq14(
   short **osip,           /* biomeval_nbis_quantized output             */
   int *ocmp_siz,          /* size of biomeval_nbis_quantized output     */
   const DQT_TABLE biomeval_nbis_dqt_table, /* quantization table structure   */
   Q_TREE biomeval_nbis_q_tree_wsq14[],   /* quantization "tree"          */
   const int biomeval_nbis_q_treelen,    /* size of biomeval_nbis_q_tree               */
   short *fip,             /* floating point image pointer */
   const int width,        /* image width                  */
   const int height)       /* image height                 */
{
   short *fptr;           /* temp image pointer */
   short *sip, *sptr;     /* pointers to biomeval_nbis_quantized image */
   int row, col;          /* temp image characteristic parameters */
   int cnt;               /* subband counter */

   if(biomeval_nbis_dqt_table.dqt_def != 1) {
      fprintf(stderr,
      "ERROR: biomeval_nbis_shuffle_wsq14 : quantization table parameters not defined!\n");
      return(-92);
   }

   /* Set up output buffer. */
   if((sip = (short *) calloc(width*height, sizeof(short))) == NULL) {
      fprintf(stderr,"ERROR : biomeval_nbis_quantize : calloc : sip\n");
      return(-90);
   }
   sptr = sip;

   for(cnt = 0; cnt < NUM_SUBBANDS; cnt++)

      if(biomeval_nbis_dqt_table.q_bin[cnt] != 0.0) {
         fptr = fip + (biomeval_nbis_q_tree_wsq14[cnt].y * width) + biomeval_nbis_q_tree_wsq14[cnt].x;

         for(row = 0; row < biomeval_nbis_q_tree_wsq14[cnt].leny; row++, fptr += width - biomeval_nbis_q_tree_wsq14[cnt].lenx)
            for(col = 0; col < biomeval_nbis_q_tree_wsq14[cnt].lenx; col++)
               *sptr++ = *fptr++;
      }

   *osip = sip;
   *ocmp_siz = sptr - sip;
   return(0);
}


/**************************************************************/
/* Routine shuffles quantization tree from old to new format. */
/**************************************************************/
static int biomeval_nbis_shuffle_dqt_wsq14(DQT_TABLE *biomeval_nbis_dqt_table)
{
   int i;
   float tq[MAX_SUBBANDS], tz[MAX_SUBBANDS];

   for(i = 0; i < 7; i++) {
      tq[i] = biomeval_nbis_dqt_table->q_bin[i];
      tz[i] = biomeval_nbis_dqt_table->z_bin[i];
   }

   tq[7] = biomeval_nbis_dqt_table->q_bin[8];
   tq[8] = biomeval_nbis_dqt_table->q_bin[7];
   tq[9] = biomeval_nbis_dqt_table->q_bin[10];
   tq[10] = biomeval_nbis_dqt_table->q_bin[9];
   tq[11] = biomeval_nbis_dqt_table->q_bin[13];
   tq[12] = biomeval_nbis_dqt_table->q_bin[14];
   tq[13] = biomeval_nbis_dqt_table->q_bin[11];
   tq[14] = biomeval_nbis_dqt_table->q_bin[12];
   tq[15] = biomeval_nbis_dqt_table->q_bin[18];
   tq[16] = biomeval_nbis_dqt_table->q_bin[17];
   tq[17] = biomeval_nbis_dqt_table->q_bin[16];
   tq[18] = biomeval_nbis_dqt_table->q_bin[15];
   tq[19] = biomeval_nbis_dqt_table->q_bin[23];
   tq[20] = biomeval_nbis_dqt_table->q_bin[24];
   tq[21] = biomeval_nbis_dqt_table->q_bin[25];
   tq[22] = biomeval_nbis_dqt_table->q_bin[26];
   tq[23] = biomeval_nbis_dqt_table->q_bin[20];
   tq[24] = biomeval_nbis_dqt_table->q_bin[19];
   tq[25] = biomeval_nbis_dqt_table->q_bin[22];
   tq[26] = biomeval_nbis_dqt_table->q_bin[21];
   tq[27] = biomeval_nbis_dqt_table->q_bin[33];
   tq[28] = biomeval_nbis_dqt_table->q_bin[34];
   tq[29] = biomeval_nbis_dqt_table->q_bin[31];
   tq[30] = biomeval_nbis_dqt_table->q_bin[32];
   tq[31] = biomeval_nbis_dqt_table->q_bin[30];
   tq[32] = biomeval_nbis_dqt_table->q_bin[29];
   tq[33] = biomeval_nbis_dqt_table->q_bin[28];
   tq[34] = biomeval_nbis_dqt_table->q_bin[27];
   tq[35] = biomeval_nbis_dqt_table->q_bin[43];
   tq[36] = biomeval_nbis_dqt_table->q_bin[44];
   tq[37] = biomeval_nbis_dqt_table->q_bin[45];
   tq[38] = biomeval_nbis_dqt_table->q_bin[46];
   tq[39] = biomeval_nbis_dqt_table->q_bin[48];
   tq[40] = biomeval_nbis_dqt_table->q_bin[47];
   tq[41] = biomeval_nbis_dqt_table->q_bin[50];
   tq[42] = biomeval_nbis_dqt_table->q_bin[49];
   tq[43] = biomeval_nbis_dqt_table->q_bin[37];
   tq[44] = biomeval_nbis_dqt_table->q_bin[38];
   tq[45] = biomeval_nbis_dqt_table->q_bin[35];
   tq[46] = biomeval_nbis_dqt_table->q_bin[36];
   tq[47] = biomeval_nbis_dqt_table->q_bin[42];
   tq[48] = biomeval_nbis_dqt_table->q_bin[41];
   tq[49] = biomeval_nbis_dqt_table->q_bin[40];
   tq[50] = biomeval_nbis_dqt_table->q_bin[39];
   tq[51] = biomeval_nbis_dqt_table->q_bin[51];
   tq[52] = biomeval_nbis_dqt_table->q_bin[53];
   tq[53] = biomeval_nbis_dqt_table->q_bin[52];
   tq[54] = biomeval_nbis_dqt_table->q_bin[55];
   tq[55] = biomeval_nbis_dqt_table->q_bin[54];
   tq[56] = biomeval_nbis_dqt_table->q_bin[58];
   tq[57] = biomeval_nbis_dqt_table->q_bin[59];
   tq[58] = biomeval_nbis_dqt_table->q_bin[56];
   tq[59] = biomeval_nbis_dqt_table->q_bin[57];
   tq[60] = biomeval_nbis_dqt_table->q_bin[63];
   tq[61] = biomeval_nbis_dqt_table->q_bin[62];
   tq[62] = biomeval_nbis_dqt_table->q_bin[61];
   tq[63] = biomeval_nbis_dqt_table->q_bin[60];

   tz[7] = biomeval_nbis_dqt_table->z_bin[8];
   tz[8] = biomeval_nbis_dqt_table->z_bin[7];
   tz[9] = biomeval_nbis_dqt_table->z_bin[10];
   tz[10] = biomeval_nbis_dqt_table->z_bin[9];
   tz[11] = biomeval_nbis_dqt_table->z_bin[13];
   tz[12] = biomeval_nbis_dqt_table->z_bin[14];
   tz[13] = biomeval_nbis_dqt_table->z_bin[11];
   tz[14] = biomeval_nbis_dqt_table->z_bin[12];
   tz[15] = biomeval_nbis_dqt_table->z_bin[18];
   tz[16] = biomeval_nbis_dqt_table->z_bin[17];
   tz[17] = biomeval_nbis_dqt_table->z_bin[16];
   tz[18] = biomeval_nbis_dqt_table->z_bin[15];
   tz[19] = biomeval_nbis_dqt_table->z_bin[23];
   tz[20] = biomeval_nbis_dqt_table->z_bin[24];
   tz[21] = biomeval_nbis_dqt_table->z_bin[25];
   tz[22] = biomeval_nbis_dqt_table->z_bin[26];
   tz[23] = biomeval_nbis_dqt_table->z_bin[20];
   tz[24] = biomeval_nbis_dqt_table->z_bin[19];
   tz[25] = biomeval_nbis_dqt_table->z_bin[22];
   tz[26] = biomeval_nbis_dqt_table->z_bin[21];
   tz[27] = biomeval_nbis_dqt_table->z_bin[33];
   tz[28] = biomeval_nbis_dqt_table->z_bin[34];
   tz[29] = biomeval_nbis_dqt_table->z_bin[31];
   tz[30] = biomeval_nbis_dqt_table->z_bin[32];
   tz[31] = biomeval_nbis_dqt_table->z_bin[30];
   tz[32] = biomeval_nbis_dqt_table->z_bin[29];
   tz[33] = biomeval_nbis_dqt_table->z_bin[28];
   tz[34] = biomeval_nbis_dqt_table->z_bin[27];
   tz[35] = biomeval_nbis_dqt_table->z_bin[43];
   tz[36] = biomeval_nbis_dqt_table->z_bin[44];
   tz[37] = biomeval_nbis_dqt_table->z_bin[45];
   tz[38] = biomeval_nbis_dqt_table->z_bin[46];
   tz[39] = biomeval_nbis_dqt_table->z_bin[48];
   tz[40] = biomeval_nbis_dqt_table->z_bin[47];
   tz[41] = biomeval_nbis_dqt_table->z_bin[50];
   tz[42] = biomeval_nbis_dqt_table->z_bin[49];
   tz[43] = biomeval_nbis_dqt_table->z_bin[37];
   tz[44] = biomeval_nbis_dqt_table->z_bin[38];
   tz[45] = biomeval_nbis_dqt_table->z_bin[35];
   tz[46] = biomeval_nbis_dqt_table->z_bin[36];
   tz[47] = biomeval_nbis_dqt_table->z_bin[42];
   tz[48] = biomeval_nbis_dqt_table->z_bin[41];
   tz[49] = biomeval_nbis_dqt_table->z_bin[40];
   tz[50] = biomeval_nbis_dqt_table->z_bin[39];
   tz[51] = biomeval_nbis_dqt_table->z_bin[51];
   tz[52] = biomeval_nbis_dqt_table->z_bin[53];
   tz[53] = biomeval_nbis_dqt_table->z_bin[52];
   tz[54] = biomeval_nbis_dqt_table->z_bin[55];
   tz[55] = biomeval_nbis_dqt_table->z_bin[54];
   tz[56] = biomeval_nbis_dqt_table->z_bin[58];
   tz[57] = biomeval_nbis_dqt_table->z_bin[59];
   tz[58] = biomeval_nbis_dqt_table->z_bin[56];
   tz[59] = biomeval_nbis_dqt_table->z_bin[57];
   tz[60] = biomeval_nbis_dqt_table->z_bin[63];
   tz[61] = biomeval_nbis_dqt_table->z_bin[62];
   tz[62] = biomeval_nbis_dqt_table->z_bin[61];
   tz[63] = biomeval_nbis_dqt_table->z_bin[60];

   for(i = 0; i < MAX_SUBBANDS; i++) {
      biomeval_nbis_dqt_table->q_bin[i] = tq[i];
      biomeval_nbis_dqt_table->z_bin[i] = tz[i];
   }
   return(0);
}


/************************************************************/
/* Routine to obtain old format subband "x-y locations" for */
/* creating wavelets.                                       */
/************************************************************/
static void biomeval_nbis_build_w_tree_wsq14(W_TREE biomeval_nbis_w_tree[],
                        const int width, const int height)
{

   /* This table gives the location where the splitting of particular
      subbands will occur (upper left hand corner of named subband)
      relative to the specs subband numbering system (i.e. 0-63).  It
      also gives the subbands created by the given split.

      biomeval_nbis_w_tree[?]     uppper left of this subband   subbands created
      ---------     ---------------------------   ----------------
          0                      0
          1                      0                51
          2                     52                52, 53, 54, 55
          3                     56                56, 57, 58, 59
          4                     19
          5                     35
          6                     19                19, 20, 21, 22
          7                     23                23, 24, 25, 26
          8                     27                27, 28, 29, 30
          9                     31                31, 32, 33, 34
         10                     35                35, 36, 37, 38
         11                     39                39, 40, 41, 42
         12                     43                43, 44, 45, 46
         13                     47                47, 48, 49, 50
         14                      0
         15                      0                4, 5, 6
         16                      7                7, 8, 9, 10
         17                     11                11, 12, 13, 14
         18                     15                15, 16, 17, 18
         19                      0                0, 1, 2, 3       */


   int lenx, lenx2, leny, leny2;	/* starting lengths of sections of
                                           the image being split into
                                           subbands */

   biomeval_nbis_w_tree4_wsq14(biomeval_nbis_w_tree, 0, 1, width, height, 0, 0, 1);

   if((biomeval_nbis_w_tree[1].lenx % 2) == 0) {
      lenx = biomeval_nbis_w_tree[1].lenx / 2;
      lenx2 = lenx;
   }
   else {
      lenx = (biomeval_nbis_w_tree[1].lenx + 1) / 2;
      lenx2 = lenx - 1;
   }

   if((biomeval_nbis_w_tree[1].leny % 2) == 0) {
      leny = biomeval_nbis_w_tree[1].leny / 2;
      leny2 = leny;
   }
   else {
      leny = (biomeval_nbis_w_tree[1].leny + 1) / 2;
      leny2 = leny - 1;
   }

   biomeval_nbis_w_tree4_wsq14(biomeval_nbis_w_tree, 4, 6, lenx2, leny, lenx, 0, 0);
   biomeval_nbis_w_tree4_wsq14(biomeval_nbis_w_tree, 5, 10, lenx, leny2, 0, leny, 0);
   biomeval_nbis_w_tree4_wsq14(biomeval_nbis_w_tree, 14, 15, lenx, leny, 0, 0, 0);

   biomeval_nbis_w_tree[19].x = 0;
   biomeval_nbis_w_tree[19].y = 0;
   if((biomeval_nbis_w_tree[15].lenx % 2) == 0)
      biomeval_nbis_w_tree[19].lenx = biomeval_nbis_w_tree[15].lenx / 2;
   else
      biomeval_nbis_w_tree[19].lenx = (biomeval_nbis_w_tree[15].lenx + 1) / 2;

   if((biomeval_nbis_w_tree[15].leny % 2) == 0)
      biomeval_nbis_w_tree[19].leny = biomeval_nbis_w_tree[15].leny / 2;
   else
      biomeval_nbis_w_tree[19].leny = (biomeval_nbis_w_tree[15].leny + 1) / 2;

   return;
}


/*********************************************************************/
/* Gives location and size of subband splits for biomeval_nbis_build_w_tree_wsq14. */
/*********************************************************************/
static void biomeval_nbis_w_tree4_wsq14(
   W_TREE biomeval_nbis_w_tree[],	/* wavelet tree structure */
   int start1,		/* biomeval_nbis_w_tree locations to start calculating */
   int start2,          /*    subband split locations and sizes  */
   int lenx,            /* (temp) subband split location and sizes */
   int leny,
   int x,
   int y,	
   int stop1)           /* 0 normal operation, 1 used to avoid
                           marking size and location of subbands
                           60-63 */
{
   int evenx, eveny;	/* Check length of subband for even or odd */
   int p1, p2;		/* biomeval_nbis_w_tree locations for storing subband sizes and
                           locations */
   
   p1 = start1;
   p2 = start2;

   evenx = lenx % 2;
   eveny = leny % 2;

   biomeval_nbis_w_tree[p1].x = x;
   biomeval_nbis_w_tree[p1].y = y;
   biomeval_nbis_w_tree[p1].lenx = lenx;
   biomeval_nbis_w_tree[p1].leny = leny;
   
   biomeval_nbis_w_tree[p2].x = x;
   biomeval_nbis_w_tree[p2+2].x = x;
   biomeval_nbis_w_tree[p2].y = y;
   biomeval_nbis_w_tree[p2+1].y = y;

   if(evenx == 0) {
      biomeval_nbis_w_tree[p2].lenx = lenx / 2;
      biomeval_nbis_w_tree[p2+1].lenx = biomeval_nbis_w_tree[p2].lenx;
      if(stop1 == 0)
         biomeval_nbis_w_tree[p2+3].lenx = biomeval_nbis_w_tree[p2].lenx;
   }
   else {
      biomeval_nbis_w_tree[p2].lenx = (lenx +1) / 2;
      biomeval_nbis_w_tree[p2+1].lenx = biomeval_nbis_w_tree[p2].lenx - 1;
      if(stop1 == 0)
         biomeval_nbis_w_tree[p2+3].lenx = biomeval_nbis_w_tree[p2+1].lenx;
   }
   biomeval_nbis_w_tree[p2+1].x = biomeval_nbis_w_tree[p2].lenx + x;
   if(stop1 == 0)
      biomeval_nbis_w_tree[p2+3].x = biomeval_nbis_w_tree[p2+1].x;
   biomeval_nbis_w_tree[p2+2].lenx = biomeval_nbis_w_tree[p2].lenx;


   if(eveny == 0) {
      biomeval_nbis_w_tree[p2].leny = leny / 2;
      biomeval_nbis_w_tree[p2+2].leny = biomeval_nbis_w_tree[p2].leny;
      if(stop1 == 0)
         biomeval_nbis_w_tree[p2+3].leny = biomeval_nbis_w_tree[p2].leny;
   }
   else {
      biomeval_nbis_w_tree[p2].leny = (leny + 1) / 2;
      biomeval_nbis_w_tree[p2+2].leny = biomeval_nbis_w_tree[p2].leny - 1;
      if(stop1 == 0)
         biomeval_nbis_w_tree[p2+3].leny = biomeval_nbis_w_tree[p2+2].leny;
   }
   biomeval_nbis_w_tree[p2+2].y = biomeval_nbis_w_tree[p2].leny + y;
   if(stop1 == 0)
      biomeval_nbis_w_tree[p2+3].y = biomeval_nbis_w_tree[p2+2].y;
   biomeval_nbis_w_tree[p2+1].leny = biomeval_nbis_w_tree[p2].leny;
}


/*************************************************************/
/* Routine obtains the old format locations and sizes of the */
/* subbands 0-63.                                            */
/*************************************************************/
static void biomeval_nbis_build_q_tree_wsq14(
   W_TREE biomeval_nbis_w_tree[], /* wavelet tree structure */
   Q_TREE biomeval_nbis_q_tree[])  /* quantization tree structure */
{

   biomeval_nbis_q_tree16_wsq14(biomeval_nbis_q_tree,3,biomeval_nbis_w_tree[14].lenx,biomeval_nbis_w_tree[14].leny,biomeval_nbis_w_tree[14].x,biomeval_nbis_w_tree[14].y);
   biomeval_nbis_q_tree4_wsq14(biomeval_nbis_q_tree,0,biomeval_nbis_w_tree[19].lenx,biomeval_nbis_w_tree[19].leny,biomeval_nbis_w_tree[19].x,biomeval_nbis_w_tree[19].y);
   biomeval_nbis_q_tree16_wsq14(biomeval_nbis_q_tree,19,biomeval_nbis_w_tree[4].lenx,biomeval_nbis_w_tree[4].leny,biomeval_nbis_w_tree[4].x,biomeval_nbis_w_tree[4].y);
   biomeval_nbis_q_tree16_wsq14(biomeval_nbis_q_tree,35,biomeval_nbis_w_tree[5].lenx,biomeval_nbis_w_tree[5].leny,biomeval_nbis_w_tree[5].x,biomeval_nbis_w_tree[5].y);
   biomeval_nbis_q_tree4_wsq14(biomeval_nbis_q_tree,52,biomeval_nbis_w_tree[2].lenx,biomeval_nbis_w_tree[2].leny,biomeval_nbis_w_tree[2].x,biomeval_nbis_w_tree[2].y);
   biomeval_nbis_q_tree4_wsq14(biomeval_nbis_q_tree,56,biomeval_nbis_w_tree[3].lenx,biomeval_nbis_w_tree[3].leny,biomeval_nbis_w_tree[3].x,biomeval_nbis_w_tree[3].y);
   biomeval_nbis_q_tree4_wsq14(biomeval_nbis_q_tree,60,biomeval_nbis_w_tree[2].lenx,biomeval_nbis_w_tree[3].leny,biomeval_nbis_w_tree[2].x,biomeval_nbis_w_tree[3].y);

   biomeval_nbis_q_tree[51].x = biomeval_nbis_w_tree[4].x;
   biomeval_nbis_q_tree[51].y = biomeval_nbis_w_tree[5].y;
   biomeval_nbis_q_tree[51].lenx = biomeval_nbis_w_tree[4].lenx;
   biomeval_nbis_q_tree[51].leny = biomeval_nbis_w_tree[5].leny;

   return;
}


/****************************************************************************/
/* Routine gives old format subband locations and sizes for lower frequency */
/* subbands in groups of 16 (i.e. 19-34 and 35-50).                         */
/****************************************************************************/
static void biomeval_nbis_q_tree16_wsq14(
   Q_TREE biomeval_nbis_q_tree[],  /* quantization tree structure */
   int start,           /* biomeval_nbis_q_tree location of first subband
                           in the subband group being calculated */
   int lenx,            /* (temp) subband location and sizes */
   int leny,
   int x,
   int y)
{
   int tempx, temp2x;	/* temporary x values */
   int tempy, temp2y;	/* temporary y values */
   int evenx, eveny;	/* Check length of subband for even or odd */
   int p;		/* indicates subband information being stored */

   p = start;
   evenx = lenx % 2;
   eveny = leny % 2;

   if(evenx == 0) {
      tempx = lenx / 2;
      temp2x = tempx;
   }
   else {
      tempx = (lenx + 1) / 2;
      temp2x = tempx - 1;
   }
   
   if(eveny == 0) {
      tempy = leny / 2;
      temp2y = tempy;
   }
   else {
      tempy = (leny + 1) / 2;
      temp2y = tempy - 1;
   }

   evenx = tempx % 2;
   eveny = tempy % 2;

   biomeval_nbis_q_tree[p].x = x;
   biomeval_nbis_q_tree[p+2].x = x;
   biomeval_nbis_q_tree[p].y = y;
   biomeval_nbis_q_tree[p+1].y = y;
   if(evenx == 0) {
      biomeval_nbis_q_tree[p].lenx = tempx / 2;
      biomeval_nbis_q_tree[p+1].lenx = biomeval_nbis_q_tree[p].lenx;
      biomeval_nbis_q_tree[p+2].lenx = biomeval_nbis_q_tree[p].lenx;
      biomeval_nbis_q_tree[p+3].lenx = biomeval_nbis_q_tree[p].lenx;
   }
   else {
      biomeval_nbis_q_tree[p].lenx = (tempx + 1) / 2;
      biomeval_nbis_q_tree[p+1].lenx = biomeval_nbis_q_tree[p].lenx - 1;
      biomeval_nbis_q_tree[p+2].lenx = biomeval_nbis_q_tree[p].lenx;
      biomeval_nbis_q_tree[p+3].lenx = biomeval_nbis_q_tree[p+1].lenx;
   }
   biomeval_nbis_q_tree[p+1].x = x + biomeval_nbis_q_tree[p].lenx;
   biomeval_nbis_q_tree[p+3].x = biomeval_nbis_q_tree[p+1].x;
   if(eveny == 0) {
      biomeval_nbis_q_tree[p].leny = tempy / 2;
      biomeval_nbis_q_tree[p+1].leny = biomeval_nbis_q_tree[p].leny;
      biomeval_nbis_q_tree[p+2].leny = biomeval_nbis_q_tree[p].leny;
      biomeval_nbis_q_tree[p+3].leny = biomeval_nbis_q_tree[p].leny;
   }
   else {
      biomeval_nbis_q_tree[p].leny = (tempy + 1) / 2;
      biomeval_nbis_q_tree[p+1].leny = biomeval_nbis_q_tree[p].leny;
      biomeval_nbis_q_tree[p+2].leny = biomeval_nbis_q_tree[p].leny - 1;
      biomeval_nbis_q_tree[p+3].leny = biomeval_nbis_q_tree[p+2].leny;
   }
   biomeval_nbis_q_tree[p+2].y = y + biomeval_nbis_q_tree[p].leny;
   biomeval_nbis_q_tree[p+3].y = biomeval_nbis_q_tree[p+2].y;


   evenx = temp2x % 2;

   biomeval_nbis_q_tree[p+4].x = x + tempx;
   biomeval_nbis_q_tree[p+6].x = biomeval_nbis_q_tree[p+4].x;
   biomeval_nbis_q_tree[p+4].y = y;
   biomeval_nbis_q_tree[p+5].y = y;
   biomeval_nbis_q_tree[p+6].y = biomeval_nbis_q_tree[p+2].y;
   biomeval_nbis_q_tree[p+7].y = biomeval_nbis_q_tree[p+2].y;
   if(evenx == 0) {
      biomeval_nbis_q_tree[p+4].lenx = temp2x / 2;
      biomeval_nbis_q_tree[p+5].lenx = biomeval_nbis_q_tree[p+4].lenx;
      biomeval_nbis_q_tree[p+6].lenx = biomeval_nbis_q_tree[p+4].lenx;
      biomeval_nbis_q_tree[p+7].lenx = biomeval_nbis_q_tree[p+4].lenx;
   }
   else {
      biomeval_nbis_q_tree[p+4].lenx = (temp2x + 1) / 2;
      biomeval_nbis_q_tree[p+5].lenx = biomeval_nbis_q_tree[p+4].lenx - 1;
      biomeval_nbis_q_tree[p+6].lenx = biomeval_nbis_q_tree[p+4].lenx;
      biomeval_nbis_q_tree[p+7].lenx = biomeval_nbis_q_tree[p+5].lenx;
   }
   biomeval_nbis_q_tree[p+5].x = biomeval_nbis_q_tree[p+4].x + biomeval_nbis_q_tree[p+4].lenx;
   biomeval_nbis_q_tree[p+7].x = biomeval_nbis_q_tree[p+5].x;
   biomeval_nbis_q_tree[p+4].leny = biomeval_nbis_q_tree[p].leny;
   biomeval_nbis_q_tree[p+5].leny = biomeval_nbis_q_tree[p].leny;
   biomeval_nbis_q_tree[p+6].leny = biomeval_nbis_q_tree[p+2].leny;
   biomeval_nbis_q_tree[p+7].leny = biomeval_nbis_q_tree[p+2].leny;


   eveny = temp2y % 2;

   biomeval_nbis_q_tree[p+8].x = x;
   biomeval_nbis_q_tree[p+9].x = biomeval_nbis_q_tree[p+1].x;
   biomeval_nbis_q_tree[p+10].x = x;
   biomeval_nbis_q_tree[p+11].x = biomeval_nbis_q_tree[p+1].x;
   biomeval_nbis_q_tree[p+8].y = y + tempy;
   biomeval_nbis_q_tree[p+9].y = biomeval_nbis_q_tree[p+8].y;
   biomeval_nbis_q_tree[p+8].lenx = biomeval_nbis_q_tree[p].lenx;
   biomeval_nbis_q_tree[p+9].lenx = biomeval_nbis_q_tree[p+1].lenx;
   biomeval_nbis_q_tree[p+10].lenx = biomeval_nbis_q_tree[p].lenx;
   biomeval_nbis_q_tree[p+11].lenx = biomeval_nbis_q_tree[p+1].lenx;
   if(eveny == 0) {
      biomeval_nbis_q_tree[p+8].leny = temp2y / 2;
      biomeval_nbis_q_tree[p+9].leny = biomeval_nbis_q_tree[p+8].leny;
      biomeval_nbis_q_tree[p+10].leny = biomeval_nbis_q_tree[p+8].leny;
      biomeval_nbis_q_tree[p+11].leny = biomeval_nbis_q_tree[p+8].leny;
   }
   else {
      biomeval_nbis_q_tree[p+8].leny = (temp2y + 1) / 2;
      biomeval_nbis_q_tree[p+9].leny = biomeval_nbis_q_tree[p+8].leny;
      biomeval_nbis_q_tree[p+10].leny = biomeval_nbis_q_tree[p+8].leny - 1;
      biomeval_nbis_q_tree[p+11].leny = biomeval_nbis_q_tree[p+10].leny;
   }
   biomeval_nbis_q_tree[p+10].y = biomeval_nbis_q_tree[p+8].y + biomeval_nbis_q_tree[p+8].leny;
   biomeval_nbis_q_tree[p+11].y = biomeval_nbis_q_tree[p+10].y;


   biomeval_nbis_q_tree[p+12].x = biomeval_nbis_q_tree[p+4].x;
   biomeval_nbis_q_tree[p+13].x = biomeval_nbis_q_tree[p+5].x;
   biomeval_nbis_q_tree[p+14].x = biomeval_nbis_q_tree[p+4].x;
   biomeval_nbis_q_tree[p+15].x = biomeval_nbis_q_tree[p+5].x;
   biomeval_nbis_q_tree[p+12].y = biomeval_nbis_q_tree[p+8].y;
   biomeval_nbis_q_tree[p+13].y = biomeval_nbis_q_tree[p+8].y;
   biomeval_nbis_q_tree[p+14].y = biomeval_nbis_q_tree[p+10].y;
   biomeval_nbis_q_tree[p+15].y = biomeval_nbis_q_tree[p+10].y;
   biomeval_nbis_q_tree[p+12].lenx = biomeval_nbis_q_tree[p+4].lenx;
   biomeval_nbis_q_tree[p+13].lenx = biomeval_nbis_q_tree[p+5].lenx;
   biomeval_nbis_q_tree[p+14].lenx = biomeval_nbis_q_tree[p+4].lenx;
   biomeval_nbis_q_tree[p+15].lenx = biomeval_nbis_q_tree[p+5].lenx;
   biomeval_nbis_q_tree[p+12].leny = biomeval_nbis_q_tree[p+8].leny;
   biomeval_nbis_q_tree[p+13].leny = biomeval_nbis_q_tree[p+8].leny;
   biomeval_nbis_q_tree[p+14].leny = biomeval_nbis_q_tree[p+10].leny;
   biomeval_nbis_q_tree[p+15].leny = biomeval_nbis_q_tree[p+10].leny;
}


/*********************************************************************/
/* Routine gives old format subband locations and sizes for subbands */
/* in groups of 4 (i.e. 0-3 and 52-55).                              */
/*********************************************************************/
static void biomeval_nbis_q_tree4_wsq14(
   Q_TREE biomeval_nbis_q_tree[],  /* quantization tree structure */
   int start,		/* biomeval_nbis_q_tree location of first subband
                           in the subband group being calculated */
   int lenx,            /* (temp) subband location and sizes */
   int leny,
   int x,
   int y)
{
   int evenx, eveny;	/* Check length of subband for even or odd */
   int p;		/* indicates subband information being stored */


   p = start;
   evenx = lenx % 2;
   eveny = leny % 2;


   biomeval_nbis_q_tree[p].x = x;
   biomeval_nbis_q_tree[p+2].x = x;
   biomeval_nbis_q_tree[p].y = y;
   biomeval_nbis_q_tree[p+1].y = y;
   if(evenx == 0) {
      biomeval_nbis_q_tree[p].lenx = lenx / 2;
      biomeval_nbis_q_tree[p+1].lenx = biomeval_nbis_q_tree[p].lenx;
      biomeval_nbis_q_tree[p+2].lenx = biomeval_nbis_q_tree[p].lenx;
      biomeval_nbis_q_tree[p+3].lenx = biomeval_nbis_q_tree[p].lenx;
   }
   else {
      biomeval_nbis_q_tree[p].lenx = (lenx + 1) / 2;
      biomeval_nbis_q_tree[p+1].lenx = biomeval_nbis_q_tree[p].lenx - 1;
      biomeval_nbis_q_tree[p+2].lenx = biomeval_nbis_q_tree[p].lenx;
      biomeval_nbis_q_tree[p+3].lenx = biomeval_nbis_q_tree[p+1].lenx;
   }
   biomeval_nbis_q_tree[p+1].x = x + biomeval_nbis_q_tree[p].lenx;
   biomeval_nbis_q_tree[p+3].x = biomeval_nbis_q_tree[p+1].x;
   if(eveny == 0) {
      biomeval_nbis_q_tree[p].leny = leny / 2;
      biomeval_nbis_q_tree[p+1].leny = biomeval_nbis_q_tree[p].leny;
      biomeval_nbis_q_tree[p+2].leny = biomeval_nbis_q_tree[p].leny;
      biomeval_nbis_q_tree[p+3].leny = biomeval_nbis_q_tree[p].leny;
   }
   else {
      biomeval_nbis_q_tree[p].leny = (leny + 1) / 2;
      biomeval_nbis_q_tree[p+1].leny = biomeval_nbis_q_tree[p].leny;
      biomeval_nbis_q_tree[p+2].leny = biomeval_nbis_q_tree[p].leny - 1;
      biomeval_nbis_q_tree[p+3].leny = biomeval_nbis_q_tree[p+2].leny;
   }
   biomeval_nbis_q_tree[p+2].y = y + biomeval_nbis_q_tree[p].leny;
   biomeval_nbis_q_tree[p+3].y = biomeval_nbis_q_tree[p+2].y;
}
