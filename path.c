/*
 * Program to manipulate colon-separated path list environment variables.
 * Most importantly, it removes duplicate paths from the path variables.
 * It just prints out the resulting path string. which can be used to set
 * a new path string as in:
 * 	PATH=`path [args]`
 *
 * Copyright (c) 2000 David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>

#include <sys/stat.h>
#include <errno.h>


#define	VERSION	"3.3"


/*
 * Boolean definitions.
 */
typedef	int	BOOL;

#define	FALSE	((BOOL) 0)
#define	TRUE	((BOOL) 1)


/*
 * Special path definitions
 */
#define	PATH_DIVIDER	':'
#define	ROOT_CHARACTER	'/'
#define	DOT_PATH	"."


/*
 * Specially handled options.
 */
#define	OPTION_VAR	"-var"
#define	OPTION_HELP1	"-h"
#define	OPTION_HELP2	"-help"
#define	OPTION_HELP3	"-?"


/*
 * Actions that can be applied to paths specified on the command line.
 */
typedef	int	ACTION;

#define	ACTION_NONE		((ACTION) 0)
#define	ACTION_AFTER		((ACTION) 1)
#define	ACTION_BEFORE		((ACTION) 2)
#define	ACTION_REMOVE		((ACTION) 3)
#define	ACTION_REMOVE_AFTER	((ACTION) 4)
#define	ACTION_REMOVE_BEFORE	((ACTION) 5)
#define	ACTION_MOVE_AFTER	((ACTION) 6)
#define	ACTION_MOVE_BEFORE	((ACTION) 7)
#define	ACTION_SET		((ACTION) 8)
#define	ACTION_LIST		((ACTION) 9)
#define	ACTION_LIST_SORTED	((ACTION) 10)
#define	ACTION_DISABLE_DOT	((ACTION) 11)
#define	ACTION_CHECK_INVALID	((ACTION) 12)
#define	ACTION_REMOVE_INVALID	((ACTION) 13)
#define	ACTION_ALLOW_FILES	((ACTION) 14)
#define	ACTION_CHECK_RELATIVE	((ACTION) 15)
#define	ACTION_REMOVE_RELATIVE	((ACTION) 16)
#define	ACTION_TEST_PRESENCE	((ACTION) 17)


/*
 * The results of checking the validity of a path.
 */
typedef	int	STATUS;

#define	STATUS_KEEP	((STATUS) 0)
#define	STATUS_REMOVE	((STATUS) 1)
#define	STATUS_ERROR	((STATUS) 2)


/*
 * Local data which holds the paths we are working on.
 * This is an array of path names, and the number of paths in the array.
 * The array is NOT terminated with a null pointer.
 */
static	const char **	pathTable;
static	int		pathCount;


/*
 * Option table
 */
typedef	struct
{
	const char *	name;		/* name of option */
	ACTION		action;		/* action code */
	const char *	help;		/* help description */
} OPTION;


static	const OPTION	optionTable[] =
{
	{
		"-var", ACTION_NONE,
		"use the specified environment variable instead of PATH"
	},
	{
		"-a",	ACTION_AFTER,
		"add following paths after current paths if not present"
	},
	{
		"-b",	ACTION_BEFORE,
		"add following paths before current paths if not present"
	},
	{
		"-r",	ACTION_REMOVE,
		"remove following paths from current paths if present"
	},
	{
		"-ra",	ACTION_REMOVE_AFTER,
		"remove and then add following paths after current paths"
	},
	{
		"-rb",	ACTION_REMOVE_BEFORE,
		"remove and then add following paths before current paths"
	},
	{
		"-ma",	ACTION_MOVE_AFTER,
		"move following paths after other current paths if present"
	},
	{
		"-mb",	ACTION_MOVE_BEFORE,
		"move following paths before other current paths if present"
	},
	{
		"-s",	ACTION_SET,
		"set following paths as the current path"
	},
	{
		"-dd",	ACTION_DISABLE_DOT,
		"disable any special treatment of the DOT path"
	},
	{
		"-ci",	ACTION_CHECK_INVALID,
		"check absolute paths in the path list to see if they are invalid"
	},
	{
		"-ri",	ACTION_REMOVE_INVALID,
		"remove absolute paths from the path list which are invalid"
	},
	{
		"-cr",	ACTION_CHECK_RELATIVE,
		"check whether any paths in the path list are relative"
	},
	{
		"-rr",	ACTION_REMOVE_RELATIVE,
		"remove relative paths from the path list"
	},	
	{
		"-tp",	ACTION_TEST_PRESENCE,
		"test whether specified paths are present in the path list"
	},
	{
		"-l",	ACTION_LIST,
		"list current paths one per line instead of in one string"
	},
	{
		"-ls",	ACTION_LIST_SORTED,
		"list sorted current paths one per line instead of in one string"
	},
	{
		"-af",	ACTION_ALLOW_FILES,
		"allow files in addition to directories in paths"
	},
	{
		NULL,	ACTION_NONE,
		NULL
	}
};


