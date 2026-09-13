/* adt_match.c - exhaustiveness and redundancy (Maranget usefulness). */
#include <adt.h>

#include "adt_internal.h"

#include <string.h>

/* A wildcard column is a NULL pattern pointer. Rows share one width. */
struct adt_i_row
{
    struct adt_pattern **cols;
    size_t width;
};

/* Transient grow-only allocation chain used for one query; freed before the
 * query returns. Allocations go through the context allocator. */
struct adt_i_temp
{
    struct adt_ctx *ctx;
    void *head;
};

struct adt_i_blk
{
    struct adt_i_blk *next;
};

static int adt_i_wc(const struct adt_pattern *p)
{
    if (p == NULL)
    {
        return 1;
    }
    if (p->kind == ADT_PATTERN_WILDCARD)
    {
        return 1;
    }
    return 0;
}

static void *adt_i_temp_alloc(struct adt_i_temp *temp, size_t size)
{
    void *raw;
    struct adt_i_blk *blk;
    raw = temp->ctx->alloc.alloc(temp->ctx->alloc.userdata,
                                 sizeof(struct adt_i_blk) + size);
    if (raw == NULL)
    {
        return NULL;
    }
    blk = raw;
    blk->next = temp->head;
    temp->head = blk;
    return blk + 1;
}

static struct adt_i_blk *adt_i_temp_free_one(struct adt_i_temp *temp,
                                             struct adt_i_blk *blk)
{
    struct adt_i_blk *next;
    next = blk->next;
    temp->ctx->alloc.free(temp->ctx->alloc.userdata, blk);
    return next;
}

static void adt_i_temp_free_all(struct adt_i_temp *temp)
{
    struct adt_i_blk *blk;
    blk = temp->head;
    while (blk != NULL)
    {
        blk = adt_i_temp_free_one(temp, blk);
    }
    temp->head = NULL;
}

static int adt_i_row_all_wild_cell(const struct adt_i_row *row, size_t i)
{
    int w;
    w = adt_i_wc(row->cols[i]);
    if (w == 0)
    {
        return 0;
    }
    return 1;
}

static int adt_i_row_all_wild(const struct adt_i_row *row)
{
    size_t i;
    int cell;
    for (i = 0; i < row->width; ++i)
    {
        cell = adt_i_row_all_wild_cell(row, i);
        if (cell == 0)
        {
            return 0;
        }
    }
    return 1;
}

static void adt_i_row_init(struct adt_i_row *row, struct adt_pattern **cols)
{
    row->cols = cols;
    row->width = 1;
}

static int adt_i_name_matches(const struct adt_type *type, const char *name)
{
    int cmp;
    cmp = strcmp(type->hm_name, name);
    if (cmp == 0)
    {
        return 1;
    }
    return 0;
}

static const struct adt_type *adt_i_family_for_name(struct adt_ctx *ctx,
                                                    const char *name)
{
    size_t i;
    int matched;
    for (i = 0; i < ctx->type_count; ++i)
    {
        matched = adt_i_name_matches(ctx->types[i], name);
        if (matched != 0)
        {
            return ctx->types[i];
        }
    }
    return NULL;
}

static const struct adt_type *adt_i_field_family(struct adt_ctx *ctx,
                                                 const struct adt_ctor *ctor,
                                                 size_t index)
{
    struct hm_type *field;
    struct hm_type *p;
    hm_type_kind kind;
    const char *name;
    const struct adt_type *fam;
    field = ctor->field_types[index];
    p = hm_type_prune(field);
    kind = hm_type_kind_of(p);
    if (kind != HM_TYPE_CON)
    {
        return NULL;
    }
    name = hm_type_con_name(p);
    if (name == NULL)
    {
        return NULL;
    }
    fam = adt_i_family_for_name(ctx, name);
    return fam;
}

/* Write the specialized column at index k of a row built from src under
 * constructor ctor. head_wild means the head column is a wildcard. */
static struct adt_pattern *adt_i_head_val(int head_wild,
                                          const struct adt_i_row *src, size_t k)
{
    if (head_wild != 0)
    {
        return NULL;
    }
    return src->cols[0]->u.ctor.args[k];
}

static struct adt_pattern *adt_i_tail_val(const struct adt_i_row *src,
                                          size_t head_len, size_t k)
{
    struct adt_pattern *val;
    size_t t;
    t = k - head_len;
    val = src->cols[t + 1];
    return val;
}

