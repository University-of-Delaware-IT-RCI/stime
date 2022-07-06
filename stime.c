/*
 * stime.c
 *
 * Slurm time string manipulations.
 *
 */

#include <errno.h>
#include <float.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#define __USE_XOPEN
#include <time.h>
#include <unistd.h>

#include "slurm/slurm.h"
#include "src/common/slurm_xlator.h"

/*
 * Command-line options for this utility:
 */
static struct option cli_options[] = {
        { "help",           no_argument,                NULL,   'h' },
        { "debug",          no_argument,                NULL,   'D' },
        { "quiet",          no_argument,                NULL,   'q' },
        { "reals",          no_argument,                NULL,   'r' },
        { "from",           required_argument,          NULL,   'F' },
        { "to",             required_argument,          NULL,   'T' },
        { "duration",       no_argument,                NULL,   'd' },
        { "timestamp",      no_argument,                NULL,   't' },
        { NULL,             0,                          NULL,   0   }
    };
static char *cli_options_str = "hDqrF:T:dt";

/*
 * In duration mode, the time is treated as a relative number of seconds
 * instead of an absolute offset.
 */
bool is_duration_mode = false;

/*
 * Is debug output desired?
 */
bool is_debug = false;

/*
 * Is truncated integer output desired?
 */
bool is_real_output = false;

/*
 * Each format will implement two functions:  one to convert from string to
 * seconds, one to convert from seconds to a string.
 */
typedef bool (*string_to_seconds_callback)(const char *stringValue, double *secondsValue);
typedef bool (*seconds_to_string_callback)(double secondsValue, char *stringValue, size_t stringValueLen);

typedef struct {
    const char                  *format_id;
    string_to_seconds_callback  parse_fn;
    seconds_to_string_callback  unparse_fn;
    const char                  *format_help;
} format_t;

/*
 * Slurm
 */
bool
string_to_seconds_slurm(
    const char  *stringValue,
    double      *secondsValue
)
{
    double      seconds;
    
    if ( is_duration_mode ) {
        int     slurm_seconds = slurm_time_str2secs(stringValue);    
        if ( slurm_seconds == NO_VAL ) return false;
        seconds = ( slurm_seconds == INFINITE ) ? INFINITY : (double)slurm_seconds;
    } else {
        time_t  slurm_ts = slurm_parse_time((char*)stringValue, 0);
        
        if ( slurm_ts == 0 ) return false;
        seconds = (double)slurm_ts;
    }
    *secondsValue = seconds;
    return true;
}
bool
seconds_to_string_slurm(
    double      secondsValue,
    char        *stringValue,
    size_t      stringValueLen
)
{
    time_t      slurm_ts = (time_t)secondsValue;
    
    if ( is_duration_mode ) {
        slurm_secs2time_str(slurm_ts, stringValue, (int)stringValueLen);
    } else {
        slurm_make_time_str(&slurm_ts, stringValue, (int)stringValueLen);
    }
    return true;
}

/*
 * Raw
 */
bool
string_to_seconds_raw(
    const char  *stringValue,
    double      *secondsValue
)
{
    double      seconds;
    char        *end;
    
    seconds = strtod(stringValue, &end);
    if ( end > stringValue ) {
        *secondsValue = seconds;
        return true;
    }
    return false;
}
bool
seconds_to_string_raw(
    double      secondsValue,
    char        *stringValue,
    size_t      stringValueLen
)
{
    int         n = snprintf(stringValue, stringValueLen, is_real_output? "%.3lf" : "%.0lf", secondsValue);
    
    if ( n == stringValueLen ) return false;
    return true;
}

/*
 * libc
 */