/*
 * Option variables.
 */
static	ACTION	action;
static	BOOL	disableDotFlag;
static	BOOL	checkInvalidFlag;
static	BOOL	removeInvalidFlag;
static	BOOL	checkRelativeFlag;
static	BOOL	removeRelativeFlag;
static	BOOL	testPresenceFlag;
static	BOOL	listFlag;
static	BOOL	listSortedFlag;
static	BOOL	allowFilesFlag;
static	BOOL	testFailedFlag;


/*
 * Local procedures.
 */
static	void	RemoveDuplicatePaths(void);
static	void	HandlePathList(int listCount, const char ** listTable);
static	void	HandlePath(const char * path, ACTION action);
static	BOOL	RemovePathFromList(const char * path);
static	BOOL	CheckPathList(void);
static	BOOL	HandleOption(const char * name);
static	STATUS	CheckPath(const char * path);
static	char *	CopyString(const char * oldStr);
static	int	SortCallback(const void * addr1, const void * addr2);
static	void	Usage(void);


int
main(int argc, const char ** argv)
{
	const char *	varName;
	char *		path;
	char *		str;
	const char *	argument;
	const char **	listTable;
	int		listCount;
	int		maxPaths;
	int		index;
	BOOL		dotFirst;
	BOOL		dotLast;

	action = ACTION_AFTER;
	disableDotFlag = FALSE;
	listFlag = FALSE;
	listSortedFlag = FALSE;
	allowFilesFlag = FALSE;
	checkInvalidFlag = FALSE;
	removeRelativeFlag = FALSE;
	checkRelativeFlag = FALSE;
	removeInvalidFlag = FALSE;
	testPresenceFlag = FALSE;
	testFailedFlag = FALSE;
	dotFirst = FALSE;
	dotLast = FALSE;

	/*
	 * Discard the program name.
	 */
	argc--;
	argv++;

	/*
	 * First check for explicit requests for help.
	 * This can be specified for any argument, in which case all
	 * of the other arguments are ignored.
	 */
	for (index = 0; index < argc; index++)
	{
		argument = argv[index];

		if ((strcmp(argument, OPTION_HELP1) == 0) ||
			(strcmp(argument, OPTION_HELP2) == 0) ||
			(strcmp(argument, OPTION_HELP3) == 0))
		{
			Usage();

			return 1;
		}
	}

	/*
	 * See if any argument is the one for the environment variable to
	 * be manipulated.  If not, then use the normal PATH environment
	 * variable name.  This check has to be done first so that the
	 * current path is initialized before it is acted on.  The last
	 * variable name specified is used.
	 */
	varName = "PATH";

	for (index = 0; index < argc; index++)
	{
		if (strcmp(argv[index], OPTION_VAR) != 0)
			continue;

		if ((++index >= argc) || (argv[index][0] == '-'))
		{
			fprintf(stderr, "Missing environment variable name\n");

			return 1;
		}

		varName = argv[index];
	}

	/*
	 * Get the value of the environment variable and copy it so we
	 * can safely modify it.  Don't complain about an undefined
	 * environment variable, but treat it as an empty list to help
	 * shell programmers create a path list from scratch.
	 */
	path = getenv(varName);

	if (path == NULL)
		path = "";

	path = CopyString(path);

	/*
	 * Calculate the maximum number of paths in the new variable
	 * by adding the number of command line arguments to two more
	 * than the number of colons in the existing value.  This will
	 * guarantee that the path table cannot overflow.
	 */
	maxPaths = argc + 2;

	str = path;

	while ((str = strchr(str, PATH_DIVIDER)) != NULL)
	{
		maxPaths++;
		str++;
	}

	/*
	 * Allocate an array which can hold the maximum number of paths.
	 */
	pathTable = (const char **) malloc(sizeof(char **) * maxPaths);

	if (pathTable == NULL)
	{
		fprintf(stderr, "Cannot allocate new path array");

		return 1;
	}

	/*
	 * Initialize the path table with the current path values.
	 * Be careful to make sure that all empty paths are seen
	 * (such as that caused by a trailing colon).
	 */
	pathCount = 0;

	str = path;

	if (*str)
		pathTable[pathCount++] = str;

	while ((str = strchr(str, PATH_DIVIDER)) != NULL)
	{
		*str++ = '\0';
		pathTable[pathCount++] = str;
	}

	/*
	 * Normalize any null paths by converting them all into the
	 * explicit name for the current directory.
	 */
	for (index = 0; index < pathCount; index++)
	{
		if (pathTable[index][0] == '\0')
			pathTable[index] = DOT_PATH;
	}

	/*
	 * Remove all duplicate paths from the table.
	 */
	RemoveDuplicatePaths();

	/*
	 * Remember if the special DOT path is first or last in the list.
	 */
	if (pathCount > 0)
	{
		dotFirst = (strcmp(pathTable[0], DOT_PATH) == 0);

		dotLast = (strcmp(pathTable[pathCount - 1],
			DOT_PATH) == 0);
	}

	/*
	 * Now parse the command line options and associated paths.
	 */
	while (argc > 0)
	{
		/*
		 * If this is the special variable name option, then
		 * just skip over it and its argument since it was
		 * parsed earlier.
		 */
		if (strcmp(*argv, OPTION_VAR) == 0)
		{
			argc -= 2;
			argv += 2;

			continue;
		}

		/*
		 * If the argument is an option then handle that.
		 */
		if (**argv == '-')
		{
			argc--;

			if (!HandleOption(*argv++))
				return 1;

			continue;
		}

		/*
		 * The argument is not an option so it is a path name.
		 * Count how many path names are in the argument list before
		 * the next action (if any) so that we can handle them all
		 * as a group.
		 */
		listTable = argv;
		listCount = 0;

		while ((argc > 0) && (**argv != '-'))
		{
			argc--;
			argv++;
			listCount++;
		}

		/*
		 * Handle the list of paths which were found.
		 */
		HandlePathList(listCount, listTable);
	}

	/*
	 * Remove all duplicate paths from the table.
	 */
	RemoveDuplicatePaths();

	/*
	 * If the DOT path is handled specially, then possibly move it
	 * back to its original position in the list.
	 */
	if (!disableDotFlag && (pathCount > 0) &&
		(strcmp(pathTable[0], DOT_PATH) != 0) &&
		(strcmp(pathTable[pathCount - 1], DOT_PATH) != 0))
	{
		if (dotFirst)
			HandlePath(DOT_PATH, ACTION_MOVE_BEFORE);
		else if (dotLast)
			HandlePath(DOT_PATH, ACTION_MOVE_AFTER);
	}

	/*
	 * Check the paths in the list for validity.
	 * If an error is returned, then exit with a special status
	 * without printing the path list.
	 */
	if (testFailedFlag || !CheckPathList())
		return 2;

	/*
	 * If we were just checking paths, then exit anyway with success.
	 */
	if (testPresenceFlag || checkInvalidFlag || checkRelativeFlag)
		return 0;

	/*
	 * If we want a listing of the paths one per line, then do that.
	 */
	if (listFlag || listSortedFlag)
	{
		/*
		 * If the path list is to be sorted, do that.
		 */
		if (listSortedFlag)
		{
			qsort(pathTable, pathCount, sizeof(const char *),
				SortCallback);
		}

		/*
		 * Now display the list of paths.
		 */
		for (index = 0; index < pathCount; index++)
			puts(pathTable[index]);

		return 0;
	}

	/*
	 * Print out a new path string in the form ready to be assigned
	 * into a new environment variable.
	 */
	for (index = 0; index < pathCount; index++)
	{
		if (index)
			fputc(PATH_DIVIDER, stdout);

		fputs(pathTable[index], stdout);
	}

	fputc('\n', stdout);

	return 0;
}


