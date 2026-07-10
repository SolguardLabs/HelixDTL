#include "helix.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint64_t hlx_assets_to_shares(uint64_t assets, uint64_t index) {
    if (index == 0) {
        return 0;
    }
    return (assets * HLX_INDEX_SCALE) / index;
}

uint64_t hlx_shares_to_assets(uint64_t shares, uint64_t index) {
    return (shares * index) / HLX_INDEX_SCALE;
}

static bool only_digits(const char *text) {
    if (text == NULL || *text == '\0') {
        return false;
    }
    for (const char *p = text; *p != '\0'; ++p) {
        if (!isdigit((unsigned char)*p)) {
            return false;
        }
    }
    return true;
}

uint64_t hlx_parse_index_literal(const char *text, bool *ok) {
    char whole[32];
    char frac[16];
    const char *dot;
    size_t whole_len;
    size_t frac_len;
    uint64_t whole_value;
    uint64_t frac_value = 0;

    if (ok != NULL) {
        *ok = false;
    }
    if (text == NULL || *text == '\0') {
        return 0;
    }

    dot = strchr(text, '.');
    if (dot == NULL) {
        if (!only_digits(text)) {
            return 0;
        }
        whole_value = strtoull(text, NULL, 10);
        if (ok != NULL) {
            *ok = true;
        }
        if (whole_value > UINT64_MAX / HLX_INDEX_SCALE) {
            return 0;
        }
        return whole_value * HLX_INDEX_SCALE;
    }

    whole_len = (size_t)(dot - text);
    frac_len = strlen(dot + 1);
    if (whole_len == 0 || whole_len >= sizeof(whole) || frac_len == 0 || frac_len > 6 || frac_len >= sizeof(frac)) {
        return 0;
    }

    memcpy(whole, text, whole_len);
    whole[whole_len] = '\0';
    memcpy(frac, dot + 1, frac_len);
    frac[frac_len] = '\0';

    if (!only_digits(whole) || !only_digits(frac)) {
        return 0;
    }

    whole_value = strtoull(whole, NULL, 10);
    frac_value = strtoull(frac, NULL, 10);
    while (frac_len < 6) {
        frac_value *= 10;
        frac_len++;
    }

    if (ok != NULL) {
        *ok = true;
    }
    if (whole_value > UINT64_MAX / HLX_INDEX_SCALE) {
        return 0;
    }
    return whole_value * HLX_INDEX_SCALE + frac_value;
}

void hlx_format_index(uint64_t index, char *out, size_t out_size) {
    uint64_t whole = index / HLX_INDEX_SCALE;
    uint64_t frac = index % HLX_INDEX_SCALE;
    if (out == NULL || out_size == 0) {
        return;
    }
    snprintf(out, out_size, "%llu.%06llu", (unsigned long long)whole, (unsigned long long)frac);
}

const char *hlx_status_message(HlxStatus status) {
    switch (status) {
        case HLX_OK:
            return "ok";
        case HLX_ERR_NOT_FOUND:
            return "not_found";
        case HLX_ERR_EXISTS:
            return "exists";
        case HLX_ERR_CAPACITY:
            return "capacity";
        case HLX_ERR_BALANCE:
            return "balance";
        case HLX_ERR_SHARES:
            return "shares";
        case HLX_ERR_INDEX:
            return "index";
        case HLX_ERR_STATE:
            return "state";
        case HLX_ERR_PARSE:
            return "parse";
        case HLX_ERR_IO:
            return "io";
        default:
            return "unknown";
    }
}
