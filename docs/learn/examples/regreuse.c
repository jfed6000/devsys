union u { char c[2]; int i; };

int both(p)
union u *p;
{
    char c;
    int i;

    c = p->c[0];
    i = p->i;
    return c + i;
}