/*
 * Handle an option argument name (including the leading dash).
 * Returns TRUE on success.
 */
static BOOL
HandleOption(const char * name)
{
	const OPTION *	option;

	/*
	 * Scan the option table looking for the option name.
	 */
	option = optionTable;

	while ((option->name != NULL) && (strcmp(name, option->name) != 0))
		option++;

	/*
	 * Switch on the option type.
	 */
	switch (option->action)
	{
		case ACTION_AFTER:
		case ACTION_BEFORE:
		case ACTION_REMOVE:
		case ACTION_REMOVE_AFTER:
		case ACTION_REMOVE_BEFORE:
		case ACTION_MOVE_AFTER:
		case ACTION_MOVE_BEFORE:
			action = option->action;
			break;

		case ACTION_TEST_PRESENCE:
			action = option->action;
			testPresenceFlag = TRUE;
			break;

		case ACTION_SET:
			pathCount = 0;
			action = ACTION_AFTER;
			break;

		case ACTION_LIST:
			listFlag = TRUE;
			break;

		case ACTION_LIST_SORTED:
			listSortedFlag = TRUE;
			break;

		case ACTION_ALLOW_FILES:
			allowFilesFlag = TRUE;
			break;

		case ACTION_DISABLE_DOT:
			disableDotFlag = TRUE;
			break;

		case ACTION_CHECK_INVALID:
			checkInvalidFlag = TRUE;
			break;

		case ACTION_REMOVE_INVALID:
			removeInvalidFlag = TRUE;
			break;

		case ACTION_CHECK_RELATIVE:
			checkRelativeFlag = TRUE;
			break;

		case ACTION_REMOVE_RELATIVE:
			removeRelativeFlag = TRUE;
			break;

		default:
			/*
			 * The option is not in the table.
			 * Print an error message.
			 */
			fprintf(stderr, "Unknown option \"%s\"\n", name);

			return FALSE;
	}

	return TRUE;
}


