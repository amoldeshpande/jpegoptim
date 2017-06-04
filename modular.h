#pragma once

#ifdef	__cplusplus
extern "C" {
#endif

#define FREE_LINE_BUF(buf,lines)  {				\
    unsigned int j;							\
    for (j=0;j<lines;j++) free(buf[j]);				\
    free(buf);							\
    buf=NULL;							\
  }

#define STRNCPY(dest,src,n) { strncpy(dest,src,n); dest[n-1]=0; }
#ifdef BUILD_STATIC_LIB

struct jpegoptim_options
{
    int force;
    int quality;
    size_t sizeKB;
    int threshold;
    int strip_all;
    int strip_none;
    int strip_com;
    int strip_exif;
    int strip_iptc;
    int strip_icc;
    int strip_xmp;
    int all_normal;
    int all_progressive;
};
extern void init_options(struct jpegoptim_options*options);
struct marker_context
{
    char marker_str[256];
    jpeg_saved_marker_ptr cmarker;
    int marker_in_count, marker_in_size;
};

JSAMPARRAY do_decompress(struct jpeg_decompress_struct* pdinfo,
                            unsigned char* inbuffer,
                            size_t insize,
                            struct marker_context* marker_ctx,
                            int quality,
                            int retry,
                            jvirt_barray_ptr **coef_arrays);

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
                            struct jpegoptim_options* options);

char* read_all_bytes(char const* filename, size_t* length);
void write_all_bytes(char const* filename, char* data, int datalen);

int call_jpegoptim(struct jpegoptim_options* options, unsigned char* inputbuf, size_t inputlen, unsigned char** outputbuf, size_t* outputlen);
#endif //BUILD_STATIC_LIB
void write_markers(struct jpeg_decompress_struct *dinfo, struct jpeg_compress_struct *cinfo, int save_com, int save_iptc, int save_exif, int save_icc, int save_xmp, int strip_none);

extern void dprintf(char*fmt, ...);
#ifdef	__cplusplus
}
#endif
