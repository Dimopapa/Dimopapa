#include "pagerank.h"
#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>

#define maxthreads 128

/************ Global variables ************/
int N;                                                              // number of nodes
int num_threads;										     		// number of threads
int iterations = 0;													// number of iterations
double max_error = 1, sum = 0;
double conv, d;														// convergence threshold / parameter d of algorithm

pthread_t *threads;													// table of threads
Thread *tdata;														// table with thread's data
Node *Nodes;														// table of node's data
pthread_mutex_t locksum = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lockmax = PTHREAD_MUTEX_INITIALIZER;


/************** Functions **************/

void Allocate_threads()																	// initialize threads
{
	int i;
	double load =  (double) N / num_threads;

	threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));	                	// allocate memory for threads
	tdata = (Thread*)malloc(num_threads * sizeof(Thread));				            	// stores thread's data

	tdata[0].tid = 0;																	// split data into loads, given to each thread
	tdata[0].start = 0;
	tdata[0].end = floor(load);

	for (i = 1; i < num_threads; i++)
	{
		tdata[i].tid = i;
		tdata[i].start = tdata[i - 1].end;
		if (i < (num_threads - 1))
		{
			tdata[i].end = tdata[i].start + floor(load);
		}
		else
		{
			tdata[i].end = N;
		}
	}

	for (i = 0; i < num_threads; i++)
	{
		printf("\nThread %d, starts at %d and ends at node %d", tdata[i].tid, tdata[i].start, tdata[i].end);
	}
}

void Allocate_Nodes()																// initialize nodes
{
	int i;
	Nodes = (Node*)malloc(N * sizeof(Node));

    for (i = 0; i < N; i++)
	{
		Nodes[i].to_size = 0;
		Nodes[i].from_size = 0;
      Nodes[i].from_id = (int*)malloc(sizeof(int));
   }
}

void Insert_Graph(char* filename)											    	// read graph connections from txt file
{
   FILE *f;
   int to_node, from_node;
	int tmp_size;
	char line[1000];

   f = fopen("web-Google.txt", "r");
   if (f == NULL) { printf("\nError opening the file\n"); }
	while (!feof(f))
	{
		fgets(line, sizeof(line), f);
		if (sscanf(line,"%d\t%d\n", &from_node, &to_node))
		{
			Nodes[to_node].from_size++;
			Nodes[from_node].to_size++;
			tmp_size = Nodes[to_node].from_size;
			Nodes[to_node].from_id = (int*) realloc(Nodes[to_node].from_id, tmp_size * sizeof(int));
			Nodes[to_node].from_id[tmp_size - 1] = from_node;
		}
	}
	printf("\nGraph inserted.\n");
	fclose(f);
}

void Random_P_E()
{
   int i;
   double sump1 = 0, sume = 0;;

   for (i = 0; i < N; i++)												        	// initialize arrays
   {
   	Nodes[i].p0 = 0;
      Nodes[i].p1 = 1;
      Nodes[i].p1 = (double) Nodes[i].p1 / N;
      sump1 = sump1 + Nodes[i].p1;
		Nodes[i].e = 1;
      Nodes[i].e = (double) Nodes[i].e / N;
      sume = sume + Nodes[i].e;
   }
   assert(sump1 = 1);													        	// assert sum of probabilities is = 1,exit if sum is !=1
   assert(sume = 1);
}
  
void* InitPoss(void* arg)													     	// re-initialize p0 and p1
{
	int i;
	Thread *tdata = (Thread *)arg;

	for (i = tdata->start; i < tdata->end; i++)
	{
		Nodes[i].p0 = Nodes[i].p1;
		Nodes[i].p1 = 0;
	}
}

