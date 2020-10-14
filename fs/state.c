#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "state.h"
#include "../tecnicofs-api-constants.h"

#include <pthread.h>

inode_t inode_table[INODE_TABLE_SIZE];

pthread_mutex_t fsMutex;
pthread_rwlock_t fsRWLock;

/*
 * Sleeps for synchronization testing.
 */
void insert_delay(int cycles)
{
	for (int i = 0; i < cycles; i++)
	{
	}
}

/*
 * Function for responsible for locking the fs structure, supports
 * mutex, rwlock and nosync
 */
void fsLock(char *syncstrat, char type)
{
	switch (syncstrat[0])
	{
	case 'm': /* mutex */
		if (pthread_mutex_lock(&fsMutex) != 0)
			exit(EXIT_FAILURE);
		break;
	case 'r': /* rwlock */
		switch (type)
		{
		case 'r':
			if (pthread_rwlock_rdlock(&fsRWLock) != 0)
				exit(EXIT_FAILURE);
			break;
		case 'w':
			if (pthread_rwlock_wrlock(&fsRWLock) != 0)
				exit(EXIT_FAILURE);
			break;
		default:
			break;
		}
		break;
	case 'n':
		break;
	default:
		break;
	}
}

/*
 * Function for responsible for unlocking the fs structure, supports
 * mutex, rwlock and nosync
 */
void fsUnlock(char *syncstrat)
{
	switch (syncstrat[0])
	{
	case 'm':
		if (pthread_mutex_unlock(&fsMutex) != 0)
			exit(EXIT_FAILURE);
		break;
	case 'r':
		if (pthread_rwlock_unlock(&fsRWLock) != 0)
			exit(EXIT_FAILURE);
		break;
	case 'n':
		break;
	default:
		break;
	}
}

/*
 * Initializes the i-nodes table.
 */
void inode_table_init()
{
	pthread_mutex_init(&fsMutex, NULL);
	pthread_rwlock_init(&fsRWLock, NULL);

	for (int i = 0; i < INODE_TABLE_SIZE; i++)
	{
		inode_table[i].nodeType = T_NONE;
		inode_table[i].data.dirEntries = NULL;
		inode_table[i].data.fileContents = NULL;
	}
}

/*
 * Releases the allocated memory for the i-nodes tables.
 */
void inode_table_destroy()
{
	for (int i = 0; i < INODE_TABLE_SIZE; i++)
	{
		if (inode_table[i].nodeType != T_NONE)
		{
			/* as data is an union, the same pointer is used for both dirEntries and fileContents */
			/* just release one of them */
			if (inode_table[i].data.dirEntries)
				free(inode_table[i].data.dirEntries);
		}
	}
}

/*
 * Creates a new i-node in the table with the given information.
 * Input:
 *  - nType: the type of the node (file or directory)
 * Returns:
 *  inumber: identifier of the new i-node, if successfully created
 *     FAIL: if an error occurs
 */
int inode_create(type nType, char *syncstrat)
{
	/* Used for testing synchronization speedup */
	insert_delay(DELAY);

	/*Critical Zone Beggining*/
	fsLock(syncstrat, 'w');
	for (int inumber = 0; inumber < INODE_TABLE_SIZE; inumber++)
	{
		if (inode_table[inumber].nodeType == T_NONE)
		{
			inode_table[inumber].nodeType = nType;

			if (nType == T_DIRECTORY)
			{
				/* Initializes entry table */
				inode_table[inumber].data.dirEntries = malloc(sizeof(DirEntry) * MAX_DIR_ENTRIES);

				for (int i = 0; i < MAX_DIR_ENTRIES; i++)
				{
					inode_table[inumber].data.dirEntries[i].inumber = FREE_INODE;
				}
			}
			else
			{
				inode_table[inumber].data.fileContents = NULL;
			}
			fsUnlock(syncstrat);
			return inumber;
		}
	}
	fsUnlock(syncstrat);
	return FAIL;
}

/*
 * Deletes the i-node.
 * Input:
 *  - inumber: identifier of the i-node
 * Returns: SUCCESS or FAIL
 */
int inode_delete(int inumber, char *syncstrat)
{
	/* Used for testing synchronization speedup */
	insert_delay(DELAY);

	/*Critical Zone Beggining*/
	fsLock(syncstrat, 'w');
	if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE))
	{
		fsUnlock(syncstrat);
		printf("inode_delete: invalid inumber\n");
		return FAIL;
	}

	inode_table[inumber].nodeType = T_NONE;
	/* see inode_table_destroy function */
	if (inode_table[inumber].data.dirEntries)
		free(inode_table[inumber].data.dirEntries);
	fsUnlock(syncstrat);
	return SUCCESS;
}

/*
 * Copies the contents of the i-node into the arguments.
 * Only the fields referenced by non-null arguments are copied.
 * Input:
 *  - inumber: identifier of the i-node
 *  - nType: pointer to type
 *  - data: pointer to data
 * Returns: SUCCESS or FAIL
 */