/*
 * Handle a list of path names to be acted on by the current action.
 * This list is determined by the number of path names on the command line
 * following one option and before another one.
 */
static void
HandlePathList(int listCount, const char ** listTable)
{
	/*
	 * Handle a few actions specially whose action depends on having all
	 * of the paths in the list at once.
	 */
	switch (action)
	{
		case ACTION_BEFORE:
		case ACTION_REMOVE_BEFORE:
		case ACTION_MOVE_BEFORE:
			/*
			 * Actions which put paths at the beginning must
			 * be executed in reverse order in order to get
			 * the right result.
			 */
			while (listCount-- > 0)
				HandlePath(listTable[listCount], action);

			return;

		default:
			/*
			 * Break to do the default case.
			 */
			break;
	}

	/*
	 * Apply the action each of the paths in list in the specified order.
	 */
	while (listCount-- > 0)
		HandlePath(*listTable++, action);
}


/*
 * Handle the specified path according to the specified action.
 * Note: when a path is added to the pathTable, overflow does not need
 * to be checked since the table was allocated large enough for all cases.
 */
static void
HandlePath(const char * path, ACTION action)
{
	int	index;
	BOOL	removed;

	if (*path == '\0')
		path = DOT_PATH;

	/*
	 * First see which options need to remove the path, and do that.
	 * Remember whether something was removed from the list.
	 */
	removed = FALSE;

	switch (action)
	{
		case ACTION_REMOVE:
		case ACTION_REMOVE_AFTER:
		case ACTION_REMOVE_BEFORE:
		case ACTION_MOVE_AFTER:
		case ACTION_MOVE_BEFORE:
			removed = RemovePathFromList(path);
			break;
	}

	/*
	 * Now do whatever action is required for putting the path back.
	 */
	switch (action)
	{
		case ACTION_MOVE_AFTER:
			if (!removed)
				break;

			/* fall into next case */

		case ACTION_AFTER:
		case ACTION_REMOVE_AFTER:
			pathTable[pathCount++] = path;
			break;

		case ACTION_MOVE_BEFORE:
			if (!removed)
				break;

			/* fall into next case */

		case ACTION_BEFORE:
		case ACTION_REMOVE_BEFORE:
			memmove(pathTable + 1, pathTable,
				pathCount * sizeof(char **));

			pathTable[0] = path;
			pathCount++;
			break;

		case ACTION_REMOVE:
			break;

		case ACTION_TEST_PRESENCE:
			/*
			 * See if the path is in the path table.
			 * If so then everything is all right.
			 */
			for (index = 0; index < pathCount; index++)
			{
				if (strcmp(path, pathTable[index]) == 0)
					return;
			}

			/*
			 * The path is not present.
			 * Remember this failure for later.
			 */
			testFailedFlag = TRUE;
			break;

		default:
			fprintf(stderr, "Unknown action %d\n", action);
			exit(1);
	}
}


