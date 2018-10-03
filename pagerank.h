/***** Struct for timestamps *****/
struct timeval start,end;

/***** Struct used for Threads data *****/

typedef struct
{
	int tid;
	int start, end;
} Thread;

/***** Struct used for Nodes data *****/

typedef struct
{
	double p0;
	double p1;
	double e;
	int *from_id;   // creates a table that contains ids of nodes pointing to a page
	int to_size;   // size of connections
	int from_size;  // size of connections from other nodes
}Node;
