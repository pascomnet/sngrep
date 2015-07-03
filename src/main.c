/**************************************************************************
 **
 ** sngrep - SIP Messages flow viewer
 **
 ** Copyright (C) 2013,2014 Ivan Alonso (Kaian)
 ** Copyright (C) 2013,2014 Irontec SL. All rights reserved.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 ****************************************************************************/
/**
 * @file main.c
 * @author Ivan Alonso [aka Kaian] <kaian@irontec.com>
 *
 * @brief Source of initial functions used by sngrep
 */

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <getopt.h>
#include "option.h"
#include "ui_manager.h"
#include "capture.h"
#ifdef WITH_OPENSSL
#include "capture_tls.h"
#endif

/**
 * @brief Usage function
 *
 * Print all available options (which are none actually)
 */
void
usage()
{
    printf("Usage: %s [-hVcivNq] [-IO pcap_dump] [-d dev] [-l limit]"
#ifdef WITH_OPENSSL
           " [-k keyfile]"
#endif
           " [<match expression>] [<bpf filter>]\n\n"
           "    -h --help\t\t This usage\n"
           "    -V --version\t Version information\n"
           "    -d --device\t\t Use this capture device instead of default\n"
           "    -I --input\t\t Read captured data from pcap file\n"
           "    -O --output\t\t Write captured data to pcap file\n"
           "    -c --calls\t\t Only display dialogs starting with INVITE\n"
           "    -r --rtp\t\t Capture full RTP packets\n"
           "    -l --limit\t\t Set capture limit to N dialogs\n"
           "    -i --icase\t\t Make <match expression> case insensitive\n"
           "    -v --invert\t\t Invert <match expression>\n"
           "    -N --no-interface\t Don't display sngrep interface, just capture\n"
           "    -q --quiet\t\t Don't print captured dialogs in no interface mode\n"
#ifdef WITH_OPENSSL
           "    -k --keyfile\t RSA private keyfile to decrypt captured packets\n"
#endif
           "\n",PACKAGE);
}

void
version()
{
    printf("%s - %s\n"
           "Copyright (C) 2013-2015 Irontec S.L.\n"
           "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
           "This is free software: you are free to change and redistribute it.\n"
           "There is NO WARRANTY, to the extent permitted by law.\n"

#ifdef WITH_OPENSSL
           " * Compiled with OpenSSL support.\n"
#endif
#ifdef WITH_UNICODE
           " * Compiled with Wide-character support.\n"
#endif
#ifdef WITH_PCRE
           " * Compiled with Perl Compatible regular expressions support.\n"
#endif
#ifdef WITH_IPV6
           " * Compiled with IPv6 support.\n"
#endif
           "\nWritten by Ivan Alonso [aka Kaian]\n",
           PACKAGE, VERSION);
}

/**
 * @brief Main function logic
 *
 * Parse command line options and start running threads
 */
