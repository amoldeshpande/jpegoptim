/*******************************************************************
 * JPEGoptim
 * Copyright (c) Timo Kokkonen, 1996-2016.
 * All Rights Reserved.
 *
 * requires libjpeg (Independent JPEG Group's JPEG software
 *                     release 6a or later...)
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef _WIN32
#include <dirent.h>
#else
#pragma warning(disable:4996 4003) // posix name warning and other crap
#endif
#if HAVE_GETOPT_H && HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "getopt.h"
#endif
#include <signal.h>
#include <string.h>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include <time.h>
#include <math.h>

#include "jpegoptim.h"
#include  "modular.h"


#define VERSIO "1.4.4"
#define COPYRIGHT  "Copyright (c) 1996-2016, Timo Kokkonen"


#define LOG_FH (logs_to_stdout ? stdout : stderr)


struct my_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
    int     jump_set;
};
typedef struct my_error_mgr * my_error_ptr;

const char *rcsid = "$Id$";


int verbose_mode = 0;
int quiet_mode = 0;
int global_error_counter = 0;
int preserve_mode = 0;
int preserve_perms = 0;
int overwrite_mode = 0;
int totals_mode = 0;
int stdin_mode = 0;
int stdout_mode = 0;
int noaction = 0;
int quality = -1;
int retry = 0;
int dest = 0;
int force = 0;
int save_exif = 1;
int save_iptc = 1;
int save_com = 1;
int save_icc = 1;
int save_xmp = 1;
int strip_none = 0;
int threshold = -1;
int csv = 0;
int all_normal = 0;
int all_progressive = 0;
int target_size = 0;
int logs_to_stdout = 1;


struct option long_options[] = {
  {"verbose",0,0,'v'},
  {"help",0,0,'h'},
  {"quiet",0,0,'q'},
  {"max",1,0,'m'},
  {"totals",0,0,'t'},
  {"noaction",0,0,'n'},
  {"dest",1,0,'d'},
  {"force",0,0,'f'},
  {"version",0,0,'V'},
  {"preserve",0,0,'p'},
  {"preserve-perms",0,0,'P'},
  {"strip-all",0,0,'s'},
  {"strip-none",0,&strip_none,1},
  {"strip-com",0,&save_com,0},
  {"strip-exif",0,&save_exif,0},
  {"strip-iptc",0,&save_iptc,0},
  {"strip-icc",0,&save_icc,0},
  {"strip-xmp",0,&save_xmp,0},
  {"threshold",1,0,'T'},
  {"csv",0,0,'b'},
  {"all-normal",0,&all_normal,1},
  {"all-progressive",0,&all_progressive,1},
  {"size",1,0,'S'},
  {"stdout",0,&stdout_mode,1},
  {"stdin",0,&stdin_mode,1},
  {0,0,0,0}
};


/*****************************************************************/

METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
    my_error_ptr myerr = (my_error_ptr)cinfo->err;

    (*cinfo->err->output_message) (cinfo);
    if (myerr->jump_set)
        longjmp(myerr->setjmp_buffer, 1);
    else
        fatal("fatal error");
}

METHODDEF(void)
my_output_message(j_common_ptr cinfo)
{
    char buffer[JMSG_LENGTH_MAX + 1];

    if (verbose_mode) {
        (*cinfo->err->format_message)((j_common_ptr)cinfo, buffer);
        buffer[sizeof(buffer) - 1] = 0;
        fprintf(LOG_FH, " (%s) ", buffer);
    }
    global_error_counter++;
}


