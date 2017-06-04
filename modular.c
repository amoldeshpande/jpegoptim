#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <jpeglib.h>
#include "jpegoptim.h"
#include "modular.h"
#if BUILD_STATIC_LIB

void init_options(struct jpegoptim_options*options)
{
    options->quality = -1;
    options->strip_exif = 0; 
    options->strip_iptc = 0;
    options->strip_icc = 0;
    options->strip_com = 0;
    options->strip_xmp = 0;
    options->strip_none = 0;
    options->threshold = -1;
    options->all_normal = 0;
    options->all_progressive = 0;
    options->sizeKB = 0;
}
JSAMPARRAY do_decompress(struct jpeg_decompress_struct* pdinfo,
               unsigned char* inbuffer, 
               size_t insize,
               struct marker_context* marker_ctx,
               int quality,
               int retry,
              jvirt_barray_ptr **coef_arrays)
{
    int j;
    JSAMPARRAY buf = NULL;
    jpeg_save_markers(pdinfo, JPEG_COM, 0xffff);
    for (j = 0; j <= 15; j++)
        jpeg_save_markers(pdinfo, JPEG_APP0 + j, 0xffff);
    jpeg_mem_src(pdinfo, inbuffer,(unsigned long)insize);
    jpeg_read_header(pdinfo, TRUE);

    /* check for Exif/IPTC/ICC/XMP markers */
    marker_ctx->marker_str[0] = 0;
    marker_ctx->marker_in_count = 0;
    marker_ctx->marker_in_size = 0;
    marker_ctx->cmarker = pdinfo->marker_list;

    while (marker_ctx->cmarker) {
        marker_ctx->marker_in_count++;
        marker_ctx->marker_in_size += marker_ctx->cmarker->data_length;

        if (marker_ctx->cmarker->marker == EXIF_JPEG_MARKER &&
            marker_ctx->cmarker->data_length >= EXIF_IDENT_STRING_SIZE &&
            !memcmp(marker_ctx->cmarker->data, EXIF_IDENT_STRING, EXIF_IDENT_STRING_SIZE))
            strncat(marker_ctx->marker_str, "Exif ", sizeof(marker_ctx->marker_str) - strlen(marker_ctx->marker_str) - 1);

        if (marker_ctx->cmarker->marker == IPTC_JPEG_MARKER)
            strncat(marker_ctx->marker_str, "IPTC ", sizeof(marker_ctx->marker_str) - strlen(marker_ctx->marker_str) - 1);

        if (marker_ctx->cmarker->marker == ICC_JPEG_MARKER &&
            marker_ctx->cmarker->data_length >= ICC_IDENT_STRING_SIZE &&
            !memcmp(marker_ctx->cmarker->data, ICC_IDENT_STRING, ICC_IDENT_STRING_SIZE))
            strncat(marker_ctx->marker_str, "ICC ", sizeof(marker_ctx->marker_str) - strlen(marker_ctx->marker_str) - 1);

        if (marker_ctx->cmarker->marker == XMP_JPEG_MARKER &&
            marker_ctx->cmarker->data_length >= XMP_IDENT_STRING_SIZE &&
            !memcmp(marker_ctx->cmarker->data, XMP_IDENT_STRING, XMP_IDENT_STRING_SIZE))
            strncat(marker_ctx->marker_str, "XMP ", sizeof(marker_ctx->marker_str) - strlen(marker_ctx->marker_str) - 1);

        marker_ctx->cmarker = marker_ctx->cmarker->next;
    }
  /* decompress the file */
   if (quality>=0 && !retry) {
     jpeg_start_decompress(pdinfo);

     /* allocate line buffer to store the decompressed image */
     buf = malloc(sizeof(JSAMPROW)*pdinfo->output_height);
     if (!buf) fatal("not enough memory");
     for (unsigned int j=0;j<pdinfo->output_height;j++) {
       buf[j]=malloc(sizeof(JSAMPLE)*pdinfo->output_width*
             pdinfo->out_color_components);
       if (!buf[j]) fatal("not enough memory");
     }

     while (pdinfo->output_scanline < pdinfo->output_height) {
       jpeg_read_scanlines(pdinfo,&buf[pdinfo->output_scanline],
               pdinfo->output_height-pdinfo->output_scanline);
     }
   } else {
     *coef_arrays = jpeg_read_coefficients(pdinfo);
   }

   return buf;
}
unsigned char* binary_search_size(struct jpeg_compress_struct* pcinfo,
                         struct jpeg_decompress_struct* pdinfo,
                         size_t* outsize, 
                         size_t insize,
                         size_t target_size,
                         JSAMPARRAY buf,
                         jvirt_barray_ptr *coef_arrays,
                         int quality,
                         int retry,
                         int all_normal,
                         int all_progressive,
                         struct jpegoptim_options* options)
{
    unsigned char* outbuffer = NULL;
    size_t outbuffersize = *outsize;
    size_t lastsize = 0;
    int searchcount = 0;
    int searchdone = 0;
    int         oldquality = 200;

binary_search_loop:

