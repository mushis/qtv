// fs.c -- file system related

#include "qtv.h"

FILE *FS_OpenFile(char *gamedir, char *filename, int *size)
{
	FILE *f = NULL;
	char name[1024]; // FIXME: different OSes have different max path len

	size[0] = -1;

	if (!gamedir || !gamedir[0])
		gamedir = "qw"; // use qw if ommited

	snprintf(name, sizeof(name), "%s/%s", gamedir, filename);

	if (!(f = fopen(name, "rb")))
	{
		if (strcmp(gamedir, "id1")) // we fail, try id1 gamedir, sure if it not alredy so
		{
			snprintf(name, sizeof(name), "id1/%s", filename);
			f = fopen(name, "rb");
		}
	}

	if (f)
	{
		fseek(f, 0, SEEK_END);
		size[0] = ftell(f);
		fseek(f, 0, SEEK_SET);

		if (size[0] < 0)
		{
			fclose(f);
			f = NULL;
		}
	}

	return f;
}

// open and load file in memory.
// may be used in two ways: 
// 1) user provide buffer, in this case "size" provides buffer size.
// 2) or function automatically allocate it, in this case need _FREE_ memory when it no more needed.
//
// in both cases after returning from function "size" will reflect actual data length.

char *FS_ReadFile(char *gamedir, char *filename, char *buf, int *size)
{
	FILE *f = NULL;
	int buf_size;

	if (!size)
		return NULL; // we need return filesize somehow, so this is not valid

	if (buf) // using pre alloced by user buffer
	{
		if (size[0] < 1)
		{
			size[0] = -1;
			return NULL; // specified buffer not valid, zero or less sized
		}

		buf_size = size[0]; // greater than zero, valid
	}
	else
		buf_size = 0; // this trigger use of Sys_malloc() below

	if (!(f = FS_OpenFile(gamedir, filename, size)))
		return NULL;

	if (buf_size) // using pre alloced by user buffer
	{
		if (buf_size < size[0] + 1) // can't fit file in user buffer, count null terminator too
		{
			fclose(f);
			size[0] = -1;
			return NULL;
		}
	}
	else // malloc buffer
	{
		buf_size = size[0] + 1; // space for null terminator, quake style
		buf	= Sys_malloc(buf_size);
	}

	if (fread(buf, size[0], 1, f) != 1)
	{ // read error
		fclose(f);
		size[0] = -1;
		return NULL;
	}

	fclose(f); // do not need it anymore

	buf[size[0]] = 0;
	return buf;
}

void FS_StripPathAndExtension(char *filepath)
{
	size_t lastslash = -1;
	size_t lastdot   = -1;
	size_t i		 = 0;

	for ( ; filepath[i]; i++)
	{
		if (filepath[i] == '/' || filepath[i] == '\\')
			lastslash = i;
		if (filepath[i] == '.')
			lastdot = i;
	}
	
	if (lastdot == -1 || lastdot < lastslash)
		lastdot = i;

	strlcpy(filepath, filepath + lastslash + 1, lastdot - lastslash);
}
