/*
	fp6809.c - the 6809's floating point, for the cross-compiler (devsys)

	dcc68 on the 6809 builds a double constant with the C library's own
	arithmetic (cfloats.a): lex.c sums the digits in a 56-bit integer,
	normaliz() makes it a double, scale() multiplies or divides it by a
	power of 5 and moves the exponent by the power of 2. The other
	compile-time work on a constant is negating it and converting it to
	and from int, long and float. Here the same steps are done on the
	same bit patterns, the way cfloats.a does them, rounding and all, so
	that dcc68 on Linux writes the bytes dcc68 on the F256 writes. (It
	used the host's IEEE doubles: 3 bits short of the 6809's 56, and
	HUGE_VAL, 2^127 there, wrapped to 0.)

	A double is 8 bytes, most significant first: the sign in bit 7 of
	byte 0, then 55 bits of mantissa whose leading 1 is implied (the sign
	takes its place), then an exponent byte with a bias of 128: the value
	is 0.1mmm... * 2^(e-128), and e = 0 is zero. A float is the same with
	3 bytes of mantissa. The mantissas below are 56-bit integers, leading
	1 included (bit 55).

	Where cfloats.a fails (an exponent out of range, a double too big for
	a long) it sets errno and sends the process signal 40 or 42
	(_rpterr); dcc68's intercept, tidy(), ends it with that status
	(without flushing its output). This ends the same way, with a word.
*/

#include "cj.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

typedef uint64_t u64;

#define M56	(((u64) 1 << 56) - 1)
#define TOP	((u64) 1 << 55)

/* a double's mantissa (leading 1 put back), from bytes 0..6 */
static u64
mant (d)
dbl6809 *d;
{
	u64 m = d->b[0] | 0x80;
	int i;

	for (i = 1; i < 7; ++i)
		m = (m << 8) | d->b[i];
	return m;
}

/* bytes 0..6 from a mantissa, with the sign in bit 7 of byte 0 */
static
setmant (d, m, sign)
dbl6809 *d;
u64 m;
int sign;
{
	int i;

	for (i = 6; i > 0; --i, m >>= 8)
		d->b[i] = m;
	d->b[0] = (m & 0x7f) | (sign & 0x80);
}

static
fpfail (code)
int code;
{
	fprintf (stderr, "floating point %s (error %d)\n",
			code == 40 ? "exponent out of range" : "conversion", code);
	errno = code;
	tidy ();
}

/* The end of _dmul and _ddiv: _just (normalise), _ovrchk, _rndup.
 * m is op1's mantissa (56 bits, perhaps not normalised), wrk0 the byte
 * below it, exovr:exb the exponent as the routine left it, sign the
 * result's sign in bit 7. */
static
finish (r, m, wrk0, exovr, exb, sign)
dbl6809 *r;
u64 m;
int wrk0, exovr, exb, sign;
{
	int i;

	if (!(m & TOP)) {
		/* never zero here: both operands were non-zero */
		do {
			if (--exb & 0xff)
				;
			else
				--exovr;
			exb &= 0xff;
			m = ((m << 1) | ((wrk0 >> 7) & 1)) & M56;
			wrk0 = (wrk0 << 1) & 0xff;
		} while (!(m & TOP));
		if (exb == 0)
			goto ovrchk;
	}
	setmant (r, m, sign);
	r->b[7] = exb;
	if (exovr & 0xff)
		goto ovrchk;
	if (wrk0 & 0x80) {
		/* _rndup: add one to bytes 6..1; a carry out of byte 1 goes
		 * into byte 0's 7 bits and from there into the exponent */
		for (i = 6; i > 0; --i)
			if (++r->b[i])
				return;
		i = (r->b[0] & 0x7f) + 1;
		if (i & 0x80) {
			++r->b[7];
			i &= 0x7f;
		}
		r->b[0] = i | (r->b[0] & 0x80);
	}
	return;
ovrchk:
	fpfail (40);	/* too big; too small is zero, but reported the same */
}

static int
iszero (d)
dbl6809 *d;
{
	return d->b[7] == 0;
}

static
zero (d)
dbl6809 *d;
{
	int i;

	for (i = 0; i < 8; ++i)
		d->b[i] = 0;
}

/* _dmul: r = a * b. The mantissas are multiplied by shifting and adding
 * (_dmulm): op1 gets the top 56 bits of the 112-bit product, wrk the
 * rest; the bit below the result decides the rounding (half up). */
