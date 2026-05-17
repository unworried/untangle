#include <sys/types.h>

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAX_WORDS	128
#define MAX_WORD_INPUT	64

#define RED		    "\033[31m"
#define GREY        "\033[90m"
#define RESET		"\033[0m"
#define CLEAR_LINE	"\r\033[2K"

/* DEBUG CTL */
#define DBG_TEXT_INIT
#define DBG_WORD_EQ

struct word {
	size_t	 off;
	size_t	 len;
};

struct text {
	const char	*buf;
	size_t		 len;
	struct word	 ws[MAX_WORDS];
	size_t		 nws;
};

struct typed_word {
	char	 buf[MAX_WORD_INPUT];
	size_t	 len;
};

struct session {
	struct text	 target;
	struct typed_word typed[MAX_WORDS];
	size_t		 word;
	size_t		 correct_chars;
	size_t		 incorrect_chars;
	int		 running;
};

static struct termios orig_termios;

static void	term_restore(void);
static void	on_signal(int);
static int	term_cbreak(void);
static void	text_init(struct text *, const char *);
static int	word_eq(const struct text *, size_t, const struct typed_word *);
static void	put_red_ch(char);
static void	render_word(const struct text *, size_t, const struct typed_word *);
static void	render_session(const struct session *);
static void	session_insert(struct session *, char);
static void	session_space(struct session *);
static void	session_backspace(struct session *);
static void	session_run(struct session *);
static void	session_score(const struct session *, size_t *, size_t *);

static void term_restore(void)
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void on_signal(int signo)
{
	term_restore();
	_exit(128 + signo);
}

static int term_cbreak(void)
{
	struct termios t;

	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
		return -1;

	t = orig_termios;
	t.c_lflag &= ~(ECHO | ICANON);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) == -1)
		return -1;

	atexit(term_restore);
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	return 0;
}

static void text_init(struct text *t, const char *buf)
{
	size_t i, start;

	t->buf = buf;
	t->len = strlen(buf);
	t->nws = 0;

	i = 0;
	while (i < t->len) {
		while (i < t->len && isspace((unsigned char)t->buf[i]))
			i++;

		if (i == t->len)
			break;

		start = i;

		while (i < t->len && !isspace((unsigned char)t->buf[i]))
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

static int word_eq(const struct text *a, size_t ai, const struct typed_word *tw)
{
	const struct word *aw;

	aw = &a->ws[ai];

#ifdef DBG_WORD_EQ
	printf("dbg: word_eq() => a[ ");
	fwrite(a->buf + aw->off, 1, aw->len, stdout);
	printf(" ] : b[ ");
	fwrite(tw->buf, 1, tw->len, stdout);
	printf(" ]\n");
#endif /* DBG_WORD_EQ */

	if (aw->len != tw->len)
		return 0;

	return memcmp(a->buf + aw->off, tw->buf, aw->len) == 0;
}

static void put_red_ch(char c)
{
	write(STDOUT_FILENO, RED, sizeof(RED) - 1);
	write(STDOUT_FILENO, &c, 1);
	write(STDOUT_FILENO, RESET, sizeof(RESET) - 1);
}

static void put_grey_ch(char c)
{
	write(STDOUT_FILENO, GREY, sizeof(GREY) - 1);
	write(STDOUT_FILENO, &c, 1);
	write(STDOUT_FILENO, RESET, sizeof(RESET) - 1);
}

static void render_word(const struct text *t, size_t wi, const struct typed_word *tw)
{
	const struct word *w;
	size_t i;
	char want, got;

	w = &t->ws[wi];

	for (i = 0; i < w->len; i++) {
		want = t->buf[w->off + i];

		if (i >= tw->len) {
			// write(STDOUT_FILENO, &want, 1);
            put_grey_ch(want);
			continue;
		}

		got = tw->buf[i];

		if (got == want)
			write(STDOUT_FILENO, &want, 1);
		else
			put_red_ch(want);
	}

	for (; i < tw->len; i++)
		put_red_ch(tw->buf[i]);
}

static void render_session(const struct session *s)
{
	size_t i;
	char space;

	space = ' ';

	write(STDOUT_FILENO, CLEAR_LINE, sizeof(CLEAR_LINE) - 1);

	for (i = 0; i < s->target.nws; i++) {
		render_word(&s->target, i, &s->typed[i]);

		if (i + 1 < s->target.nws)
			write(STDOUT_FILENO, &space, 1);
	}
}

static void session_insert(struct session *s, char c)
{
	struct typed_word *tw;
	const struct word *w;

	if (s->word >= s->target.nws)
		return;

	tw = &s->typed[s->word];
	w = &s->target.ws[s->word];

	if (tw->len == sizeof(tw->buf) - 1)
		return;

	if (tw->len < w->len && c == s->target.buf[w->off + tw->len])
		s->correct_chars++;
	else
		s->incorrect_chars++;

	tw->buf[tw->len++] = c;
	tw->buf[tw->len] = '\0';
}

static void session_space(struct session *s)
{
	if (s->word + 1 < s->target.nws)
		s->word++;
	else
		s->running = 0;
}

static void session_backspace(struct session *s)
{
	struct typed_word *tw;

	if (s->word >= s->target.nws)
		return;

	tw = &s->typed[s->word];

	if (tw->len > 0) {
		tw->buf[--tw->len] = '\0';
		return;
	}

	if (s->word > 0)
		s->word--;
}

static void session_run(struct session *s)
{
	char c;

	s->running = 1;
	render_session(s);

	while (s->running) {
		if (read(STDIN_FILENO, &c, 1) != 1)
			break;

		if (c == '\n' || c == '\r')
			break;

		if (c == 127 || c == '\b')
			session_backspace(s);
		else if (c == ' ')
			session_space(s);
		else if (isprint((unsigned char)c))
			session_insert(s, c);

		render_session(s);
	}
}

static void session_score(const struct session *s, size_t *correct, size_t *incorrect)
{
	size_t i;

	*correct = 0;
	*incorrect = 0;

	for (i = 0; i < s->target.nws; i++) {
		if (word_eq(&s->target, i, &s->typed[i]))
			(*correct)++;
		else
			(*incorrect)++;
	}
}

int main(void)
{
	const char *target_buf = "the quick brown fox jumps over the lazy dog";
	struct session s;
	size_t correct_words, incorrect_words;

	memset(&s, 0, sizeof(s));
	text_init(&s.target, target_buf);

	if (term_cbreak() == -1)
		return 1;

	session_run(&s);
	term_restore();
	putchar('\n');

	session_score(&s, &correct_words, &incorrect_words);

	printf("correct chars:   %zu\n", s.correct_chars);
	printf("incorrect chars: %zu\n", s.incorrect_chars);
	printf("correct words:   %zu\n", correct_words);
	printf("incorrect words: %zu\n", incorrect_words);
	printf("total words:     %zu\n", s.target.nws);

	return 0;
}