static void adt_i_fill_cell(struct adt_pattern **cols, size_t k,
                            const struct adt_i_row *src,
                            const struct adt_ctor *ctor, int head_wild)
{
    struct adt_pattern *val;
    size_t head_len;
    head_len = ctor->field_count;
    if (k < head_len)
    {
        val = adt_i_head_val(head_wild, src, k);
    }
    else
    {
        val = adt_i_tail_val(src, head_len, k);
    }
    cols[k] = val;
}

static void adt_i_fill_row(struct adt_pattern **cols, size_t width,
                           const struct adt_i_row *src,
                           const struct adt_ctor *ctor, int head_wild)
{
    size_t k;
    for (k = 0; k < width; ++k)
    {
        adt_i_fill_cell(cols, k, src, ctor, head_wild);
    }
}

/* Build a specialized row for constructor ctor from src, or return NULL when
 * src's head is a different constructor. Wildcard heads always specialize. */
static struct adt_i_row *adt_i_spec_row(struct adt_i_temp *temp,
                                        const struct adt_ctor *ctor,
                                        const struct adt_i_row *src)
{
    struct adt_i_row *out;
    struct adt_pattern **cols;
    const struct adt_pattern *head;
    size_t argw;
    size_t width;
    size_t bytes;
    int head_wild;
    out = adt_i_temp_alloc(temp, sizeof(struct adt_i_row));
    if (out == NULL)
    {
        return NULL;
    }
    head = src->cols[0];
    head_wild = adt_i_wc(head);
    if (head_wild == 0)
    {
        if (ctor != head->u.ctor.ctor)
        {
            return NULL;
        }
    }
    argw = ctor->field_count;
    width = argw + src->width - 1;
    bytes = width * sizeof(struct adt_pattern *);
    cols = adt_i_temp_alloc(temp, bytes);
    if (cols == NULL)
    {
        return NULL;
    }
    adt_i_fill_row(cols, width, src, ctor, head_wild);
    out->cols = cols;
    out->width = width;
    return out;
}

static size_t adt_i_row_take(struct adt_i_row *dst, size_t index,
                             const struct adt_i_row *r)
{
    dst[index] = *r;
    return index + 1;
}

static void adt_i_spec_collect(struct adt_i_temp *temp,
                               const struct adt_ctor *ctor,
                               struct adt_i_row *dst, size_t *n,
                               const struct adt_i_row *src)
{
    struct adt_i_row *r;
    size_t taken;
    r = adt_i_spec_row(temp, ctor, src);
    if (r == NULL)
    {
        return;
    }
    taken = adt_i_row_take(dst, *n, r);
    *n = taken;
}

/* Produce the matrix of rows that match constructor ctor. Returns count, or
 * -1 on allocation failure. */
static int adt_i_specialize(struct adt_ctx *ctx, struct adt_i_temp *temp,
                            struct adt_i_row *rows, size_t row_count,
                            const struct adt_ctor *ctor, struct adt_i_row **out)
{
    struct adt_i_row *dst;
    size_t bytes;
    size_t n;
    size_t i;
    (void)ctx;
    bytes = row_count * sizeof(struct adt_i_row);
    dst = adt_i_temp_alloc(temp, bytes);
    if (dst == NULL)
    {
        return -1;
    }
    n = 0;
    for (i = 0; i < row_count; ++i)
    {
        adt_i_spec_collect(temp, ctor, dst, &n, &rows[i]);
    }
    *out = dst;
    return (int)n;
}

static void adt_i_fams_tail_cell(const struct adt_type **fams, size_t argw,
                                 const struct adt_type **tail, size_t i)
{
    fams[argw + i] = tail[i + 1];
}

static void adt_i_fams_tail(const struct adt_type **fams, size_t argw,
                            const struct adt_type **tail, size_t width)
{
    size_t i;
    for (i = 0; i + 1 < width; ++i)
    {
        adt_i_fams_tail_cell(fams, argw, tail, i);
    }
}

static void adt_i_fams_head(struct adt_ctx *ctx, const struct adt_type **fams,
                            const struct adt_ctor *ctor, size_t argw)
{
    size_t i;
    for (i = 0; i < argw; ++i)
    {
        fams[i] = adt_i_field_family(ctx, ctor, i);
    }
}