    /* allocate memory buffer that should be large enough to store the output JPEG... */
    if (outbuffer) free(outbuffer);
    outbuffersize = insize + 32768;
    outbuffer = malloc(outbuffersize);
    if (!outbuffer) fatal("not enough memory");

    /* setup custom "destination manager" for libjpeg to write to our buffer */
    jpeg_memory_dest(pcinfo, &outbuffer, &outbuffersize, 65536);

    if (quality >= 0 && !retry) {
        /* lossy "optimization" ... */

        pcinfo->in_color_space = pdinfo->out_color_space;
        pcinfo->input_components = pdinfo->output_components;
        pcinfo->image_width = pdinfo->image_width;
        pcinfo->image_height = pdinfo->image_height;
        jpeg_set_defaults(pcinfo);
        jpeg_set_quality(pcinfo, quality, TRUE);
        if (all_normal) {
            pcinfo->scan_info = NULL; // Explicitly disables progressive if libjpeg had it on by default
            pcinfo->num_scans = 0;
        }
        else if (pdinfo->progressive_mode || all_progressive) {
            jpeg_simple_progression(pcinfo);
        }
        pcinfo->optimize_coding = TRUE;

        unsigned int j = 0;
        jpeg_start_compress(pcinfo, TRUE);

        /* write markers */
        write_markers(pdinfo, pcinfo,options->strip_com? 0 : 1,options->strip_iptc ?  0 : 1,options->strip_exif?0:1,options->strip_icc?0:1,options->strip_xmp?0:1,options->strip_none);

        /* write image */
        while (pcinfo->next_scanline < pcinfo->image_height) {
            jpeg_write_scanlines(pcinfo, &buf[pcinfo->next_scanline], pdinfo->output_height);
        }

    }
    else {
        /* lossless "optimization" ... */

        jpeg_copy_critical_parameters(pdinfo, pcinfo);
        if (all_normal) {
            pcinfo->scan_info = NULL; // Explicitly disables progressive if libjpeg had it on by default
            pcinfo->num_scans = 0;
        }
        else if (pdinfo->progressive_mode || all_progressive) {
            jpeg_simple_progression(pcinfo);
        }
        pcinfo->optimize_coding = TRUE;

        /* write image */
        jpeg_write_coefficients(pcinfo, coef_arrays);

        /* write markers */
        write_markers(pdinfo, pcinfo,options->strip_com? 0 : 1,options->strip_iptc ?  0 : 1,options->strip_exif?0:1,options->strip_icc?0:1,options->strip_xmp?0:1,options->strip_none);

    }

    jpeg_finish_compress(pcinfo);
    *outsize = outbuffersize;