void print_usage(void)
{
    fprintf(stderr, PROGRAMNAME " v" VERSIO "  " COPYRIGHT "\n");

    fprintf(stderr,
        "Usage: " PROGRAMNAME " [options] <filenames> \n\n"
        "  -d<path>, --dest=<path>\n"
        "                    specify alternative destination directory for \n"
        "                    optimized files (default is to overwrite originals)\n"
        "  -f, --force       force optimization\n"
        "  -h, --help        display this help and exit\n"
        "  -m<quality>, --max=<quality>\n"
        "                    set maximum image quality factor (disables lossless\n"
        "                    optimization mode, which is by default on)\n"
        "                    Valid quality values: 0 - 100\n"
        "  -n, --noaction    don't really optimize files, just print results\n"
        "  -S<size>, --size=<size>\n"
        "                    Try to optimize file to given size (disables lossless\n"
        "                    optimization mode). Target size is specified either in\n"
        "                    kilo bytes (1 - n) or as percentage (1%% - 99%%)\n"
        "  -T<threshold>, --threshold=<threshold>\n"
        "                    keep old file if the gain is below a threshold (%%)\n"
        "  -b, --csv         print progress info in CSV format\n"
        "  -o, --overwrite   overwrite target file even if it exists (meaningful\n"
        "                    only when used with -d, --dest option)\n"
        "  -p, --preserve    preserve file timestamps\n"
        "  -P, --preserve-perms\n"
        "                    preserve original file permissions by overwriting it\n"
        "  -q, --quiet       quiet mode\n"
        "  -t, --totals      print totals after processing all files\n"
        "  -v, --verbose     enable verbose mode (positively chatty)\n"
        "  -V, --version     print program version\n\n"
        "  -s, --strip-all   strip all markers from output file\n"
        "  --strip-none      do not strip any markers\n"
        "  --strip-com       strip Comment markers from output file\n"
        "  --strip-exif      strip Exif markers from output file\n"
        "  --strip-iptc      strip IPTC/Photoshop (APP13) markers from output file\n"
        "  --strip-icc       strip ICC profile markers from output file\n"
        "  --strip-xmp       strip XMP markers markers from output file\n"
        "\n"
        "  --all-normal      force all output files to be non-progressive\n"
        "  --all-progressive force all output files to be progressive\n"
        "  --stdout          send output to standard output (instead of a file)\n"
        "  --stdin           read input from standard input (instead of a file)\n"
        "\n\n");
}

void print_version()
{
    struct jpeg_error_mgr jcerr, *err;


    printf(PROGRAMNAME " v%s  %s\n", VERSIO, HOST_TYPE);
    printf(COPYRIGHT "\n");

    if (!(err = jpeg_std_error(&jcerr)))
        fatal("jpeg_std_error() failed");

    printf("\nlibjpeg version: %s\n%s\n",
        err->jpeg_message_table[JMSG_VERSION],
        err->jpeg_message_table[JMSG_COPYRIGHT]);
}


void own_signal_handler(int a)
{
    if (verbose_mode > 1)
        fprintf(stderr, PROGRAMNAME ": signal: %d\n", a);
    exit(1);
}











