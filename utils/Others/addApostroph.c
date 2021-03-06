#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define NATOMS 120
#define BSIZE 1024

/**********************************************/
void str_delete_n(char* str)
{
	char *s;

	if(str == NULL)
		return;

	if (!*str)
		return;
	for (s = str + strlen (str) - 1; s >= str && ((unsigned char)*s)=='\n'; s--)
		*s = '\0';
}
/**********************************************/
void delete_last_spaces(char* str)
{
	char *s;

	if(str == NULL)
		return;

	if (!*str)
		return;
	for (s = str + strlen (str) - 1; s >= str && isspace ((unsigned char)*s); s--)
		*s = '\0';
}
/**********************************************/
void delete_first_spaces(char* str)
{
	unsigned char *start;
	int i;
	int lenSpace = 0;

	if(str == NULL)
		return;
	if (!*str)
		return;

	for (start = str; *start && isspace (*start); start++)lenSpace++;

	for(i=0;i<(int)(strlen(str)-lenSpace);i++)
		str[i] = str[i+lenSpace];
	str[strlen(str)-lenSpace] = '\0';
}
int main(int argc,char* argv[])
{
	FILE* fin;
	FILE* fout;
	char t[BSIZE];
	char* filename = NULL;
	if(argc<2)
	 	filename = strdup("p");
	else
		filename = strdup(argv[1]);

	printf("Input file = %s\n",filename);
	printf("Output file = %s\n","p.c");

	fin = fopen(filename,"r");
	if(!fin)
	{
		printf("I can not open %s\n",filename);
		return 1;
	}
	fout = fopen("p.c","w");
	if(!fout)
	{
		printf("I can not open Fragment.c\n");
		return 1;
	}
	fprintf(fout,"\t{\n");
	while(!feof(fin))
	{
		if(!fgets(t,BSIZE,fin))
		break;
		delete_first_spaces(t);
		delete_last_spaces(t);
		str_delete_n(t);
 		fprintf(fout,"\t\t\"%s\",\n",t);
	}
	fprintf(fout,"\t}\n");
	fclose(fin);
	fclose(fout);
	return 0;
}