fp_mul (r, a, b)
dbl6809 *r, *a, *b;
{
	u64 op1 = 0, wrk = mant (a), op2 = mant (b), c;
	int i, e;

	if (iszero (a) || iszero (b)) {
		zero (r);
		return;
	}
	for (i = 0; i < 56; ++i) {
		c = 0;
		if (wrk & 1) {
			op1 += op2;
			c = op1 >> 56;
			op1 &= M56;
		}
		wrk = (wrk >> 1) | ((op1 & 1) << 55);
		op1 = (op1 >> 1) | (c << 55);
	}
	e = a->b[7] + b->b[7] - 0x80;
	finish (r, op1, (int) (wrk >> 48) & 0xff, (e >> 8) & 0xff, e & 0xff,
			(a->b[0] ^ b->b[0]) & 0x80);
}

/* _ddiv: r = a / b, by _ddivm's restoring division: 57 quotient bits,
 * the first of which is shifted out of op1 into wrk's bottom bit on the
 * last round. _ddivs puts it back on top: the result's 56 bits are the
 * first 56 quotient bits, and the 57th is the rounding bit, unless the
 * first was 0; then the result is bits 2..57 and the rounding bit is
 * the remainder's top bit (the carry out of wrk's last shift), not the
 * 58th quotient bit. */
fp_div (r, a, b)
dbl6809 *r, *a, *b;
{
	u64 op1 = 0, wrk, op2, t;
	int count, c, c1, c2 = 0, wrk0, e;

	if (iszero (b))
		fpfail (41);	/* divide by zero */
	if (iszero (a)) {
		zero (r);
		return;
	}
	wrk = mant (a);
	op2 = mant (b);
	count = 57;
	c = 1;
	for (;;) {
		/* _ddivm1: does op2 go into wrk? c = 1 when it doesn't */
		if ((wrk >> 48) < (op2 >> 48))
			c = 1;
		else if (wrk < op2)
			c = 1;
		else {
			wrk -= op2;
			c = 0;
		}
shift:
		/* _ddivm2: shift op1:wrk left, c into op1's bottom */
		c1 = (op1 >> 55) & 1;
		op1 = ((op1 << 1) | c) & M56;
		c2 = (wrk >> 55) & 1;
		wrk = ((wrk << 1) | c1) & M56;
		if (--count > 0 && !c2)
			continue;
		if (count == 0)
			break;
		/* a bit carried out of wrk: op2 surely goes in */
		wrk = (wrk - op2) & M56;
		c = 0;
		goto shift;
	}
	/* _ddivm3: "ror _wrk" (byte 0 only), then the answer was flipped */
	wrk0 = (int) (wrk >> 48) & 0xff;
	c1 = wrk0 & 1;
	wrk0 = (wrk0 >> 1) | (c2 << 7);
	wrk = (wrk & ~((u64) 0xff << 48)) | ((u64) wrk0 << 48);
	op1 = ~op1 & M56;
	/* _ddivs: the bottom bit of wrk (the first quotient bit, flipped)
	 * back on top of op1 */
	c = !(wrk & 1);
	t = op1 & 1;
	op1 = (op1 >> 1) | ((u64) c << 55);
	wrk0 = (wrk0 >> 1) | ((int) t << 7);
	e = a->b[7] - b->b[7] + 0x81;
	finish (r, op1, wrk0, (e >> 8) & 0xff, e & 0xff,
			(a->b[0] ^ b->b[0]) & 0x80);
}

/* _dadd: r = a + b, as _dadds does it. The operand with the smaller
 * exponent is shifted right to the other's; the last bit shifted out
 * is the carry into the add (or, for a subtract, the borrow: inverted
 * when op1 was the one shifted). Equal exponents add with a carry of 1
 * (half up, after the shift down that such a sum always needs). A
 * negative difference is complemented and, as wrk is then non-zero,
 * given its 1 back by _rndup. No rounding after: _dadd doesn't call
 * _rndup at the end as _dmul and _ddiv do. */