/*
 * Look for and remove all occurrances of the specified path from the
 * list of current paths.  Returns TRUE if it was found and removed.
 */
static BOOL
RemovePathFromList(const char * path)
{
	const char *	srcPath;
	int		srcOffset;
	int		destOffset;

	destOffset = 0;

	for (srcOffset = 0; srcOffset < pathCount; srcOffset++)
	{
		srcPath = pathTable[srcOffset];

		if (strcmp(path, srcPath) != 0)
			pathTable[destOffset++] = srcPath;
	}

	/*
	 * Return whether or not the path was found,
	 * and adjust the count of paths if necessary.
	 */
	if (pathCount == destOffset)
		return FALSE;

	pathCount = destOffset;

	return TRUE;
}


/*
 * Remove all duplicate paths from the table of paths.
 * The count of paths is adjusted as necessary.
 */
static void
RemoveDuplicatePaths(void)
{
	const char *	srcPath;
	int		srcOffset;
	int		destOffset;
	int		i;

	destOffset = 0;

	for (srcOffset = 0; srcOffset < pathCount; srcOffset++)
	{
		srcPath = pathTable[srcOffset];

		for (i = 0; i < srcOffset; i++)
		{
			if (strcmp(srcPath, pathTable[i]) == 0)
				break;
		}

		if (i == srcOffset)
			pathTable[destOffset++] = srcPath;
	}

	pathCount = destOffset;
}


/*
 * Check a path for validity according to the command line options.
 * Depending on the options, an error message is printed for invalid paths.
 * Returns one of the following status values:
 *	STATUS_KEEP		Path is valid and should be kept
 *	STATUS_REMOVE		Path is invalid and must be removed
 *	STATUS_ERROR		Path is invalid and generated an error
 */
