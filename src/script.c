#include "helix.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKENS 12

typedef struct ScriptContext {
    HlxLedger ledger;
    FILE *out;
    const char *scenario_name;
} ScriptContext;

static char *ltrim(char *text) {
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    return text;
}

static void rtrim(char *text) {
    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[len - 1] = '\0';
        len--;
    }
}

static int tokenize(char *line, char **tokens, int max_tokens) {
    int count = 0;
    char *cursor;
    char *hash = strchr(line, '#');
    if (hash != NULL) {
        *hash = '\0';
    }
    cursor = ltrim(line);
    rtrim(cursor);
    while (*cursor != '\0' && count < max_tokens) {
        tokens[count++] = cursor;
        while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        *cursor++ = '\0';
        cursor = ltrim(cursor);
    }
    return count;
}

static bool parse_u64(const char *text, uint64_t *out) {
    char *end = NULL;
    unsigned long long value;
    if (text == NULL || *text == '\0') {
        return false;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == NULL || *end != '\0') {
        return false;
    }
    *out = (uint64_t)value;
    return true;
}

static bool parse_u32(const char *text, uint32_t *out) {
    uint64_t value = 0;
    if (!parse_u64(text, &value) || value > UINT32_MAX) {
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

static HlxStatus usage_error(ScriptContext *ctx, size_t line_no, const char *detail) {
    hlx_write_error_json(ctx->out, HLX_ERR_PARSE, detail, line_no);
    return HLX_ERR_PARSE;
}

static HlxStatus domain_error(ScriptContext *ctx, HlxStatus status, size_t line_no) {
    hlx_write_error_json(ctx->out, status, ctx->ledger.last_error, line_no);
    return status;
}

static bool command_is(const char *actual, const char *a, const char *b) {
    if (strcmp(actual, a) == 0) {
        return true;
    }
    return b != NULL && strcmp(actual, b) == 0;
}

static HlxStatus run_account(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t id = 0;
    uint64_t assets = 0;
    HlxStatus status;
    if (count != 4) {
        return usage_error(ctx, line_no, "account <id> <label> <assets>");
    }
    if (!parse_u32(tokens[1], &id) || !parse_u64(tokens[3], &assets)) {
        return usage_error(ctx, line_no, "invalid account id or assets");
    }
    status = hlx_add_account(&ctx->ledger, id, tokens[2], assets);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_vault(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t id = 0;
    uint64_t index = 0;
    bool ok = false;
    HlxStatus status;
    if (count != 4) {
        return usage_error(ctx, line_no, "vault <id> <symbol> <share_index>");
    }
    if (!parse_u32(tokens[1], &id)) {
        return usage_error(ctx, line_no, "invalid vault id");
    }
    index = hlx_parse_index_literal(tokens[3], &ok);
    if (!ok || index == 0) {
        return usage_error(ctx, line_no, "invalid vault share index");
    }
    status = hlx_add_vault(&ctx->ledger, id, tokens[2], index);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_deposit(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t account_id = 0;
    uint32_t vault_id = 0;
    uint64_t assets = 0;
    uint64_t shares = 0;
    HlxStatus status;
    if (count != 4) {
        return usage_error(ctx, line_no, "deposit <account_id> <vault_id> <assets>");
    }
    if (!parse_u32(tokens[1], &account_id) || !parse_u32(tokens[2], &vault_id) || !parse_u64(tokens[3], &assets)) {
        return usage_error(ctx, line_no, "invalid deposit argument");
    }
    status = hlx_deposit(&ctx->ledger, account_id, vault_id, assets, &shares);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_lock(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t account_id = 0;
    uint32_t vault_id = 0;
    uint32_t lock_id = 0;
    uint64_t shares = 0;
    HlxStatus status;
    if (count != 4) {
        return usage_error(ctx, line_no, "lock <account_id> <vault_id> <shares>");
    }
    if (!parse_u32(tokens[1], &account_id) || !parse_u32(tokens[2], &vault_id) || !parse_u64(tokens[3], &shares)) {
        return usage_error(ctx, line_no, "invalid lock argument");
    }
    status = hlx_lock_shares(&ctx->ledger, account_id, vault_id, shares, &lock_id);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_transfer_lock(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t from_id = 0;
    uint32_t to_id = 0;
    uint32_t lock_id = 0;
    uint32_t child_id = 0;
    uint64_t shares = 0;
    HlxStatus status;
    if (count != 5) {
        return usage_error(ctx, line_no, "transfer-lock <from_account_id> <to_account_id> <lock_id> <shares>");
    }
    if (!parse_u32(tokens[1], &from_id) || !parse_u32(tokens[2], &to_id) || !parse_u32(tokens[3], &lock_id) || !parse_u64(tokens[4], &shares)) {
        return usage_error(ctx, line_no, "invalid transfer-lock argument");
    }
    status = hlx_transfer_locked(&ctx->ledger, from_id, to_id, lock_id, shares, &child_id);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_redeem(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t account_id = 0;
    uint32_t lock_id = 0;
    uint32_t redemption_id = 0;
    uint64_t shares = 0;
    HlxStatus status;
    if (count != 4) {
        return usage_error(ctx, line_no, "redeem <account_id> <lock_id> <shares>");
    }
    if (!parse_u32(tokens[1], &account_id) || !parse_u32(tokens[2], &lock_id) || !parse_u64(tokens[3], &shares)) {
        return usage_error(ctx, line_no, "invalid redeem argument");
    }
    status = hlx_request_redemption(&ctx->ledger, account_id, lock_id, shares, &redemption_id);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_quote_deposit(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t account_id = 0;
    uint32_t vault_id = 0;
    uint32_t quote_id = 0;
    uint64_t assets = 0;
    HlxStatus status;
    if (count != 4) {
        return usage_error(ctx, line_no, "quote-deposit <account_id> <vault_id> <assets>");
    }
    if (!parse_u32(tokens[1], &account_id) || !parse_u32(tokens[2], &vault_id) || !parse_u64(tokens[3], &assets)) {
        return usage_error(ctx, line_no, "invalid quote-deposit argument");
    }
    status = hlx_quote_deposit(&ctx->ledger, account_id, vault_id, assets, &quote_id);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_quote_redeem(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t account_id = 0;
    uint32_t lock_id = 0;
    uint32_t quote_id = 0;
    uint64_t shares = 0;
    HlxStatus status;
    if (count != 4) {
        return usage_error(ctx, line_no, "quote-redeem <account_id> <lock_id> <shares>");
    }
    if (!parse_u32(tokens[1], &account_id) || !parse_u32(tokens[2], &lock_id) || !parse_u64(tokens[3], &shares)) {
        return usage_error(ctx, line_no, "invalid quote-redeem argument");
    }
    status = hlx_quote_redemption(&ctx->ledger, account_id, lock_id, shares, &quote_id);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_revalue(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t vault_id = 0;
    uint64_t index = 0;
    bool ok = false;
    HlxStatus status;
    if (count != 3) {
        return usage_error(ctx, line_no, "revalue <vault_id> <share_index>");
    }
    if (!parse_u32(tokens[1], &vault_id)) {
        return usage_error(ctx, line_no, "invalid vault id");
    }
    index = hlx_parse_index_literal(tokens[2], &ok);
    if (!ok || index == 0) {
        return usage_error(ctx, line_no, "invalid share index");
    }
    status = hlx_revalue_vault(&ctx->ledger, vault_id, index);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_advance(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t vault_id = 0;
    HlxStatus status;
    if (count != 2) {
        return usage_error(ctx, line_no, "advance <vault_id>");
    }
    if (!parse_u32(tokens[1], &vault_id)) {
        return usage_error(ctx, line_no, "invalid vault id");
    }
    status = hlx_advance_epoch(&ctx->ledger, vault_id);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_settle(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t vault_id = 0;
    HlxStatus status;
    if (count != 2) {
        return usage_error(ctx, line_no, "settle <vault_id>");
    }
    if (!parse_u32(tokens[1], &vault_id)) {
        return usage_error(ctx, line_no, "invalid vault id");
    }
    status = hlx_settle_epoch(&ctx->ledger, vault_id);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_withdraw(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    uint32_t account_id = 0;
    uint32_t redemption_id = 0;
    HlxStatus status;
    if (count != 3) {
        return usage_error(ctx, line_no, "withdraw <account_id> <redemption_id>");
    }
    if (!parse_u32(tokens[1], &account_id) || !parse_u32(tokens[2], &redemption_id)) {
        return usage_error(ctx, line_no, "invalid withdraw argument");
    }
    status = hlx_withdraw_redemption(&ctx->ledger, account_id, redemption_id);
    if (status != HLX_OK) {
        return domain_error(ctx, status, line_no);
    }
    return HLX_OK;
}

static HlxStatus run_network(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    if (count != 2) {
        return usage_error(ctx, line_no, "network <network_id>");
    }
    if (hlx_count_active_accounts(&ctx->ledger) != 0 || hlx_count_active_vaults(&ctx->ledger) != 0) {
        return usage_error(ctx, line_no, "network must be declared before accounts or vaults");
    }
    hlx_ledger_init(&ctx->ledger, tokens[1]);
    return HLX_OK;
}

static HlxStatus run_line(ScriptContext *ctx, char **tokens, int count, size_t line_no) {
    if (count == 0) {
        return HLX_OK;
    }
    if (strcmp(tokens[0], "network") == 0) {
        return run_network(ctx, tokens, count, line_no);
    }
    if (strcmp(tokens[0], "account") == 0) {
        return run_account(ctx, tokens, count, line_no);
    }
    if (strcmp(tokens[0], "vault") == 0) {
        return run_vault(ctx, tokens, count, line_no);
    }
    if (strcmp(tokens[0], "deposit") == 0) {
        return run_deposit(ctx, tokens, count, line_no);
    }
    if (strcmp(tokens[0], "lock") == 0) {
        return run_lock(ctx, tokens, count, line_no);
    }
    if (command_is(tokens[0], "transfer-lock", "transfer_lock")) {
        return run_transfer_lock(ctx, tokens, count, line_no);
    }
    if (strcmp(tokens[0], "redeem") == 0) {
        return run_redeem(ctx, tokens, count, line_no);
    }
    if (command_is(tokens[0], "quote-deposit", "quote_deposit")) {
        return run_quote_deposit(ctx, tokens, count, line_no);
    }
    if (command_is(tokens[0], "quote-redeem", "quote_redeem")) {
        return run_quote_redeem(ctx, tokens, count, line_no);
    }
    if (strcmp(tokens[0], "revalue") == 0) {
        return run_revalue(ctx, tokens, count, line_no);
    }
    if (strcmp(tokens[0], "advance") == 0) {
        return run_advance(ctx, tokens, count, line_no);
    }
    if (strcmp(tokens[0], "settle") == 0) {
        return run_settle(ctx, tokens, count, line_no);
    }
    if (strcmp(tokens[0], "withdraw") == 0) {
        return run_withdraw(ctx, tokens, count, line_no);
    }
    if (strcmp(tokens[0], "snapshot") == 0) {
        return count == 1 ? HLX_OK : usage_error(ctx, line_no, "snapshot takes no arguments");
    }
    return usage_error(ctx, line_no, "unknown command");
}

static const char *scenario_name_from_path(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *base = path;
    if (slash != NULL && slash + 1 > base) {
        base = slash + 1;
    }
    if (backslash != NULL && backslash + 1 > base) {
        base = backslash + 1;
    }
    return base;
}

HlxStatus hlx_run_script_path(const char *path, FILE *out) {
    FILE *file;
    ScriptContext ctx;
    char line[512];
    size_t line_no = 0;

    if (path == NULL || *path == '\0') {
        hlx_write_error_json(out, HLX_ERR_IO, "script path is required", 0);
        return HLX_ERR_IO;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        hlx_write_error_json(out, HLX_ERR_IO, path, 0);
        return HLX_ERR_IO;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.out = out;
    ctx.scenario_name = scenario_name_from_path(path);
    hlx_ledger_init(&ctx.ledger, "helix-script");

    while (fgets(line, sizeof(line), file) != NULL) {
        char *tokens[MAX_TOKENS];
        int count;
        HlxStatus status;
        line_no++;
        count = tokenize(line, tokens, MAX_TOKENS);
        status = run_line(&ctx, tokens, count, line_no);
        if (status != HLX_OK) {
            fclose(file);
            return status;
        }
    }

    if (ferror(file)) {
        fclose(file);
        hlx_write_error_json(out, HLX_ERR_IO, "failed while reading script", line_no);
        return HLX_ERR_IO;
    }
    fclose(file);

    hlx_write_snapshot_json(&ctx.ledger, ctx.scenario_name, out);
    return HLX_OK;
}
