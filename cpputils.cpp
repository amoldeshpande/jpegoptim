#include <windows.h>
#include <stdio.h>
#include <fstream>
#include <jpeglib.h>
#include "jpegoptim.h"
#include "modular.h"
using namespace std;

extern "C" char* read_all_bytes(char const* filename, size_t* length)
{
    ifstream ifs(filename, ios::binary | ios::ate);
    ifstream::pos_type pos = ifs.tellg();

    *length = (size_t)pos;
    char* result = (char*)malloc((size_t)pos);
    if (result)
    {
        ifs.seekg(0, ios::beg);
        ifs.read(result, pos);
    }
    return result;
}
extern "C" void write_all_bytes(char const* filename, char* data, int datalen)
{
    ofstream ofs(filename, ios::binary | ios::ate);

    ofs.write(data, datalen);

}
extern "C" void dprintf(char* fmt, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, fmt, args);
    va_end(args);
    OutputDebugStringA(buf);

}

#if BUILD_STATIC_LIB
void lib_error_exit(j_common_ptr cinfo)
{
    struct jpeg_error_mgr* myerr = (struct jpeg_error_mgr*)cinfo->err;

    (*cinfo->err->output_message) (cinfo);

    throw new std::exception("fatal error");
    
}
int global_error_counter;
void do_throw(const char* str)
{
    throw new std::exception(str);
}
void fatal(const char *format, ...)
{
  va_list args;
  char buf[4096];

  OutputDebugStringA(PROGRAMNAME ": ");
  va_start(args,format);
  vsnprintf_s(buf,ARRAYSIZE(buf), format, args);
  va_end(args);
  OutputDebugStringA("\r\n");
  do_throw(buf);
}
void lib_output_message(j_common_ptr cinfo)
{
    char buffer[JMSG_LENGTH_MAX + 1];

    (*cinfo->err->format_message)((j_common_ptr)cinfo, buffer);
    buffer[sizeof(buffer) - 1] = 0;
    OutputDebugStringA(buffer);
    global_error_counter++;
}
int call_jpegoptim(struct jpegoptim_options* options, unsigned char* inputbuf, size_t inputlen, unsigned char** outputbuf, size_t* outputlen)
{
    struct jpeg_decompress_struct dinfo;
    struct jpeg_compress_struct cinfo;
    JSAMPARRAY buf = NULL;
    jvirt_barray_ptr *coef_arrays = NULL;
    int opt_index = 0;
    size_t insize = 0, lastsize = 0;
    unsigned char *outbuffer = NULL;
    char *outfname = NULL;
    int compress_err_count = 0;
    int decompress_err_count = 0;
    long average_count = 0;
    double average_rate = 0.0, total_save = 0.0;
    struct jpeg_error_mgr jderr, jcerr;
    int quality = -1;
    int save_exif = 1;
    int save_iptc = 1;
    int save_com = 1;
    int save_icc = 1;
    int save_xmp = 1;
    int threshold = -1;
    size_t target_size = 0;
    int all_normal = options->all_normal;
    int all_progressive = options->all_progressive;
    int retry = 0;

    /* initialize decompression object */
    dinfo.err = jpeg_std_error(&jderr);
    jpeg_create_decompress(&dinfo);
    jderr.error_exit = lib_error_exit;
    jderr.output_message = lib_output_message;

    /* initialize compression object */
    cinfo.err = jpeg_std_error(&jcerr);
    jpeg_create_compress(&cinfo);
    jcerr.error_exit = lib_error_exit;
    jcerr.output_message = lib_output_message;


    quality = options->quality;

    save_exif = 1- options->strip_exif;
    save_iptc = 1- options->strip_iptc;
    save_com = 1- options->strip_com ;
    save_icc = 1- options->strip_icc ;
    save_xmp = 1 - options->strip_xmp;
    if (options->strip_all)
    {
        save_exif = 0;
        save_iptc = 0;
        save_com = 0;
        save_icc = 0;
        save_xmp = 0;
    }
    if (options->strip_none)
    {
        save_exif = 1;
        save_iptc = 1;
        save_com = 1;
        save_icc = 1;
        save_xmp = 1;
    }
    threshold = options->threshold;
    if (threshold < 0) threshold = 0;
    if (threshold > 100) threshold = 100;

    target_size = options->sizeKB;
    if (target_size > 0 && (quality == -1) )
    {
        quality = 100;
    }


    if (all_normal && all_progressive)
        throw new std::exception("cannot specify both --all-normal and --all-progressive");


retry_point:

    try
    {
        struct marker_context markerContext;
        buf = do_decompress(&dinfo, inputbuf, inputlen, &markerContext, quality, retry, &coef_arrays);
        insize = inputlen;
        outbuffer = binary_search_size(&cinfo, &dinfo, outputlen,insize , target_size, buf, coef_arrays, quality, retry, all_normal, all_progressive,options);
        if (quality >= 0 && *outputlen >= insize && !retry) {
            retry = 1;
            free(outbuffer);
            goto retry_point;
        }
        *outputbuf = outbuffer;
        if (buf) FREE_LINE_BUF(buf, dinfo.output_height);
        jpeg_finish_decompress(&dinfo);
    }
    catch (std::exception& ex)
    {
        OutputDebugStringA(ex.what());
        /* error handler for decompress */
        jpeg_abort_decompress(&dinfo);
        if (buf) FREE_LINE_BUF(buf, dinfo.output_height);
        decompress_err_count++;
    }
    jpeg_destroy_decompress(&dinfo);
    jpeg_destroy_compress(&cinfo);

    return global_error_counter;
}
#endif // BUILD_STATIC_LIB