static int adt_i_concat_fams(struct adt_ctx *ctx, struct adt_i_temp *temp,
                             const struct adt_ctor *ctor, size_t width,
                             const struct adt_type **tail,
                             const struct adt_type ***out)
{
    const struct adt_type **fams;
    size_t argw;
    size_t bytes;
    argw = ctor->field_count;
    bytes = (argw + width - 1) * sizeof(const struct adt_type *);
    fams = adt_i_temp_alloc(temp, bytes);
    if (fams == NULL)
    {
        return 0;
    }
    adt_i_fams_head(ctx, fams, ctor, argw);
    adt_i_fams_tail(fams, argw, tail, width);
    *out = fams;
    return 1;
}

static int adt_i_useful(struct adt_ctx *ctx, struct adt_i_temp *temp,
                        struct adt_i_row *rows, size_t row_count,
                        struct adt_i_row *q, size_t width,
                        const struct adt_type **fams);

static void adt_i_row_drop_col(struct adt_i_row *row)
{
    row->cols = row->cols + 1;
    row->width = row->width - 1;
}

static void adt_i_rows_drop_col(struct adt_i_row *rows, size_t row_count)
{
    size_t i;
    for (i = 0; i < row_count; ++i)
    {
        adt_i_row_drop_col(&rows[i]);
    }
}

static int adt_i_useful_abstract(struct adt_ctx *ctx, struct adt_i_temp *temp,
                                 struct adt_i_row *rows, size_t row_count,
                                 struct adt_i_row *q, size_t width,
                                 const struct adt_type **fams)
{
    struct adt_i_row q2;
    int result;
    q2.cols = q->cols + 1;
    q2.width = width - 1;
    adt_i_rows_drop_col(rows, row_count);
    result = adt_i_useful(ctx, temp, rows, row_count, &q2, width - 1, fams + 1);
    return result;
}

static int adt_i_make_qcols(struct adt_i_temp *temp,
                            const struct adt_ctor *ctor,
                            const struct adt_i_row *q, size_t width,
                            int head_wild, struct adt_i_row *q2)
{
    struct adt_pattern **qcols;
    size_t argw;
    size_t qwidth;
    size_t bytes;
    argw = ctor->field_count;
    qwidth = argw + width - 1;
    bytes = qwidth * sizeof(struct adt_pattern *);
    qcols = adt_i_temp_alloc(temp, bytes);
    if (qcols == NULL)
    {
        return 0;
    }
    adt_i_fill_row(qcols, qwidth, q, ctor, head_wild);
    q2->cols = qcols;
    q2->width = qwidth;
    return 1;
}

static int adt_i_useful_ctor(struct adt_ctx *ctx, struct adt_i_temp *temp,
                             struct adt_i_row *rows, size_t row_count,
                             struct adt_i_row *q, size_t width,
                             const struct adt_type **fams,
                             const struct adt_ctor *ctor, int head_wild)
{
    struct adt_i_row *spec;
    struct adt_i_row q2;
    const struct adt_type **nfams;
    int spec_count;
    int made;
    int ok;
    int result;
    spec_count = adt_i_specialize(ctx, temp, rows, row_count, ctor, &spec);
    if (spec_count < 0)
    {
        return -1;
    }
    made = adt_i_make_qcols(temp, ctor, q, width, head_wild, &q2);
    if (made == 0)
    {
        return -1;
    }
    ok = adt_i_concat_fams(ctx, temp, ctor, width, fams, &nfams);
    if (ok == 0)
    {
        return -1;
    }
    result =
        adt_i_useful(ctx, temp, spec, (size_t)spec_count, &q2, q2.width, nfams);
    return result;
}

static int adt_i_useful_dispatch(struct adt_ctx *ctx, struct adt_i_temp *temp,
                                 struct adt_i_row *rows, size_t row_count,
                                 struct adt_i_row *q, size_t width,
                                 const struct adt_type **fams)
{
    int result;
    result = adt_i_useful_ctor(ctx, temp, rows, row_count, q, width, fams,
                               q->cols[0]->u.ctor.ctor, 0);
    return result;
}

static int adt_i_useful_wild_ctor(struct adt_ctx *ctx, struct adt_i_temp *temp,
                                  struct adt_i_row *rows, size_t row_count,
                                  struct adt_i_row *q, size_t width,
                                  const struct adt_type **fams,
                                  const struct adt_ctor *ctor)
{
    int result;
    result =
        adt_i_useful_ctor(ctx, temp, rows, row_count, q, width, fams, ctor, 1);
    return result;
}