int
main(int argc, char* argv[])
{
    int opt, idx, limit, only_calls, no_incomplete, i;
    const char *device, *infile, *outfile;
    char bpf[512];
    const char *keyfile;
    const char *match_expr;
    int match_insensitive = 0, match_invert = 0;
    int no_interface = 0, quiet = 0, rtp_capture = 0;

    // Program otptions
    static struct option long_options[] = {
        { "help", no_argument, 0, 'h' },
        { "version", no_argument, 0, 'V' },
        { "device", required_argument, 0, 'd' },
        { "input", required_argument, 0, 'I' },
        { "output", required_argument, 0, 'O' },
        { "keyfile", required_argument, 0, 'k' },
        { "calls", no_argument, 0, 'c' },
        { "rtp", no_argument, 0, 'r' },
        { "limit", no_argument, 0, 'l' },
        { "icase", no_argument, 0, 'i' },
        { "invert", no_argument, 0, 'v' },
        { "no-interface", no_argument, 0, 'N' },
        { "quiet", no_argument, 0, 'q' },
    };

    // Initialize configuration options
    init_options();

    // Get initial values for configurable arguments
    device = setting_get_value(SETTING_CAPTURE_DEVICE);
    infile = setting_get_value(SETTING_CAPTURE_INFILE);
    outfile = setting_get_value(SETTING_CAPTURE_OUTFILE);
    keyfile = setting_get_value(SETTING_CAPTURE_KEYFILE);
    limit = setting_get_intvalue(SETTING_CAPTURE_LIMIT);
    only_calls = setting_enabled(SETTING_SIP_CALLS);
    no_incomplete = setting_enabled(SETTING_SIP_NOINCOMPLETE);
    rtp_capture = setting_enabled(SETTING_SIP_NOINCOMPLETE);

    // Parse command line arguments
    opterr = 0;
    char *options = "hVd:I:O:pqtW:k:crl:ivNq";
    while ((opt = getopt_long(argc, argv, options, long_options, &idx)) != -1) {
        switch (opt) {
            case 'h':
                usage();
                return 0;
            case 'V':
                version();
                return 0;
            case 'd':
                device = optarg;
                break;
            case 'I':
                infile = optarg;
                break;
            case 'O':
                outfile = optarg;
                break;
            case 'l':
                if(!(limit = atoi(optarg))) {
                    fprintf(stderr, "Invalid limit value.\n");
                    return 0;
                }
                break;
            case 'k':
                keyfile = optarg;
                break;
            case 'c':
                only_calls = 1;
                break;
            case 'r':
                rtp_capture = 1;
                break;
            case 'i':
                match_insensitive++;
                break;
            case 'v':
                match_invert++;
                break;
            case 'N':
                no_interface = 1;
                break;
            case 'q':
                quiet = 1;
                break;
            // Dark options for dummy ones
            case 'p':
            case 't':
            case 'W':
                break;
            case '?':
                if (strchr(options, optopt)) {
                    fprintf(stderr, "-%c option requires an argument.\n", optopt);
                } else if (isprint(optopt)) {
                    fprintf(stderr, "Unknown option -%c.\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
                }
                return 1;
            default:
                break;
        }
    }

#ifdef WITH_OPENSSL
    // Set capture decrypt key file
    capture_set_keyfile(keyfile);
    // Check if we have a keyfile and is valid
    if (keyfile && !tls_check_keyfile(keyfile)) {
        fprintf(stderr, "%s does not contain a valid RSA private key.\n", keyfile);
        return 1;
    }
#endif

    // Check if given argument is a file
    if (argc == 2 && (access(argv[1], F_OK) == 0)) {
        // Old legacy option to open pcaps without other arguments
        printf("%s seems to be a file: You forgot -I flag?\n", argv[1]);
        return 0;
    }

    // Initialize SIP Messages Storage
    sip_init(limit, only_calls, no_incomplete);

    // Set capture options
    capture_set_opts(limit, rtp_capture);

    // If we have an input file, load it
    if (infile) {
        // Try to load file
        if (capture_offline(infile, outfile) != 0)
            return 1;
    } else {
        // Check if all capture data is valid
        if (capture_online(device, outfile) != 0)
            return 1;
    }

    // More arguments pending!
    if (argv[optind]) {
        // Assume first pending argument is  match expression
        match_expr = argv[optind++];

        // Try to build the bpf filter string with the rest
        memset(bpf, 0, sizeof(bpf));
        for (i = optind; i < argc; i++)
            sprintf(bpf + strlen(bpf), "%s ", argv[i]);

        // Check if this BPF filter is valid
        if (capture_set_bpf_filter(bpf) != 0) {
            // BPF Filter invalid, check incluiding match_expr
            match_expr = 0;    // There is no need to parse all payload at this point


            // Build the bpf filter string
            memset(bpf, 0, sizeof(bpf));
            for (i = optind - 1; i < argc; i++)
                sprintf(bpf + strlen(bpf), "%s ", argv[i]);

            // Check bpf filter is valid again
            if (capture_set_bpf_filter(bpf) != 0) {
                fprintf(stderr, "Couldn't install filter %s: %s\n", bpf, capture_last_error());
                return 1;
            }
        }

        // Set the capture filter
        if (match_expr)
            if (sip_set_match_expression(match_expr, match_insensitive, match_invert)) {
                fprintf(stderr, "Unable to parse expression %s\n", match_expr);
                return 1;
            }
    }

    // Start a capture thread
    if (capture_launch_thread() != 0) {
        ncurses_deinit();
        fprintf(stderr, "Failed to launch capture thread.\n");
        return 1;
    }

    if (!no_interface) {
        // Initialize interface
        ncurses_init();
        // This is a blocking call.
        // Create the first panel and wait for user input
        ui_create_panel(PANEL_CALL_LIST);
        wait_for_input();
    } else {
        setbuf(stdout, NULL);
        while(capture_get_status() != CAPTURE_OFFLINE) {
            if (!quiet)
                printf("\rDialog count: %d", sip_calls_count());
            usleep(500 * 1000);
        }
        if (!quiet)
            printf("\rDialog count: %d\n", sip_calls_count());
    }

    // Close pcap handler
    capture_close();

    // Deinitialize interface
    ncurses_deinit();

    // Deinitialize configuration options
    deinit_options();

    // Deallocate sip stored messages
    sip_deinit();

    // Leaving!
    return 0;
}