bool
string_to_seconds_libc(
    const char  *stringValue,
    double      *secondsValue
)
{
    double      seconds;
    
    if ( is_duration_mode ) {
        fprintf(stderr, "ERROR:  libc format cannot parse durations\n");
        return false;
    } else {
        struct tm   parsed_bits;
        time_t      to_seconds;
        
        memset(&parsed_bits, 0, sizeof(parsed_bits));
        if ( strptime(stringValue, "%c", &parsed_bits) == NULL ) return false;
        if ( is_debug ) printf(
                "DEBUG: strptime() => %d:%d:%d:%d:%d:%d:%d:%d:%d\n",
                parsed_bits.tm_sec,
                parsed_bits.tm_min,
                parsed_bits.tm_hour,
                parsed_bits.tm_mday,
                parsed_bits.tm_mon,
                parsed_bits.tm_year,
                parsed_bits.tm_wday,
                parsed_bits.tm_yday,
                parsed_bits.tm_isdst
            );
        to_seconds = mktime(&parsed_bits);
        if ( to_seconds == -1 ) return false;
        seconds = (double)to_seconds;
    }
    *secondsValue = seconds;
    return true;
}
bool
seconds_to_string_libc(
    double      secondsValue,
    char        *stringValue,
    size_t      stringValueLen
)
{
    time_t      slurm_ts = (time_t)secondsValue;
    
    if ( is_duration_mode ) {
        fprintf(stderr, "ERROR:  libc format cannot unparse durations\n");
        return false;
    } else {
        struct tm   unparsed_bits;
        time_t      libc_ts = (time_t)secondsValue;
        size_t      n;
        
        memset(&unparsed_bits, 0, sizeof(unparsed_bits));
        if ( localtime_r(&libc_ts, &unparsed_bits) == NULL ) return false;
        if ( is_debug ) printf(
                "DEBUG:  localtime_r() => %d:%d:%d:%d:%d:%d:%d:%d:%d\n",
                unparsed_bits.tm_sec,
                unparsed_bits.tm_min,
                unparsed_bits.tm_hour,
                unparsed_bits.tm_mday,
                unparsed_bits.tm_mon,
                unparsed_bits.tm_year,
                unparsed_bits.tm_wday,
                unparsed_bits.tm_yday,
                unparsed_bits.tm_isdst
            );
        n = strftime(stringValue, stringValueLen, "%c", &unparsed_bits);
        if ( n == stringValueLen ) return false;
    }
    return true;
}

/*
 *
 */

static format_t formats_registry[] = {
        { "libc", string_to_seconds_libc, seconds_to_string_libc, "strptime/strftime with locale-dependent times" },
        { "raw", string_to_seconds_raw, seconds_to_string_raw, "Values are a number of seconds" },
        { "slurm", string_to_seconds_slurm, seconds_to_string_slurm, "Slurm accepts a variety of timestamp and duration formats, please see the 'sbatch' man page" },
        { NULL, NULL, NULL, NULL }
    };

format_t*
format_for_id(
    const char  *format_id
)
{
    format_t    *formats = formats_registry;
    
    while ( formats->format_id ) {
        if ( strcasecmp(formats->format_id, format_id) == 0 ) return formats;
        formats++;
    }
    return NULL;
}

bool
format_is_valid(
    const char  *format_id
)
{
    return (format_for_id(format_id) != NULL);
}

/* */

#ifndef USAGE_FORMAT_HELP_WIDTH
#define USAGE_FORMAT_HELP_WIDTH   54
#endif

void
usage(
    const char      *exe
)
{
    format_t    *formats = formats_registry;
    
    printf(
            "usage:\n\n"
            "    %s {options} <value> {<value> ..}\n\n"
            "  options:\n\n"
            "    --help/-h                      show this information\n"
            "    --quiet/-q                     emit no error messages\n"
            "    --debug/-D                     emit extra info to stdout\n"
            "    --reals/-r                     unparse seconds as float instead of integer\n"
            "    --from/-F <format-id>          parse values in this format\n"
            "                                   (default: slurm)\n"
            "    --to/-T <format-id>            unparse values in this format\n"
            "                                   (default: raw)\n"
            "    --duration/-d                  values are interpreted as durations\n"
            "    --timestamp/-t                 values are interpreted as timestamps\n"
            "\n"
            "  <value>:\n\n"
            "    - if the <value> is a hyphen (-) then stdin is read, one <value> per line\n"
            "    - otherwise, the value is a string compliant with chosen --from/-F format\n"
            "\n"
            "  <format-id>:\n\n",
            exe
        );
    while ( formats->format_id ) {
        const char      *s = formats->format_help;
        int             n = -1;
        
        printf("    %-18s", formats->format_id);
        while ( *s ) {
            const char  *s2 = s;
            int         prev_ws = -1;
            
            if ( n > 0 ) printf("\n                      ");
            
            /* We want somewhere around 50 characters; go forward and back
             * from there to find the nearest whitespace:
             */
            n = 0;
            while ( *s2 && (n < USAGE_FORMAT_HELP_WIDTH) ) {
                if ( isspace(*s2) ) prev_ws = n;
                s2++;
                n++;
            }
            if ( *s2 && (n == USAGE_FORMAT_HELP_WIDTH) ) {
                while ( *s2 && ! isspace(*s2) ) {
                    s2++;
                    n++;
                }
                if ( (prev_ws >= 0) && ((USAGE_FORMAT_HELP_WIDTH - prev_ws) <= (n - USAGE_FORMAT_HELP_WIDTH)) ) {
                    s2 = s + prev_ws;
                } 
            }
            n = (s2 - s) + 1;
            n = printf(" %.*s", n, s);
            if ( n <= 0 ) break;
            /* Don't forget to drop the newline that was printed... */
            s += (n - 1);
        }
        printf("\n");
        formats++;
    }
}

