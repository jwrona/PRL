/*
 * author: Jan Wrona
 * email: <xwrona00@stud.fit.vutbr.cz>
 */

#include <iostream>
#include <fstream>
#include <queue>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <ctime>

#include <mpi.h>

#define FILE_NAME "numbers"
#define TAG 0
#define ROOT_PROC 0

#ifdef NO_OUT
#define PRINT(x) do { ; } while (false)
#else
#define PRINT(x) do { std::cout << x; } while (false)
#endif

void receive_and_store(const int proc_rank, std::queue<unsigned char>ques[2], const int seq_size, unsigned &received_cntr)
{
    static bool store_que_index = 0;
    unsigned char recv_byte;

    /* Receive and store data from superior process. */
    MPI::COMM_WORLD.Recv(&recv_byte, 1, MPI_CHAR, proc_rank - 1, TAG);
    ques[store_que_index].push(recv_byte);
    received_cntr++;

    /* Received whole sequence? Switch ques. */
    if (!(received_cntr % seq_size)) {
	store_que_index = !store_que_index;
    }
}

void merge_and_send(const int proc_rank, std::queue<unsigned char>ques[2], const int seq_size, const int num_procs)
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
    } else if(ques[0].front() <= ques[1].front()) { //possible to switch < to > for reverse order
	send_que_index = 0;
    } else {
	send_que_index = 1;
    }

    /* Store and remove element from choosen que. */
    to_send = ques[send_que_index].front();
    ques[send_que_index].pop();
    que_popped[send_que_index]++;

    /* Both ques are fully popped for this sequence? Clear counters. */
    if (que_popped[0] + que_popped[1] == 2 * seq_size) {
	que_popped[0] = que_popped[1] = 0;
    }

    /* Last processor doesn't send but prints sorted sequence. */
    if (proc_rank < num_procs - 1) {
	MPI::COMM_WORLD.Send(&to_send, 1, MPI_CHAR, proc_rank + 1, TAG);
    } else {
	PRINT(static_cast<unsigned>(to_send) << std::endl);
    }
}

int main(int argc, char *argv[])
{
    MPI::Init(argc, argv);
    const int num_procs = MPI::COMM_WORLD.Get_size();
    const int proc_rank = MPI::COMM_WORLD.Get_rank();
    const unsigned input_size = 1 << (num_procs - 1);

#ifdef MEASURE_TIME
    size_t reduced_mem, ques_max_size = 0;
    double wall_time, to_reduce_time, reduced_time;
    clock_t cpu_time;

    MPI::COMM_WORLD.Barrier();
#endif //MEASURE_TIME

    /* 
     * First processor reads input and sends it to the second processor.
     * Every other processor receives and stores data, merges and sends
     * merged sequence to the succeding processor. The last processor
     * doesn't send anything but prints sorted sequence.
     */
    if (proc_rank == ROOT_PROC) {
	std::queue<unsigned char> in_que;

	/* Open file and check for errors. */
	std::ifstream is(FILE_NAME, std::ifstream::in | std::ifstream::binary);
	if (is) {
	    unsigned char read_byte;
	    bool first = true;

	    /* Read byte after byte, print it and store it in queue. */
	    while (is.read(reinterpret_cast<char*>(&read_byte), 1)) {
		if (first) {
		    first = false;
		} else {
		    PRINT(' ');
		}
		PRINT(static_cast<unsigned>(read_byte));
		in_que.push(read_byte);
	    }
	    PRINT(std::endl);

	    /* Check for errors during file reading. */
	    if (!is.eof()) {
		std::cerr << strerror(errno) << " \"" FILE_NAME "\"" << std::endl;
		MPI::COMM_WORLD.Abort(errno);
	    }
	    is.close();
	} else {
	    std::cerr << strerror(errno) << " \"" FILE_NAME "\"" << std::endl;
	    MPI::COMM_WORLD.Abort(errno);
	}

#ifdef MEASURE_TIME
	wall_time = MPI::Wtime();
	cpu_time = clock();
#endif
	/* Send each number from the queue to the first processor. */
	while (!in_que.empty()) {
	    unsigned to_send = in_que.front();

	    in_que.pop();
	    MPI::COMM_WORLD.Send(&to_send, 1, MPI_CHAR, proc_rank + 1, TAG);
	}
    } else {
	const unsigned seq_size = 1 << (proc_rank - 1);
	unsigned received_cntr = 0;

	std::queue<unsigned char> ques[2];

#ifdef MEASURE_TIME
	cpu_time = clock();
#endif
	/* Loop until all data processed, AKA until at least one queue is not empty. */
	do {
	    /* Receive and store until got all data. */
	    if (received_cntr < input_size) {
		receive_and_store(proc_rank, ques, seq_size, received_cntr);
	    }

	    ques_max_size = std::max(ques_max_size, ques[0].size() + ques[1].size());

	    /* Merge and send until all data processed. */
	    if (received_cntr > seq_size) {
		merge_and_send(proc_rank, ques, seq_size, num_procs);
	    }
	} while (!(ques[0].empty() && ques[1].empty()));
    }

#ifdef MEASURE_TIME
    cpu_time = clock() - cpu_time;
    MPI::COMM_WORLD.Barrier();
    if (proc_rank == ROOT_PROC) {
	wall_time = MPI::Wtime() - wall_time;
    }
    to_reduce_time = static_cast<double>(cpu_time) / CLOCKS_PER_SEC;
    MPI::COMM_WORLD.Reduce(&to_reduce_time, &reduced_time, 1, MPI_DOUBLE, MPI_SUM, ROOT_PROC);
    MPI::COMM_WORLD.Reduce(&ques_max_size, &reduced_mem, 1, MPI_UNSIGNED_LONG, MPI_SUM, ROOT_PROC);

    if (proc_rank == ROOT_PROC) {
	std::cout << "walltime: " << std::fixed << wall_time << std::endl;
	std::cout << "reduced: " << std::fixed << reduced_time << std::endl;
	std::cout << "mem: " << std::fixed << reduced_mem + input_size << std::endl;
    }
#endif //MEASURE_TIME

    MPI::Finalize();
    return EXIT_SUCCESS;
}
