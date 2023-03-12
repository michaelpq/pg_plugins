/*
 * Small-sh example program to test threads with libpq, mixing up
 * randomly SSL and non-SSL connections in a set.
 *
 * To compile:
 * gcc -o pthread_libpq pthread_libpq.c -lpthread -lpq
 */

#include <pthread.h>
#include <stdio.h>

#include "libpq-fe.h"

#define NUM_THREADS 100
#define NUM_LOOPS 100

/* this function is run by the second thread */
void *
conn_thread_func(void *num_ptr)
{
	/* increment x to 100 */
	int			num_thread;
	const char *conninfo_nossl = "host=localhost sslmode=disable";
	const char *conninfo_ssl = "host=localhost sslmode=require";
	const char *conninfo;
	PGconn	   *conn;
	int			count;

	num_thread = *((int *) num_ptr);

	/* Choose SSL or not SSL.  This is let random, on purpose :) */
	if (num_thread < NUM_THREADS / 2)
		conninfo = conninfo_nossl;
	else
		conninfo = conninfo_ssl;

	for (count = 0; count < NUM_LOOPS; count++)
	{
		conn = PQconnectdb(conninfo);

		if (PQstatus(conn) != CONNECTION_OK)
		{
			fprintf(stderr, "connection on loop %d failed: %s",
					count, PQerrorMessage(conn));
			return NULL;
		}

		/* close the connection */
		PQfinish(conn);
	}

	/* the function must return something - NULL will do */
	return NULL;
}

int
main()
{
	int			count = 0;

	/* this variable is the reference to the other 100 threads */
	pthread_t	conn_thread[NUM_THREADS];

	if (PQisthreadsafe() == 0)
	{
		fprintf(stderr, "libpq is not thread-safe\n");
		return 1;
	}
	else
		fprintf(stderr, "libpq is thread safe\n");

	/* create a second thread which executes inc_x(&x) */
	for (count = 0; count < NUM_THREADS; count++)
	{
		if (pthread_create(&conn_thread[count], NULL, conn_thread_func, &count))
		{
			fprintf(stderr, "Error creating thread\n");
			return 1;
		}
	}

	/* wait for the other threads to finish, in sync fashion */
	for (count = 0; count < NUM_THREADS; count++)
	{
		if (pthread_join(conn_thread[count], NULL))
		{
			fprintf(stderr, "Error joining thread\n");
			return 1;
		}
	}

	/* all done */
	return 0;
}