void* Max_error(void* arg)												    	// Parallel section, compute local max
{
	Thread *tdata = (Thread *) arg;
	int i, j;
	double tmp_max = -1;														// every thread will find a local max,then check if its a global max

	for (i = tdata->start; i < tdata->end; i++)
	{
		Nodes[i].p1 = d * (Nodes[i].p1 + sum) + (1 - d) * Nodes[i].e;
      if (fabs(Nodes[i].p1 - Nodes[i].p0) > tmp_max)
      {
         tmp_max  = fabs(Nodes[i].p1 - Nodes[i].p0);
      }
	}

	pthread_mutex_lock(&lockmax);											// this is an atomic operation
	if (max_error  < tmp_max)												// check for new global max
	{
		max_error = tmp_max;
	}
	pthread_mutex_unlock(&lockmax);
}

void* PageRank_Pthreads(void* arg)									    	// parallel section
{
	Thread *tdata = (Thread *) arg;
	int i, j, index;
	double tmp_sum = 0;														// each thread will compute a local sum,then add it to the global sum

	for (i = tdata->start; i < tdata->end; i++)
	{
		if (Nodes[i].to_size == 0)
		{
			 tmp_sum = tmp_sum + (double) Nodes[i].p0 / N;
		}
		if (Nodes[i].from_size != 0)
      {
         for (j = 0; j < Nodes[i].from_size; j++)					       // compute total probability, contributed by node's neighbors
         {
				index = Nodes[i].from_id[j];
				Nodes[i].p1 = Nodes[i].p1 + (double) Nodes[index].p0 / Nodes[index].to_size;
			}
      }
	}
	pthread_mutex_lock(&locksum);
	sum = sum + tmp_sum;
	pthread_mutex_unlock(&locksum);
}

void PageRank()
{
   int i, j, index;
   while (max_error > conv)												// continue if we don't have convergence yet
   {
    	max_error = -1;
		sum = 0;
      for (i = 0; i < num_threads; i++)
      {
			pthread_create(&threads[i], NULL, &InitPoss,(void*) &tdata[i]);
		}
		for (i = 0; i < num_threads; i++)								// wait for all threads to "catch" this point
		{
			pthread_join(threads[i], NULL);
      }
      for (i = 0; i < num_threads; i++)							     	// find P for each webpage
      {
         pthread_create(&threads[i], NULL, &PageRank_Pthreads, (void*) &tdata[i]);
      }
		for (i = 0; i < num_threads; i++)
		{
			pthread_join(threads[i], NULL);
		}
		for (i = 0; i < num_threads; i++)								// find local and global max
      {
         pthread_create(&threads[i], NULL, &Max_error, (void*) &tdata[i]);
      }
		for (i = 0; i < num_threads; i++)
		{
			pthread_join(threads[i], NULL);
		}
      printf("Max error of iteration %d is: %f\n", iterations+1, max_error);
      iterations++;
   }
}

/************ Main function ************/

int main(int argc, char** argv)
{
	int i,j,k;
	double totaltime;
	char filename[256];

	if (argc < 5 && argc >= 6)
	{
		printf("Usage : 5 arguments required: filename, number of nodes(N), convergence(conv), d\n and number of threads(num_threads)\n");
		return 0;
	}

	strcpy(filename, argv[1]);												// get arguments
	N = atoi(argv[2]);
	conv = atof(argv[3]);
	d = atof(argv[4]);
	num_threads = atoi(argv[5]);

	if ((num_threads < 1) || (num_threads > maxthreads))
	{
		printf("Usage : number of threads must be >= 1 and  <= %d!\n", maxthreads);
		exit(1);
	}

	Allocate_threads();
	Allocate_Nodes();
   Insert_Graph(filename);
   Random_P_E();
   printf("\nPageRank starts...\n");
   gettimeofday(&start, NULL);
   PageRank();
	gettimeofday(&end, NULL);
   printf("Total iterations: %d\n", iterations);
	//printf("Node with most connections leading to it is : %d", Node[])
   totaltime = (((end.tv_usec - start.tv_usec) / 1.0e6
		+ end.tv_sec - start.tv_sec) * 1000) / 1000;
	printf("\nPageRank ended in totaltime : %f seconds\n", totaltime);
   return 0;
}
