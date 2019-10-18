
/**************************************************************************
 *
 * Copyright 2011 Jose Fonseca
 * Copyright 2010 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/


#include <string.h>
#include <limits.h> // for CHAR_MAX
#include <getopt.h>
#ifndef _WIN32
#include <unistd.h> // for isatty()
#endif

#include <memory>
#include <fstream>
#include <string>
#include <regex>

#include "cxx_compat.hpp" // for std::to_string, std::make_unique

#include "cli.hpp"
#include "cli_pager.hpp"

#include "trace_parser.hpp"
#include "trace_dump_internal.hpp"
#include "trace_callset.hpp"
#include "trace_option.hpp"

static trace::CallSet calls(trace::FREQUENCY_ALL);

static const char *synopsis = "Print a trace file information.";

static void
usage(void)
{
    std::cout
        << "usage: apitrace info [OPTIONS] TRACE_FILE...\n"
        << synopsis << "\n"
        "\n"
        "    -h, --help           show this help message and exit\n"
        "    --json               output in json format\n"
        "\n"
    ;
}

enum {
    CALLS_OPT = CHAR_MAX + 1,
    JSON_OPT,
};

const static char *
shortOptions = "hv";

const static struct option
longOptions[] = {
    {"help", no_argument, 0, 'h'},
    {"json", no_argument, 0, JSON_OPT},
    {0, 0, 0, 0}
};

static int
parseArrayAttrib(const trace::Array *array_attrib, int attrib, int def_val, int terminator) {
    for (size_t idx = 0; idx < array_attrib->values.size()/2; ++idx) {
        int key = array_attrib->values[idx*2]->toSInt();
        if (terminator == key) break;
        if (key == attrib) return array_attrib->values[idx*2+1]->toSInt();
    }

    return def_val;
}

static int
command(int argc, char *argv[])
{
    std::regex grepRegex;
    bool json = false;

    int opt;
    while ((opt = getopt_long(argc, argv, shortOptions, longOptions, NULL)) != -1) {
        switch (opt) {
        case 'h':
            usage();
            return 0;
        case JSON_OPT:
            json = true;
            break;
        default:
            std::cerr << "error: unexpected option `" << (char)opt << "`\n";
            usage();
            return 1;
        }
    }

    for (int i = optind; i < argc; ++i) {
        unsigned long frames_count = 0;
        unsigned int context_version_major = 0, context_version_minor = 0;
        trace::Parser p;

       if (!p.open(argv[i])) {
            return 1;
        }

        trace::Call *call;
        while ((call = p.parse_call())) {
            if (call->no > calls.getLast()) {
                delete call;
                break;
            }

            if (context_version_major == 0 && strcmp(call->sig->name, "glXCreateContextAttribsARB") == 0) {
                if (call->sig->num_args == 5 && strcmp(call->sig->arg_names[4], "attrib_list") == 0 ) {
                    const trace::Array *attribs = call->arg(4).toArray();
                    if (attribs) {
                         context_version_major = parseArrayAttrib(attribs, 0x2091, 0xFFFF, 0);
                         context_version_minor = parseArrayAttrib(attribs, 0x2092, 0xFFFF, 0);
                    } else context_version_major = context_version_minor = 0xFFFF;
                } else context_version_major = context_version_minor = 0xFFFF;
            }

            if (call->flags & trace::CALL_FLAG_END_FRAME)
                ++frames_count;

            delete call;
        }

        if (json) {
            printf("{ \"FileVersion\": \"%llu\", \"FramesCount\": \"%lu\", \"ContextVersion\": \"%u.%u\" }\n",
                   p.getVersion(), frames_count, context_version_major, context_version_minor);
        } else {
            printf("File version: %llu\nFrames count: %lu\nContext version: %u.%u\n",
                   p.getVersion(), frames_count, context_version_major, context_version_minor);
        }
    }

   return 0;
}

const Command info_command = {
    "info",
    synopsis,
    usage,
    command
};
