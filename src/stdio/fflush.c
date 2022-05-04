#include "stdio_impl.h"

extern int __m3_fflush(int fd);

/* stdout.c will override this if linked */
static FILE *volatile dummy = 0;
weak_alias(dummy, __stdout_used);
weak_alias(dummy, __stderr_used);

int fflush(FILE *f)
{
	if (!f) {
		int r = 0;
		if (__stdout_used) r |= fflush(__stdout_used);
		if (__stderr_used) r |= fflush(__stderr_used);

		for (f=*__ofl_lock(); f; f=f->next) {
			FLOCK(f);
			if (f->wpos != f->wbase) r |= fflush(f);
			FUNLOCK(f);
		}
		__ofl_unlock();

		return r;
	}

	FLOCK(f);

	/* If writing, flush output */
	if (f->wpos != f->wbase) {
		f->write(f, 0, 0);
		if (!f->wpos) {
			FUNLOCK(f);
			return EOF;
		}
	}

	// explicitly tell our translation layer that the file should be flushed; do that even if wpos
	// == wbase, because it could be that as far as musl is concerned the buffer is flushed, but M³
	// still didn't do that, because it was a normal write call.
	__m3_fflush(f->fd);

	/* If reading, sync position, per POSIX */
	if (f->rpos != f->rend) f->seek(f, f->rpos-f->rend, SEEK_CUR);

	/* Clear read and write modes */
	f->wpos = f->wbase = f->wend = 0;
	f->rpos = f->rend = 0;

	FUNLOCK(f);
	return 0;
}

weak_alias(fflush, fflush_unlocked);
