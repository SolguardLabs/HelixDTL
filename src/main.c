#include "helix.h"

#include <stdio.h>
#include <string.h>

static void usage(FILE *out, const char *argv0) {
    fprintf(out, "Usage:\n");
    fprintf(out, "  %s [scenario]\n", argv0);
    fprintf(out, "  %s --list\n", argv0);
    fprintf(out, "  %s run <script.hlx>\n", argv0);
}

int main(int argc, char **argv) {
    HlxStatus status;

    if (argc == 1) {
        status = hlx_run_scenario("basic", stdout);
        return status == HLX_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout, argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--list") == 0) {
        hlx_print_scenarios(stdout);
        return 0;
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc != 3) {
            usage(stderr, argv[0]);
            return 2;
        }
        status = hlx_run_script_path(argv[2], stdout);
        return status == HLX_OK ? 0 : 1;
    }

    status = hlx_run_scenario(argv[1], stdout);
    return status == HLX_OK ? 0 : 1;
}