fp_add (r, a, b)
dbl6809 *r, *a, *b;
{
	u64 op1, op2, s;
	int ea = a->b[7], eb = b->b[7], diff, c = 0, wrk = 0, exb, sign;
	int sub = (a->b[0] ^ b->b[0]) & 0x80, borrow;

	if (ea == 0) {			/* _x2op1: the answer is op2 */
		*r = *b;
		return;
	}
	if (eb == 0) {			/* _dsubs1: op1 */
		*r = *a;
		return;
	}
	op1 = mant (a);
	op2 = mant (b);
	exb = ea;
	diff = (eb - ea) & 0xff;
	if (diff == 0) {
		wrk = 1;		/* inc _wrk: garbage + 1, not 0 */
		c = 1;
		if (sub)
			c = 0;		/* _dadds9 inverts it */
		goto op1big;
	} else if (eb > ea) {
		/* _dadds2: op1 shifted down to op2's exponent */
		exb = eb;
		while (diff--) {
			c = op1 & 1;
			op1 >>= 1;
		}
		if (sub) {
			c = !c;		/* _dadds9 */
			goto subtract;
		}
		goto add;
	} else {
		/* _dadds6: op2 shifted down */
		diff = (ea - eb) & 0xff;
		while (diff--) {
			c = op2 & 1;
			op2 >>= 1;
		}
op1big:
		if (sub)
			goto subtract;
	}
add:
	s = op1 + op2 + c;
	if (s >> 56) {
		exb = (exb + 1) & 0xff;
		s >>= 1;	/* the carry comes back in on top */
	}
	op1 = s & M56;
	sign = b->b[0];			/* _dadds8: op2's sign */
	goto just;
subtract:
	borrow = c;
	s = op1 - op2 - borrow;
	sign = a->b[0];
	if (op1 < op2 + borrow) {
		s = ~s & M56;		/* com: -(op1 - op2 - c) - 1 */
		if (wrk) {
			/* _rndup on the raw mantissa: bytes 6..1, then 7 bits of
			 * byte 0 (from there into the exponent) */
			u64 lo = (s & (((u64) 1 << 48) - 1)) + 1;

			if (lo >> 48) {
				int top = (int) (s >> 48) & 0xff, t = (top & 0x7f) + 1;

				if (t & 0x80) {
					exb = (exb + 1) & 0xff;
					t &= 0x7f;
				}
				s = ((u64) ((t | (top & 0x80)) & 0xff) << 48);
			} else
				s = (s & ((u64) 0xff << 48)) | lo;
		}
		sign = b->b[0];		/* _dadds11 */
	}
	op1 = s & M56;
just:
	/* _dadds13, _just: wrk is 0 now, so a shift up brings in zeros */
	if (!(op1 & TOP)) {
		int exovr = 0;

		if (op1 == 0) {
			zero (r);
			return;
		}
		do {
			if (!(--exb & 0xff))
				--exovr;
			exb &= 0xff;
			op1 <<= 1;
		} while (!(op1 & TOP));
		if (exb == 0 || (exovr & 0xff))
			fpfail (40);
	}
	setmant (r, op1, sign);
	r->b[7] = exb;
}

/* _dneg: flip the sign, unless all 8 bytes are 0 */
fp_neg (d)
dbl6809 *d;
{
	int i;

	for (i = 0; i < 8; ++i)
		if (d->b[i]) {
			d->b[0] ^= 0x80;
			return;
		}
}

/* a non-negative integer, exactly: _ltod's result for a long */
fp_fromint (d, n, sign)
dbl6809 *d;
u64 n;
int sign;
{
	int e = 128;
	u64 t;

	if (n == 0) {
		zero (d);
		return;
	}
	for (t = n; t; t >>= 1)
		++e;
	while (!(n & TOP))
		n <<= 1;
	setmant (d, n, sign);
	d->b[7] = e;
}

/* _ltod (and _itod, after sign extension) */
fp_ltod (d, l)
dbl6809 *d;
long l;
{
	int32_t v = l;

	if (v < 0)
		fp_fromint (d, (u64) -(int64_t) v, 0x80);
	else
		fp_fromint (d, (u64) v, 0);
}

/* _dtol: rounds down (towards minus infinity: a negative value with a
 * fraction gets one more in magnitude); too big for a long is error 42 */
