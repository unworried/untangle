#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define MAX_WORDS 128
#define MAX_INPUT 512

#define DBG_TEXT_INIT
#define DBG_WORD_EQ

struct word {
    size_t off;
    size_t len;
};

struct text {
    const char  *buf;
    size_t      len;
    struct word ws[MAX_WORDS];
    size_t      nws;
};

void text_init(struct text *t, const char *buf)
{
    size_t i, start;

    t->buf = buf;
    t->len = strlen(buf);
    t->nws = 0;

    i = 0;
    while (i < t->len) {
        while (i < t->len && isspace(t->buf[i]))
            i++;

        if (i == t->len)
            break;

        start = i;

        while (i < t->len && !isspace(t->buf[i]))
            i++;

        if (t->nws == MAX_WORDS)
            break;

        t->ws[t->nws].off = start;
        t->ws[t->nws].len = i - start;
        t->nws++;
    }

#ifdef DBG_TEXT_INIT
    printf("dbg: text_init() <= buf[ %s ]\n", buf);
    for (size_t j = 0; j < t->nws; j++)
        printf("dbg: text_init() => off[ %zu ] : len[ %zu ]\n",
                t->ws[j].off, t->ws[j].len);
#endif /* DBG_TEXT_INIT */
}

static int word_eq(const struct text *a, size_t ai, const struct text *b, size_t bi)
{
    const struct word *aw, *bw;

    aw = &a->ws[ai];
    bw = &b->ws[bi];

#ifdef DBG_WORD_EQ
    printf("dbg: word_eq() => a[ ");
    fwrite(a->buf + aw->off, 1, aw->len, stdout);
    printf(" ] : b[ ");
    fwrite(b->buf + bw->off, 1, bw->len, stdout);
    printf(" ]\n");
#endif /* DBG_WORD_EQ */

    if (aw->len != bw->len)
        return 0;

    return 0 == memcmp(a->buf + aw->off, b->buf + bw->off, aw->len);
}


int main(void)
{
    const char *target_buf = "the quick brown fox jumps over the lazy dog";
    char input_buf[MAX_INPUT];
    struct text target, input;
    size_t i, correct, incorrect, total;

    text_init(&target, target_buf);

    printf("%s\n", target_buf);

    if (fgets(input_buf, sizeof(input_buf), stdin) == NULL)
        return 1;

    input_buf[strcspn(input_buf, "\n")] = '\0';
    text_init(&input, input_buf);

    correct = 0;
    incorrect = 0;
    total = target.nws;

    for (i = 0; i < target.nws; i++) {
        if (i >= input.nws) {
            incorrect++;
            continue;
        }

        if (word_eq(&target, i, &input, i))
            correct++;
        else
            incorrect++;
    }

    printf("correct: %zu\n", correct);
    printf("incorrect: %zu\n", incorrect);
    printf("total: %zu\n", total);

    return 0;
}