static int adt_i_ctor_covered(struct adt_i_row *rows, size_t row_count)
{
    size_t i;
    int all_wild;
    for (i = 0; i < row_count; ++i)
    {
        all_wild = adt_i_row_all_wild(&rows[i]);
        if (all_wild != 0)
        {
            return 1;
        }
    }
    return 0;
}

static int adt_i_useful_enumerate(struct adt_ctx *ctx, struct adt_i_temp *temp,
                                  struct adt_i_row *rows, size_t row_count,
                                  struct adt_i_row *q, size_t width,
                                  const struct adt_type **fams,
                                  const struct adt_type *fam)
{
    size_t i;
    int r;
    for (i = 0; i < fam->ctor_count; ++i)
    {
        r = adt_i_useful_wild_ctor(ctx, temp, rows, row_count, q, width, fams,
                                   fam->ctors[i]);
        if (r == 1)
        {
            return 1;
        }
        if (r < 0)
        {
            return r;
        }
    }
    return 0;
}

static int adt_i_useful(struct adt_ctx *ctx, struct adt_i_temp *temp,
                        struct adt_i_row *rows, size_t row_count,
                        struct adt_i_row *q, size_t width,
                        const struct adt_type **fams)
{
    const struct adt_type *fam;
    int covered;
    int head_wild;
    int result;
    covered = adt_i_ctor_covered(rows, row_count);
    if (covered != 0)
    {
        return 0;
    }
    if (width == 0)
    {
        return 1;
    }
    head_wild = adt_i_wc(q->cols[0]);
    if (head_wild == 0)
    {
        result =
            adt_i_useful_dispatch(ctx, temp, rows, row_count, q, width, fams);
        return result;
    }
    fam = fams[0];
    if (fam == NULL)
    {
        result =
            adt_i_useful_abstract(ctx, temp, rows, row_count, q, width, fams);
        return result;
    }
    result =
        adt_i_useful_enumerate(ctx, temp, rows, row_count, q, width, fams, fam);
    return result;
}

static int adt_i_row_of_type(const struct adt_pattern *p,
                             const struct adt_type *type)
{
    int w;
    w = adt_i_wc(p);
    if (w != 0)
    {
        return 0;
    }
    if (p->u.ctor.ctor->owner != type)
    {
        return 0;
    }
    return 1;
}

adt_status adt_match_exhaustive(adt_type *type, size_t pattern_count,
                                adt_pattern **patterns,
                                adt_match_result *result)
{
    struct adt_i_temp temp;
    struct adt_i_row *rows;
    struct adt_i_row q;
    struct adt_pattern **qcols;
    const struct adt_type **fams;
    size_t i;
    int useful;
    int own;
    int wild;
    if (type == NULL)
    {
        return ADT_ERROR_INVALID;
    }
    if (result == NULL)
    {
        return ADT_ERROR_INVALID;
    }
    if (type->sealed == 0)
    {
        adt_i_fail(type->ctx, ADT_ERROR_UNSEALED, ADT_ERR_UNSEALED,
                   "cannot analyze an unsealed type", type, NULL, NULL);
        return ADT_ERROR_UNSEALED;
    }
    for (i = 0; i < pattern_count; ++i)
    {
        if (patterns[i] == NULL)
        {
            adt_i_fail(type->ctx, ADT_ERROR_INVALID, ADT_ERR_INVALID,
                       "pattern does not belong to the analyzed type", type,
                       NULL, NULL);
            return ADT_ERROR_INVALID;
        }
        own = adt_i_row_of_type(patterns[i], type);
        if (own == 0)
        {
            wild = adt_i_wc(patterns[i]);
            if (wild == 0)
            {
                adt_i_fail(type->ctx, ADT_ERROR_INVALID, ADT_ERR_INVALID,
                           "pattern does not belong to the analyzed type", type,
                           NULL, NULL);
                return ADT_ERROR_INVALID;
            }
        }
    }
    temp.ctx = type->ctx;
    temp.head = NULL;
    rows = adt_i_temp_alloc(&temp, pattern_count * sizeof(struct adt_i_row));
    if (rows == NULL)
    {
        adt_i_temp_free_all(&temp);
        return ADT_ERROR_NOMEM;
    }
    for (i = 0; i < pattern_count; ++i)
    {
        adt_i_row_init(&rows[i], &patterns[i]);
    }
    qcols = adt_i_temp_alloc(&temp, sizeof(struct adt_pattern *));
    if (qcols == NULL)
    {
        adt_i_temp_free_all(&temp);
        return ADT_ERROR_NOMEM;
    }
    qcols[0] = NULL;
    q.cols = qcols;
    q.width = 1;
    fams = adt_i_temp_alloc(&temp, sizeof(const struct adt_type *));
    if (fams == NULL)
    {
        adt_i_temp_free_all(&temp);
        return ADT_ERROR_NOMEM;
    }
    fams[0] = type;
    useful = adt_i_useful(type->ctx, &temp, rows, pattern_count, &q, 1, fams);
    adt_i_temp_free_all(&temp);
    if (useful < 0)
    {
        return ADT_ERROR_NOMEM;
    }
    if (useful == 0)
    {
        *result = ADT_MATCH_EXHAUSTIVE;
    }
    else
    {
        *result = ADT_MATCH_NONEXHAUSTIVE;
    }
    return ADT_OK;
}