long
fp_dtol (d)
dbl6809 *d;
{
	int e = d->b[7], neg = d->b[0] & 0x80, i;
	u64 m;
	int64_t v;

	if (e == 0)
		return 0;
	if (e < 0x81)
		return neg ? -1 : 0;
	if (e >= 160) {
		/* only -2^31 itself: sign set, the rest of the mantissa 0 */
		if (e == 160 && d->b[0] == 0x80) {
			for (i = 1; i < 7 && !d->b[i]; ++i)
				;
			if (i == 7)
				return (long) INT32_MIN;
		}
		fpfail (42);
	}
	m = mant (d);
	v = m >> (56 - (e - 128));
	if (neg) {
		if (m & ((((u64) 1) << (56 - (e - 128))) - 1))
			++v;
		v = -v;
	}
	return (long) (int32_t) v;
}

/* _dtof: the first 3 bytes and the exponent, cut, not rounded */
fp_dtof (f, d)
flt6809 *f;
dbl6809 *d;
{
	f->b[0] = d->b[0];
	f->b[1] = d->b[1];
	f->b[2] = d->b[2];
	f->b[3] = d->b[7];
}

/* scale.c on the 6809: the powers of 5 it multiplies tables entries
 * for, as words */
static unsigned short atoftbl[] = {
	0, 0, 0, 0x81,				/* 5^0 (1) */
	0x2000, 0, 0, 0x83,			/* 5^1 (5) */
	0x4800, 0, 0, 0x85,			/* 5^2 (25) */
	0x7a00, 0, 0, 0x87,			/* 5^3 (125) */
	0x1c40, 0, 0, 0x8a,			/* 5^4 (625) */
	0x4350, 0, 0, 0x8c,			/* 5^5 (3125) */
	0x7424, 0, 0, 0x8e,			/* 5^6 (15625) */
	0x1896, 0x8000, 0, 0x91,	/* 5^7 */
	0x3ebc, 0x2000, 0, 0x93,	/* 5^8 */
	0x6e6b, 0x2800, 0, 0x95,	/* 5^9 */
	0x1502, 0xf900, 0, 0x98,	/* 5^10 */
	0x2d78, 0xebc5, 0xac62, 0x00af,	/* 5^20 */
	0x49f2, 0xc9cd, 0x0467, 0x4fc6,	/* 5^30 */
	0x6b19, 0x4f8e, 0x1ae5, 0x26dd,	/* 5^40 */
	0x08d8, 0x762b, 0xf324, 0xcdf5	/* 5^50 */
};

static
tbl (d, i)
dbl6809 *d;
int i;
{
	int k;

	if (i > 14) {
		/* the 6809 reads past the table: no defined value */
		fprintf (stderr, "floating point constant out of range\n");
		errno = 40;
		tidy ();
	}
	for (k = 0; k < 4; ++k) {
		d->b[2 * k] = atoftbl[4 * i + k] >> 8;
		d->b[2 * k + 1] = atoftbl[4 * i + k];
	}
}

/* scale(): d * 5^cnt (sign) or d / 5^cnt, then the exponent byte moved
 * by cnt, as the 6809 does it: in 8 bits, so out of range wraps */
dbl6809
scale (d, cnt, sign)
dbl6809 d;
int cnt, sign;
{
	dbl6809 p, q, r;

	if (cnt && d.b[7]) {
		if (cnt > 9) {
			tbl (&p, cnt % 10);
			tbl (&q, cnt / 10 + 9);
			fp_mul (&r, &p, &q);
		} else
			tbl (&r, cnt);
		if (sign)
			fp_mul (&d, &d, &r);
		else
			fp_div (&d, &d, &r);
		if (sign)
			d.b[7] += cnt;
		else
			d.b[7] -= cnt;
	}
	return d;
}

/* lex.c's normaliz() on the 6809: the digit sum n (7 bytes) as
 * n[0] * 2^32 + n[1], where n[1] is a signed long, so n[0] gets one
 * more when n[1]'s top bit is set: _ltod, _dmul by 2^32, _ltod, _dadd.
 * Exact except at the very top (n[0] + 1 = 2^24), where _dadd's
 * alignment shifts a bit out and rounds its own way. */
fp_normaliz (d, n)
dbl6809 *d;
u64 n;
{
	dbl6809 hi, two32, lo;
	long n0 = (long) (n >> 32), n1 = (long) (int32_t) (uint32_t) n;
	int i;

	if (n1 < 0)
		++n0;
	fp_ltod (&hi, n0);
	for (i = 0; i < 7; ++i)
		two32.b[i] = 0;
	two32.b[7] = 128 + 33;
	fp_mul (&hi, &hi, &two32);
	fp_ltod (&lo, n1);
	fp_add (d, &hi, &lo);
}