static STATUS
CheckPath(const char * path)
{
	struct	stat	statbuf;

	/*
	 * See if the path is relative.
	 * If so, then check whether that is allowed.
	 */
	if (*path != ROOT_CHARACTER)
	{
		/*
		 * If we can treat DOT as special, then keep it even when
		 * all other relative paths are disallowed.
		 */
		if (!disableDotFlag && (strcmp(path, DOT_PATH) == 0))
			return STATUS_KEEP;

		/*
		 * If we want to remove relative paths, then return that.
		 */
		if (removeRelativeFlag)
			return STATUS_REMOVE;

		/*
		 * If we are checking relative paths, then give an error
		 * message and return that.
		 */
		if (checkRelativeFlag)
		{
			fprintf(stderr, "Path \"%s\" is relative\n", path);

			return STATUS_ERROR;
		}

		/*
		 * The relative path is allowed and should be kept.
		 */
		return STATUS_KEEP;
	}

	/*
	 * The path is an absolute one.
	 * If checking of the validity of paths is not enabled,
	 * then we want to keep all the absolute paths.
	 */
	if (!checkInvalidFlag && !removeInvalidFlag)
		return STATUS_KEEP;

	/*
	 * The absolute path has to be checked for validity.
	 * Make sure the path is accessible, and give an error message
	 * if required.
	 */
	if (stat(path, &statbuf) < 0)
	{
		if (removeInvalidFlag)
			return STATUS_REMOVE;

		fprintf(stderr, "Path \"%s\": %s\n", path, strerror(errno));

		return STATUS_ERROR;
	}

	/*
	 * If the allow files flag is not set then make sure the path is
	 * a directory.  If not, then give an error message if required.
	 */
	if((!allowFilesFlag) && (!S_ISDIR(statbuf.st_mode)))
	{
		if (removeInvalidFlag)
			return STATUS_REMOVE;

		fprintf(stderr, "Path \"%s\": Not a directory\n", path);

		return STATUS_ERROR;
	}

	/*
	 * The absolute path is valid and should be kept.
	 */
	return STATUS_KEEP;
}


/*
 * Check the list of paths for validity.  Depending on the options set,
 * remove invalid paths from the list or generate error messages for them.
 * Returns TRUE if there were no errors generated.
 */
static BOOL
CheckPathList(void)
{
	const char *	srcPath;
	int		srcOffset;
	int		destOffset;
	STATUS		status;
	BOOL		successFlag;

	successFlag = TRUE;
	destOffset = 0;

	for (srcOffset = 0; srcOffset < pathCount; srcOffset++)
	{
		/*
		 * Get the next path and check it for validity.
		 */
		srcPath = pathTable[srcOffset];

		status = CheckPath(srcPath);

		/*
		 * Act on the result of checking the path.
		 * This is either to keep the path, silently remove the path
		 * from the list, or set an error because of the path.
		 */
		switch (status)
		{
			case STATUS_KEEP:
				pathTable[destOffset++] = srcPath;
				break;

			case STATUS_REMOVE:
				break;

			case STATUS_ERROR:
				successFlag = FALSE;
				break;

			default:
				fprintf(stderr, "Unknown check status %d\n",
					status);

				return FALSE;
		}
	}

	/*
	 * Update the number of paths remaining in the list.
	 */
	pathCount = destOffset;

	/*
	 * Return whether we failed or not.
	 */
	return successFlag;
}


/*
 * Function called by qsort to compare two entries of the path table.
 * Returns -1, 0, or 1 according to whether the first argument is less than,
 * equal to, or greater than the second argument.
 */
static int
SortCallback(const void * addr1, const void * addr2)
{
	const char **	ptr1;
	const char **	ptr2;

	ptr1 = (const char **) addr1;
	ptr2 = (const char **) addr2;

	return strcmp(*ptr1, *ptr2);
}


/*
 * Copy a NULL-terminated string into our own allocated memory.
 * This exits on an malloc failure.
 */
static char *
CopyString(const char * oldStr)
{
	int	len;
	char *	str;

	len = strlen(oldStr) + 1;

	str = malloc(len);

	if (str == NULL)
	{
		fprintf(stderr, "Cannot allocate %d bytes\n", len);

		exit(1);
	}

	memcpy(str, oldStr, len);

	return str;
}


static void
Usage(void)
{
	const OPTION *	option;

	fprintf(stderr, "\n");
	fprintf(stderr, "Program to manipulate PATH-like environment variables (version %s).\n\n",
		VERSION);

	fprintf(stderr, "Usage: path [-var name] [-option [path ...]] ...\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");

	for (option = optionTable; option->name; option++)
		fprintf(stderr, " %-4s: %s\n", option->name, option->help);

	fprintf(stderr, "\n");
}