/* */

int
main(
    int             argc,
    char * const    argv[]
)
{
    int             rc = 0;
    int             opt_ch;
    const char      *exe = argv[0];
    char            *from_format = "slurm";
    char            *to_format = "raw";
    bool            is_quiet = false;
    
    setlocale(LC_ALL, "");
    
    while ( ! rc && ((opt_ch = getopt_long(argc, argv, cli_options_str, cli_options, NULL)) != -1) ) {
        switch ( opt_ch ) {
        
            case 'h':
                usage(exe);
                break;
                
            case 'D':
                is_debug = true;
                break;
                
            case 'q':
                is_quiet = true;
                break;
            
            case 'r':
                is_real_output = true;
                break;
                
            case 'F': {
                if ( optarg && *optarg && format_is_valid(optarg) ) {
                    from_format = optarg;
                } else {
                    fprintf(stderr, "ERROR:  invalid 'from' format: %s\n", optarg);
                    rc = EINVAL;
                }
                break;
            }
            
            case 'T': {
                if ( optarg && *optarg && format_is_valid(optarg) ) {
                    to_format = optarg;
                } else {
                    fprintf(stderr, "ERROR:  invalid 'to' format: %s\n", optarg);
                    rc = EINVAL;
                }
                break;
            }
            
            case 'd':
                is_duration_mode = true;
                break;
            case 't':
                is_duration_mode = false;
                break;
        
        }
    }
    if ( ! rc ) {
        format_t        *parse = format_for_id(from_format);
        format_t        *unparse = format_for_id(to_format);
        bool            did_see_stdin = false;
        
        /*
         * Some of the APIs (ahem, Slurm) may emit feedback to stderr, so to enforce
         * the "quiet" aspect we have to reopen stderr on /dev/null:
         */
        if ( is_quiet ) freopen("/dev/null", "w", stderr);
        
        while ( ! rc && (optind < argc) ) {
            char            output[128];
            double          seconds;
            
            if ( strcmp(argv[optind], "-") == 0 ) {
                if ( did_see_stdin ) {
                    fprintf(stderr, "ERROR:  cannot use stdin ('-') for multiple <value> arguments\n");
                    rc = EINVAL;
                } else {
                    did_see_stdin = true;
                    while ( ! rc && ! feof(stdin) ) {
                        char    *s = NULL;
                        size_t  slen;
                    
                        if ( getline(&s, &slen, stdin) > 0 ) {
                            size_t  i = 0;
                            
                            while ( (i < slen) && s[i] ) i++;
                            while ( i && isspace(s[i-1]) ) {
                                s[--i] = '\0';
                            }
                            if ( parse->parse_fn(s, &seconds) ) {
                                if ( is_debug ) printf("DEBUG:  parsed to %lf seconds\n", seconds);
                                if ( unparse->unparse_fn(seconds, output, sizeof(output)) ) {
                                    printf("%s\n", output);
                                } else {
                                    rc = EINVAL;
                                    fprintf(stderr, "ERROR:  unable to unparse %.3lf to format %s (%d)\n", seconds, to_format, errno);
                                }
                            }
                            else {
                                rc = EINVAL;
                                fprintf(stderr, "ERROR:  unable to parse %s from format %s (%d)\n", s, from_format, errno);
                            }
                        }
                    }
                }
            }
            else if ( parse->parse_fn(argv[optind], &seconds) ) {
                if ( is_debug ) printf("DEBUG:  parsed to %lf seconds\n", seconds);
                if ( unparse->unparse_fn(seconds, output, sizeof(output)) ) {
                    printf("%s\n", output);
                } else {
                    rc = EINVAL;
                    fprintf(stderr, "ERROR:  unable to unparse %.3lf to format %s (%d)\n", seconds, to_format, errno);
                }
            }
            else {
                rc = EINVAL;
                fprintf(stderr, "ERROR:  unable to parse %s from format %s (%d)\n", argv[optind], from_format, errno);
            }
            optind++;
        }
    }
    return rc;
}