/*****************************************************************/
#ifndef BUILD_STATIC_LIB
int main(int argc, char **argv)
{
    struct jpeg_decompress_struct dinfo;
    struct jpeg_compress_struct cinfo;
    struct my_error_mgr jcerr, jderr;
    JSAMPARRAY buf = NULL;
    jvirt_barray_ptr *coef_arrays = NULL;
    char tmpfilename[MAXPATHLEN], tmpdir[MAXPATHLEN];
    char newname[MAXPATHLEN], dest_path[MAXPATHLEN];
    volatile int i;
    int c, tmpfd, searchcount, searchdone;
    int opt_index = 0;
    size_t insize = 0, outsize = 0, lastsize = 0;
    int oldquality;
    double ratio;
    struct stat file_stat;
    unsigned char *outbuffer = NULL;
    size_t outbuffersize;
    char *outfname = NULL;
    FILE *infile = NULL, *outfile = NULL;
    char* infilename = NULL;
    unsigned char* infilebuffer = NULL;
    int compress_err_count = 0;
    int decompress_err_count = 0;
    long average_count = 0;
    double average_rate = 0.0, total_save = 0.0;


    if (rcsid)
        ; /* so compiler won't complain about "unused" rcsid string */

    umask(077);
    signal(SIGINT, own_signal_handler);
    signal(SIGTERM, own_signal_handler);

    /* initialize decompression object */
    dinfo.err = jpeg_std_error(&jderr.pub);
    jpeg_create_decompress(&dinfo);
    jderr.pub.error_exit = my_error_exit;
    jderr.pub.output_message = my_output_message;
    jderr.jump_set = 0;

    /* initialize compression object */
    cinfo.err = jpeg_std_error(&jcerr.pub);
    jpeg_create_compress(&cinfo);
    jcerr.pub.error_exit = my_error_exit;
    jcerr.pub.output_message = my_output_message;
    jcerr.jump_set = 0;


    if (argc < 2) {
        if (!quiet_mode) fprintf(stderr, PROGRAMNAME ": file arguments missing\n"
            "Try '" PROGRAMNAME " --help' for more information.\n");
        exit(1);
    }

    /* parse command line parameters */
    while (1) {
        opt_index = 0;
        if ((c = getopt_long(argc, argv, "d:hm:nstqvfVpPoT:S:b", long_options, &opt_index))
            == -1)
            break;

        switch (c) {
            case 'm':
                {
                    int tmpvar;

                    if (sscanf(optarg, "%d", &tmpvar) == 1) {
                        quality = tmpvar;
                        if (quality < 0) quality = 0;
                        if (quality > 100) quality = 100;
                    }
                    else
                        fatal("invalid argument for -m, --max");
                }
                break;
            case 'd':
                if (realpath(optarg, dest_path) == NULL)
                    fatal("invalid destination directory: %s", optarg);
                if (!is_directory(dest_path))
                    fatal("destination not a directory: %s", dest_path);
                strncat(dest_path, DIR_SEPARATOR_S, sizeof(dest_path) - strlen(dest_path) - 1);

                if (verbose_mode)
                    fprintf(stderr, "Destination directory: %s\n", dest_path);
                dest = 1;
                break;
            case 'v':
                verbose_mode++;
                break;
            case 'h':
                print_usage();
                exit(0);
                break;
            case 'q':
                quiet_mode = 1;
                break;
            case 't':
                totals_mode = 1;
                break;
            case 'n':
                noaction = 1;
                break;
            case 'f':
                force = 1;
                break;
            case 'b':
                csv = 1;
                quiet_mode = 1;
                break;
            case '?':
                break;
            case 'V':
                print_version();
                exit(0);
                break;
            case 'o':
                overwrite_mode = 1;
                break;
            case 'p':
                preserve_mode = 1;
                break;
            case 'P':
                preserve_perms = 1;
                break;
            case 's':
                save_exif = 0;
                save_iptc = 0;
                save_com = 0;
                save_icc = 0;
                save_xmp = 0;
                break;
            case 'T':
                {
                    int tmpvar;
                    if (sscanf(optarg, "%d", &tmpvar) == 1) {
                        threshold = tmpvar;
                        if (threshold < 0) threshold = 0;
                        if (threshold > 100) threshold = 100;
                    }
                    else fatal("invalid argument for -T, --threshold");
                }
                break;
            case 'S':
                {
                    int tmpvar;
                    if (sscanf(optarg, "%u", &tmpvar) == 1) {
                        if (tmpvar > 0 && tmpvar < 100 && optarg[strlen(optarg) - 1] == '%') {
                            target_size = -tmpvar;
                        }
                        else {
                            target_size = tmpvar;
                        }
                        quality = 100;
                    }
                    else fatal("invalid argument for -S, --size");
                }
                break;

        }
    }


    /* check for '-' option indicating input is from stdin... */
    i = 1;
    while (argv[i]) {
        if (argv[i][0] == '-' && argv[i][1] == 0) stdin_mode = 1;
        i++;
    }

    if (stdin_mode) { stdout_mode = 1; force = 1; }
    if (stdout_mode) { logs_to_stdout = 0; }

    if (all_normal && all_progressive)
        fatal("cannot specify both --all-normal and --all-progressive");

    if (verbose_mode) {
        if (quality >= 0 && target_size == 0)
            fprintf(stderr, "Image quality limit set to: %d\n", quality);
        if (threshold >= 0)
            fprintf(stderr, "Compression threshold (%%) set to: %d\n", threshold);
        if (all_normal)
            fprintf(stderr, "All output files will be non-progressive\n");
        if (all_progressive)
            fprintf(stderr, "All output files will be progressive\n");
        if (target_size > 0)
            fprintf(stderr, "Target size for output files set to: %u Kbytes.\n",
                target_size);
        if (target_size < 0)
            fprintf(stderr, "Target size for output files set to: %u%%\n",
                -target_size);
    }


    /* loop to process the input files */
    i = (optind > 0 ? optind : 1);
    do {
        if (stdin_mode) {
            infile = stdin;
            set_filemode_binary(infile);
        }
        else {
            if (!argv[i][0]) continue;
            if (argv[i][0] == '-') continue;
            if (strlen(argv[i]) >= MAXPATHLEN) {
                warn("skipping too long filename: %s", argv[i]);
                continue;
            }

            if (!noaction) {
                /* generate tmp dir & new filename */
                if (dest) {
                    STRNCPY(tmpdir, dest_path, sizeof(tmpdir));
                    STRNCPY(newname, dest_path, sizeof(newname));
                    if (!splitname(argv[i], tmpfilename, sizeof(tmpfilename)))
                        fatal("splitname() failed for: %s", argv[i]);
                    strncat(newname, tmpfilename, sizeof(newname) - strlen(newname) - 1);
                }
                else {
                    if (!splitdir(argv[i], tmpdir, sizeof(tmpdir)))
                        fatal("splitdir() failed for: %s", argv[i]);
                    STRNCPY(newname, argv[i], sizeof(newname));
                }
            }

retry_point:

            if (!is_file(argv[i], &file_stat)) {
                if (is_directory(argv[i]))
                    warn("skipping directory: %s", argv[i]);
                else
                    warn("skipping special file: %s", argv[i]);
                continue;
            }
            if ((infile = fopen(argv[i], "rb")) == NULL) {
                warn("cannot open file: %s", argv[i]);
                continue;
            }
            infilename = argv[i];
        }

        if (setjmp(jderr.setjmp_buffer)) {
            /* error handler for decompress */
            jpeg_abort_decompress(&dinfo);
            fclose(infile);
            if (buf) FREE_LINE_BUF(buf, dinfo.output_height);
            if (!quiet_mode || csv)
                fprintf(LOG_FH, csv ? ",,,,,error\n" : " [ERROR]\n");
            decompress_err_count++;
            jderr.jump_set = 0;
            continue;
        }
        else {
            jderr.jump_set = 1;
        }

        if (!retry && (!quiet_mode || csv)) {
            fprintf(LOG_FH, csv ? "%s," : "%s ", (stdin_mode ? "stdin" : argv[i])); fflush(LOG_FH);
        }


        /* prepare to decompress */
        global_error_counter = 0;
        if(infilebuffer){free(infilebuffer); }
        infilebuffer = read_all_bytes(infilename, &insize);
        struct marker_context marker_ctx;
        buf = do_decompress(&dinfo, infilebuffer, insize, &marker_ctx, quality, retry, &coef_arrays);

        if (!retry && !quiet_mode) {
            if (global_error_counter == 0) fprintf(LOG_FH, " [OK] ");
            else fprintf(LOG_FH, " [WARNING] ");
            fflush(LOG_FH);
        }



        if (dest && !noaction) {
            if (file_exists(newname) && !overwrite_mode) {
                warn("target file already exists: %s\n", newname);
                jpeg_abort_decompress(&dinfo);
                fclose(infile);
                if (buf) FREE_LINE_BUF(buf, dinfo.output_height);
                continue;
            }
        }


        if (setjmp(jcerr.setjmp_buffer)) {
            /* error handler for compress failures */

            jpeg_abort_compress(&cinfo);
            jpeg_abort_decompress(&dinfo);
            fclose(infile);
            if (!quiet_mode) fprintf(LOG_FH, " [Compress ERROR]\n");
            if (buf) FREE_LINE_BUF(buf, dinfo.output_height);
            compress_err_count++;
            jcerr.jump_set = 0;
            continue;
        }
        else {
            jcerr.jump_set = 1;
        }


        lastsize = 0;
        searchcount = 0;
        searchdone = 0;
        oldquality = 200;

        struct jpegoptim_options options;
        init_options(&options);
        options.all_normal = all_normal;
        options.all_progressive = all_progressive;
        options.force = force;
        options.quality = quality;
        options.sizeKB = target_size;
        options.strip_all = 1 - strip_none;
        options.strip_com = 1- save_com ;
        options.strip_iptc = 1- save_iptc;
        options.strip_exif = 1- save_exif;
        options.strip_icc = 1- save_icc ;
        options.strip_xmp = 1- save_xmp;
        options.strip_none = strip_none;
        outbuffer = binary_search_size(&cinfo, &dinfo, &outbuffersize, insize, target_size, buf, coef_arrays, quality, retry, all_normal, all_progressive,&options);
        outsize = outbuffersize;


        if (buf) FREE_LINE_BUF(buf, dinfo.output_height);
        jpeg_finish_decompress(&dinfo);
        fclose(infile);


        if (quality >= 0 && outsize >= insize && !retry && !stdin_mode) {
            if (verbose_mode) fprintf(LOG_FH, "(retry w/lossless) ");
            retry = 1;
            goto retry_point;
        }

        retry = 0;
        ratio = (insize - outsize)*100.0 / insize;
        if (!quiet_mode || csv)
            fprintf(LOG_FH, csv ? "%ld,%ld,%0.2f," : "%ld --> %ld bytes (%0.2f%%), ", insize, outsize, ratio);
        average_count++;
        average_rate += (ratio < 0 ? 0.0 : ratio);

        if ((outsize < insize && ratio >= threshold) || force) {
            total_save += (insize - outsize) / 1024.0;
            if (!quiet_mode || csv) fprintf(LOG_FH, csv ? "optimized\n" : "optimized.\n");
            if (noaction) continue;

            if (stdout_mode) {
                outfname = NULL;
                set_filemode_binary(stdout);
                if (fwrite(outbuffer, outbuffersize, 1, stdout) != 1)
                    fatal("%s, write failed to stdout", (stdin_mode ? "stdin" : argv[i]));
            }
            else {
                if (preserve_perms && !dest) {
                    /* make backup of the original file */
                    snprintf(tmpfilename, sizeof(tmpfilename), "%s.jpegoptim.bak", newname);
                    if (verbose_mode > 1 && !quiet_mode)
                        fprintf(LOG_FH, "%s, creating backup as: %s\n", (stdin_mode ? "stdin" : argv[i]), tmpfilename);
                    if (file_exists(tmpfilename))
                        fatal("%s, backup file already exists: %s", (stdin_mode ? "stdin" : argv[i]), tmpfilename);
                    if (copy_file(newname, tmpfilename))
                        fatal("%s, failed to create backup: %s", (stdin_mode ? "stdin" : argv[i]), tmpfilename);
                    if ((outfile = fopen(newname, "wb")) == NULL)
                        fatal("%s, error opening output file: %s", (stdin_mode ? "stdin" : argv[i]), newname);
                    outfname = newname;
                }
                else {
#ifdef HAVE_MKSTEMPS
                    /* rely on mkstemps() to create us temporary file safely... */
                    snprintf(tmpfilename, sizeof(tmpfilename),
                        "%sjpegoptim-%d-%d.XXXXXX.tmp", tmpdir, (int)getuid(), (int)getpid());
                    if ((tmpfd = mkstemps(tmpfilename, 4)) < 0)
                        fatal("%s, error creating temp file %s: mkstemps() failed", (stdin_mode ? "stdin" : argv[i]), tmpfilename);
                    if ((outfile = fdopen(tmpfd, "wb")) == NULL)
#else
                    /* if platform is missing mkstemps(), try to create at least somewhat "safe" temp file... */
                    snprintf(tmpfilename, sizeof(tmpfilename),
                        "%sjpegoptim-%d-%d.%ld.tmp", tmpdir, (int)getuid(), (int)getpid(), (long)time(NULL));
                    tmpfd = 0;
                    if ((outfile = fopen(tmpfilename, "wb")) == NULL)
#endif
                        fatal("error opening temporary file: %s", tmpfilename);
                    outfname = tmpfilename;
                }

                if (verbose_mode > 1 && !quiet_mode)
                    fprintf(LOG_FH, "writing %lu bytes to file: %s\n",
                    (long unsigned int)outbuffersize, outfname);
                if (fwrite(outbuffer, outbuffersize, 1, outfile) != 1)
                    fatal("write failed to file: %s", outfname);
                fclose(outfile);
            }

            if (outfname) {

                if (preserve_mode) {
                    /* preserve file modification time */
                    struct utimbuf time_save;
                    time_save.actime = file_stat.st_atime;
                    time_save.modtime = file_stat.st_mtime;
                    if (utime(outfname, &time_save) != 0)
                        warn("failed to reset output file time/date");
                }

                if (preserve_perms && !dest) {
                    /* original file was already replaced, remove backup... */
                    if (delete_file(tmpfilename))
                        warn("failed to remove backup file: %s", tmpfilename);
                }
                else {
                    /* make temp file to be the original file... */

                    /* preserve file mode */
                    if (chmod(outfname, (file_stat.st_mode & 0777)) != 0)
                        warn("failed to set output file mode");

                    /* preserve file group (and owner if run by root) */
                    if (chown(outfname,
                        (geteuid() == 0 ? file_stat.st_uid : -1),
                        file_stat.st_gid) != 0)
                        warn("failed to reset output file group/owner");

                    if (verbose_mode > 1 && !quiet_mode)
                        fprintf(LOG_FH, "renaming: %s to %s\n", outfname, newname);
                    if (rename_file(outfname, newname)) fatal("cannot rename temp file");
                }
            }
        }
        else {
            if (!quiet_mode || csv) fprintf(LOG_FH, csv ? "skipped\n" : "skipped.\n");
        }


    } while (++i < argc && !stdin_mode);


    if (totals_mode && !quiet_mode)
        fprintf(LOG_FH, "Average ""compression"" (%ld files): %0.2f%% (%0.0fk)\n",
            average_count, average_rate / average_count, total_save);
    jpeg_destroy_decompress(&dinfo);
    jpeg_destroy_compress(&cinfo);

    return (decompress_err_count > 0 || compress_err_count > 0 ? 1 : 0);;
}
#endif // BUILD_STATIC_LIB

/* :-) */
