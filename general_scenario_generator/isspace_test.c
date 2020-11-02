#include	<stdio.h>
#include	<ctype.h>

int main(int argc, char *argv[])
{
	char	*test_str = " \t\nABC";
	char	*curr;

	for (curr = test_str; *curr; curr++)
	{
		if (isspace(*curr))
			printf("SPACE\n");
		else
			printf("Non-SPACE\n");
	}
}
