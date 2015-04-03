/*
 * author: Jan Wrona
 * email: <xwrona00@stud.fit.vutbr.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <mpi.h>

#include "queue.h"

#define FILE_NAME "numbers"
#define TAG 0
#define ROOT_PROC 0
#define IN_BUFF_SIZE 8192

#ifdef NPRINT_IN
#define IN_PRINT(...) do { ; } while (0)
#else
#define IN_PRINT(...) do { printf(__VA_ARGS__); } while (0)
#endif
#ifdef NPRINT_OUT
#define OUT_PRINT(...) do { ; } while (0)
#else
#define OUT_PRINT(...) do { printf(__VA_ARGS__); } while (0)
#endif

void receive_and_store(const int proc_rank, queue_t *ques[2], const int seq_size, unsigned *received_cntr)
{
    static unsigned store_que_index = 0;
    unsigned char recv_byte;

    /* Receive and store data from superior process. */
    MPI_Recv(&recv_byte, 1, MPI_CHAR, proc_rank - 1, TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    queue_enqueue(ques[store_que_index], recv_byte);
    (*received_cntr)++;

    /* Received whole sequence? Switch ques. */
    if (!(*received_cntr % seq_size)) {
	store_que_index = !store_que_index;
    }
}

void merge_and_send(const int proc_rank, queue_t *ques[2], const int seq_size, const int num_procs)
{
    static unsigned que_popped[2] = { 0 };
    unsigned char to_send;
    unsigned send_que_index;

    /* One que allready empty for this sequece? Use element from the second one. */
    if (que_popped[0] == seq_size) {
	send_que_index = 1;
    } else if (que_popped[1] == seq_size) {
	send_que_index = 0;

    /* Both ques available? Compare front elements. */
    /* (Possible to switch < to > for reverse order.) */
    } else if(queue_front(ques[0]) <= queue_front(ques[1])) {
	send_que_index = 0;
    } else {
	send_que_index = 1;
    }

    /* Store and remove element from choosen que. */
    to_send = queue_dequeue(ques[send_que_index]);
    que_popped[send_que_index]++;

    /* Both ques are fully popped for this sequence? Clear counters. */
    if (que_popped[0] + que_popped[1] == 2 * seq_size) {
	que_popped[0] = que_popped[1] = 0;
    }

    /* Last processor doesn't send but prints sorted sequence. */
    if (proc_rank < num_procs - 1) {
	MPI_Send(&to_send, 1, MPI_CHAR, proc_rank + 1, TAG, MPI_COMM_WORLD);
    } else {
	OUT_PRINT("%hhu\n", to_send);
    }
}

int main(int argc, char *argv[])
{
    int num_procs, proc_rank;
    unsigned input_size;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &proc_rank);

    input_size = 1 << (num_procs - 1);

#ifdef MEASURE_TIME
    double wall_time, to_reduce_time, reduced_time;
    clock_t cpu_time;

    MPI_Barrier(MPI_COMM_WORLD);
#endif /* MEASURE_TIME */

    /* 
     * First processor reads input and sends it to the second processor.
     * Every other processor receives and stores data, merges and sends
     * merged sequence to the succeding processor. The last processor
     * doesn't send anything but prints sorted sequence.
     */
    if (proc_rank == ROOT_PROC) {
	queue_t *in_que;
	FILE *in;
	unsigned char buff[IN_BUFF_SIZE];
	size_t bytes_read;
	unsigned first = 1;
	
	in_que = queue_init(input_size);
	if (in_que == NULL) {
	    perror("queue_init()");
	    MPI_Abort(MPI_COMM_WORLD, errno);
	}

	/* Open file and check for errors. */
	in = fopen(FILE_NAME, "r");
	if (in == NULL) {
	    perror(FILE_NAME);
	    MPI_Abort(MPI_COMM_WORLD, errno);
	}

	/* Read bytes, print them and store in queue. */
	do {
	    bytes_read = fread(buff, sizeof(*buff), IN_BUFF_SIZE, in);

	    /* Check errors. */
	    if (ferror(in)) {
		perror("fread()");
		MPI_Abort(MPI_COMM_WORLD, errno);
	    }

	    for (size_t i = 0; i < bytes_read; ++i) {
		if (first) {
		    first = 0;
		} else {
		    IN_PRINT(" ");
		}
		IN_PRINT("%hhu", buff[i]);
		queue_enqueue(in_que, buff[i]);
	    }
	} while (!feof(in));
	IN_PRINT("\n");

#ifdef MEASURE_TIME
	wall_time = MPI_Wtime();
	cpu_time = clock();
#endif
	/* Send each number from the queue to the first processor. */
	while (!queue_empty(in_que)) {
	    unsigned char to_send = queue_dequeue(in_que);

	    MPI_Send(&to_send, 1, MPI_CHAR, proc_rank + 1, TAG, MPI_COMM_WORLD);
	}
	queue_destroy(in_que);
    } else {
	const unsigned seq_size = 1 << (proc_rank - 1);
	unsigned received_cntr = 0;

	queue_t *ques[2] = { 0 };

	ques[0] = queue_init(seq_size + 1);
	ques[1] = queue_init(seq_size);
	if (ques[0] == NULL || ques[1] == NULL) {
	    perror("queue_init()");
	    queue_destroy(ques[0]);
	    queue_destroy(ques[1]);
	    MPI_Abort(MPI_COMM_WORLD, errno);
	}

#ifdef MEASURE_TIME
	cpu_time = clock();
#endif
	/* Loop until all data processed, AKA until at least one queue is not empty. */
	do {
	    /* Receive and store until got all data. */
	    if (received_cntr < input_size) {
		receive_and_store(proc_rank, ques, seq_size, &received_cntr);
	    }

	    /* Merge and send until all data processed. */
	    if (received_cntr > seq_size) {
		merge_and_send(proc_rank, ques, seq_size, num_procs);
	    }
	} while (!(queue_empty(ques[0]) && queue_empty(ques[1])));

	queue_destroy(ques[1]);
	queue_destroy(ques[0]);
    }

#ifdef MEASURE_TIME
    cpu_time = clock() - cpu_time;
    MPI_Barrier(MPI_COMM_WORLD);
    if (proc_rank == ROOT_PROC) {
	wall_time = MPI_Wtime() - wall_time;
    }
    to_reduce_time = ((double)cpu_time) / CLOCKS_PER_SEC;
    MPI_Reduce(&to_reduce_time, &reduced_time, 1, MPI_DOUBLE, MPI_SUM, ROOT_PROC, MPI_COMM_WORLD);

    if (proc_rank == ROOT_PROC) {
	printf("walltime: %f\nreduced: %f\n", wall_time, reduced_time);
    }
#endif //MEASURE_TIME

    MPI_Finalize();
    return EXIT_SUCCESS;
}
