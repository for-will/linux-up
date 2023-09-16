int total = 0;
static int count = 13;

static int add(int a, int b)
{
	total += a;
	total += b;
	count++;
	return a + b;
}

void call_add()
{
	int a = 100;
	int b = 200;
	int c;
	c = add(a, b);
	if (c == a + b) {
		c = 0;
	}
}