static int adt_i_family_from(const struct adt_pattern *p,
                             const struct adt_type **out)
{
    int w;
    w = adt_i_wc(p);
    if (w != 0)
    {
        return 0;
    }
    *out = p->u.ctor.ctor->owner;
    return 1;
}

static int adt_i_take_ctor_family(const struct adt_pattern *const *patterns,
                                  size_t count, const struct adt_type **out)
{
    size_t i;
    int got;
    for (i = 0; i < count; ++i)
    {
        got = adt_i_family_from(patterns[i], out);
        if (got != 0)
        {
            return 1;
        }
    }
    return 0;
}

static void adt_i_mark_after_first(size_t count, unsigned char *redundant)
{
    size_t i;
    redundant[0] = 0;
    for (i = 1; i < count; ++i)
    {
        redundant[i] = 1;
    }
}

static int adt_i_useful_prefix(struct adt_ctx *ctx, struct adt_i_temp *temp,
                               const struct adt_type *fam,
                               adt_pattern **patterns, size_t up_to)
{
    struct adt_i_row *rows;
    struct adt_i_row q;
    const struct adt_type **fams;
    size_t j;
    int result;
    fams = adt_i_temp_alloc(temp, sizeof(const struct adt_type *));
    if (fams == NULL)
    {
        return -1;
    }
    fams[0] = fam;
    rows = adt_i_temp_alloc(temp, up_to * sizeof(struct adt_i_row));
    if (rows == NULL)
    {
        return -1;
    }
    for (j = 0; j < up_to; ++j)
    {
        adt_i_row_init(&rows[j], &patterns[j]);
    }
    q.cols = &patterns[up_to];
    q.width = 1;
    result = adt_i_useful(ctx, temp, rows, up_to, &q, 1, fams);
    return result;
}

adt_status adt_match_redundant(size_t pattern_count, adt_pattern **patterns,
                               unsigned char *out_redundant)
{
    struct adt_i_temp temp;
    struct adt_ctx *ctx;
    const struct adt_type *fam;
    size_t i;
    int has;
    int result;
    if (patterns == NULL)
    {
        return ADT_ERROR_INVALID;
    }
    if (out_redundant == NULL)
    {
        return ADT_ERROR_INVALID;
    }
    fam = NULL;
    has = adt_i_take_ctor_family((const struct adt_pattern *const *)patterns,
                                 pattern_count, &fam);
    if (has == 0)
    {
        adt_i_mark_after_first(pattern_count, out_redundant);
        return ADT_OK;
    }
    ctx = fam->ctx;
    temp.ctx = ctx;
    temp.head = NULL;
    for (i = 0; i < pattern_count; ++i)
    {
        result = adt_i_useful_prefix(ctx, &temp, fam, patterns, i);
        if (result < 0)
        {
            adt_i_temp_free_all(&temp);
            return ADT_ERROR_NOMEM;
        }
        if (result == 0)
        {
            out_redundant[i] = 1;
        }
        else
        {
            out_redundant[i] = 0;
        }
    }
    adt_i_temp_free_all(&temp);
    return ADT_OK;
}