int inode_get(int inumber, type *nType, union Data *data, char *syncstrat)
{
	/* Used for testing synchronization speedup */
	insert_delay(DELAY);

	/*Critical Zone Beggining*/
	fsLock(syncstrat, 'r');
	if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE))
	{
		printf("inode_get: invalid inumber %d\n", inumber);
		fsUnlock(syncstrat);
		return FAIL;
	}

	if (nType)
		*nType = inode_table[inumber].nodeType;

	if (data)
		*data = inode_table[inumber].data;

	fsUnlock(syncstrat);
	return SUCCESS;
}

/*
 * Resets an entry for a directory.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 * Returns: SUCCESS or FAIL
 */
int dir_reset_entry(int inumber, int sub_inumber, char *syncstrat)
{
	/* Used for testing synchronization speedup */
	insert_delay(DELAY);

	/*Critical Zone Beggining*/
	fsLock(syncstrat, 'w');
	if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE))
	{
		fsUnlock(syncstrat);
		printf("inode_reset_entry: invalid inumber\n");
		return FAIL;
	}

	if (inode_table[inumber].nodeType != T_DIRECTORY)
	{
		fsUnlock(syncstrat);
		printf("inode_reset_entry: can only reset entry to directories\n");
		return FAIL;
	}

	if ((sub_inumber < FREE_INODE) || (sub_inumber > INODE_TABLE_SIZE) ||
		(inode_table[sub_inumber].nodeType == T_NONE))
	{
		fsUnlock(syncstrat);
		printf("inode_reset_entry: invalid entry inumber\n");
		return FAIL;
	}

	for (int i = 0; i < MAX_DIR_ENTRIES; i++)
	{
		if (inode_table[inumber].data.dirEntries[i].inumber == sub_inumber)
		{
			inode_table[inumber].data.dirEntries[i].inumber = FREE_INODE;
			inode_table[inumber].data.dirEntries[i].name[0] = '\0';
			fsUnlock(syncstrat);
			return SUCCESS;
		}
	}
	fsUnlock(syncstrat);
	return FAIL;
}

/*
 * Adds an entry to the i-node directory data.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 *  - sub_name: name of the sub i-node entry 
 * Returns: SUCCESS or FAIL
 */
int dir_add_entry(int inumber, int sub_inumber, char *sub_name, char *syncstrat)
{
	/* Used for testing synchronization speedup */
	insert_delay(DELAY);

	/*Critical Zone Beggining*/
	fsLock(syncstrat, 'w');
	if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE))
	{
		fsUnlock(syncstrat);
		printf("inode_add_entry: invalid inumber\n");
		return FAIL;
	}

	if (inode_table[inumber].nodeType != T_DIRECTORY)
	{

		fsUnlock(syncstrat);
		printf("inode_add_entry: can only add entry to directories\n");
		return FAIL;
	}

	if ((sub_inumber < 0) || (sub_inumber > INODE_TABLE_SIZE) || (inode_table[sub_inumber].nodeType == T_NONE))
	{
		fsUnlock(syncstrat);
		printf("inode_add_entry: invalid entry inumber\n");
		return FAIL;
	}

	if (strlen(sub_name) == 0)
	{
		fsUnlock(syncstrat);
		printf("inode_add_entry: \
               entry name must be non-empty\n");
		return FAIL;
	}

	for (int i = 0; i < MAX_DIR_ENTRIES; i++)
	{
		if (inode_table[inumber].data.dirEntries[i].inumber == FREE_INODE)
		{
			inode_table[inumber].data.dirEntries[i].inumber = sub_inumber;
			strcpy(inode_table[inumber].data.dirEntries[i].name, sub_name);
			fsUnlock(syncstrat);
			return SUCCESS;
		}
	}
	fsUnlock(syncstrat);
	return FAIL;
}

/*
 * Prints the i-nodes table.
 * Input:
 *  - inumber: identifier of the i-node
 *  - name: pointer to the name of current file/dir
 */
void inode_print_tree(FILE *fp, int inumber, char *name)
{
	if (inode_table[inumber].nodeType == T_FILE)
	{
		fprintf(fp, "%s\n", name);
		return;
	}

	if (inode_table[inumber].nodeType == T_DIRECTORY)
	{
		fprintf(fp, "%s\n", name);
		for (int i = 0; i < MAX_DIR_ENTRIES; i++)
		{
			if (inode_table[inumber].data.dirEntries[i].inumber != FREE_INODE)
			{
				char path[MAX_FILE_NAME];
				if (snprintf(path, sizeof(path), "%s/%s", name, inode_table[inumber].data.dirEntries[i].name) >
					sizeof(path))
				{
					fprintf(stderr, "truncation when building full path\n");
				}
				inode_print_tree(fp, inode_table[inumber].data.dirEntries[i].inumber, path);
			}
		}
	}
}