    if (target_size != 0 && !retry) {
        /* perform (binary) search to try to reach target file size... */

        size_t osize = *outsize / 1024;
        size_t isize = insize / 1024;
        size_t tsize = target_size;

        if (tsize < 0) {
            tsize = ((-target_size)*insize / 100) / 1024;
            if (tsize < 1) tsize = 1;
        }

        if (osize == tsize || searchdone || searchcount >= 8 || tsize > isize) {
            if (searchdone < 42 && lastsize > 0) {
                if (labs(osize - tsize) > labs(lastsize - tsize)) {
                    searchdone = 42;
                    quality = oldquality;
                    goto binary_search_loop;
                }
            }

        }
        else {
            int newquality;
            int dif = floor((abs(oldquality - quality) / 2.0) + 0.5);
            if (osize > tsize) {
                newquality = quality - dif;
                if (dif < 1) { newquality--; searchdone = 1; }
                if (newquality < 0) { newquality = 0; searchdone = 2; }
            }
            else {
                newquality = quality + dif;
                if (dif < 1) { newquality++; searchdone = 3; }
                if (newquality > 100) { newquality = 100; searchdone = 4; }
            }
            oldquality = quality;
            quality = newquality;

            dprintf("(try %d)", quality);

            lastsize = osize;
            searchcount++;
            goto binary_search_loop;
        }
    }

    return outbuffer;
}
#endif //  BUILD_STATIC_LIB
void write_markers(struct jpeg_decompress_struct *dinfo, struct jpeg_compress_struct *cinfo,int save_com,int save_iptc,int save_exif,int save_icc, int save_xmp,int strip_none)
{
    jpeg_saved_marker_ptr mrk;
    int write_marker;

    if (!cinfo || !dinfo) fatal("invalid call to write_markers()");

    mrk = dinfo->marker_list;
    while (mrk) {
        write_marker = 0;

        /* check for markers to save... */

        if (save_com && mrk->marker == JPEG_COM)
            write_marker++;

        if (save_iptc && mrk->marker == IPTC_JPEG_MARKER)
            write_marker++;

        if (save_exif && mrk->marker == EXIF_JPEG_MARKER &&
            mrk->data_length >= EXIF_IDENT_STRING_SIZE &&
            !memcmp(mrk->data, EXIF_IDENT_STRING, EXIF_IDENT_STRING_SIZE))
            write_marker++;

        if (save_icc && mrk->marker == ICC_JPEG_MARKER &&
            mrk->data_length >= ICC_IDENT_STRING_SIZE &&
            !memcmp(mrk->data, ICC_IDENT_STRING, ICC_IDENT_STRING_SIZE))
            write_marker++;

        if (save_xmp && mrk->marker == XMP_JPEG_MARKER &&
            mrk->data_length >= XMP_IDENT_STRING_SIZE &&
            !memcmp(mrk->data, XMP_IDENT_STRING, XMP_IDENT_STRING_SIZE))
            write_marker++;

        if (strip_none) write_marker++;


        /* libjpeg emits some markers automatically so skip these to avoid duplicates... */

        /* skip JFIF (APP0) marker */
        if (mrk->marker == JPEG_APP0 && mrk->data_length >= 14 &&
            mrk->data[0] == 0x4a &&
            mrk->data[1] == 0x46 &&
            mrk->data[2] == 0x49 &&
            mrk->data[3] == 0x46 &&
            mrk->data[4] == 0x00)
            write_marker = 0;

        /* skip Adobe (APP14) marker */
        if (mrk->marker == JPEG_APP0 + 14 && mrk->data_length >= 12 &&
            mrk->data[0] == 0x41 &&
            mrk->data[1] == 0x64 &&
            mrk->data[2] == 0x6f &&
            mrk->data[3] == 0x62 &&
            mrk->data[4] == 0x65)
            write_marker = 0;



        if (write_marker)
            jpeg_write_marker(cinfo, mrk->marker, mrk->data, mrk->data_length);

        mrk = mrk->next;
    }
}
