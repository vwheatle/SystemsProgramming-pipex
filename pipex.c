#include <stdlib.h>  // -> EXIT_*, *_FILENO
#include <stdio.h>   // -> printf, exit
#include <stdbool.h> // -> bool

#include <errno.h>  // -> errno
#include <string.h> // -> strerror

#include <unistd.h> // -> fork, perror, exec
#include <fcntl.h>  // -> pid_t

#define oops(m, x) \
	{ \
		perror(m); \
		exit(x); \
	}

#define forked_oops(m, x) \
	{ \
		char *errmsg = strerror(errno); \
		fprintf(stderr, "[%d] %s: %s\n", getpid(), m, errmsg); \
		exit(x); \
	}

// utility function that turns one process into n different processes.
int become_n_processes(int n) {
	// // well, you asked for it!!
	// if (n < 1) exit(EXIT_SUCCESS);

	int i = 0;
	for (; i < n - 1; i++)
		if (fork() > 0) break;
	return i;
}

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

// i: index into pipe_fds array
// e: pipe end, chosen from above constants.
//    (or 0, if you just want the multiplied index)
#define pipeline_index(i, e) ((i)*2 + (e))
// (i like giving expressions names, for clarity)

typedef struct {
	int pipe_count;
	int proc_count;
	int *pipe_fds;
	char **arg_values;
} pipeline_info;

pipeline_info pipeline_create(int proc_count, char *arg_values[]) {
	pipeline_info info;

	info.pipe_count = proc_count - 1;
	info.proc_count = proc_count;
	info.pipe_fds = malloc(info.pipe_count * sizeof(int[2]));
	info.arg_values = arg_values;

	// makes all the pipes before doing any forking.
	for (int i = 0; i < info.pipe_count; i++) {
		int *destination_slice = &(info.pipe_fds[pipeline_index(i, 0)]);
		if (pipe(destination_slice) == -1) oops("couldn't get pipe", 1);
	}

	return info;
}

void pipeline_set_io(pipeline_info *info, int proc_index) {
	// please note that i have offset the argument list by one argument
	// when copying its pointer into the pipeline info struct.

	// also some of this stuff is counterintuitive and hard to put into words,
	// but if you look at the attached `pipex.png` diagram, it makes sense.
	// kinda.

	// the read end of the first pipe is used by the second program.
	// the first program (argv[1]) does not need any read end,
	// as it is connected to stdin.
	bool uses_read_end = proc_index > 0;
	if (uses_read_end) {
		int read_end_fd =
			info->pipe_fds[pipeline_index(proc_index - 1, PIPE_READ_END)];

		// redirect standard input so that it reads from the pipe.
		// (this closes the "real" stdin
		//  and replaces it with the pipe's read end.)
		if (dup2(read_end_fd, STDIN_FILENO) == -1)
			forked_oops("could not redirect stdin", EXIT_FAILURE);
	}

	// the write end of the last pipe is used by the second-to-last program.
	// the last program (argv[argc - 1]) does not need any write end,
	// as it is connected to stdout.
	bool uses_write_end = proc_index < (info->proc_count - 1);
	if (uses_write_end) {
		int write_end_fd =
			info->pipe_fds[pipeline_index(proc_index, PIPE_WRITE_END)];

		// redirect standard output so that it writes into the pipe.
		// (this closes the "real" stdout
		//  and replaces it with the pipe's write end.)
		if (dup2(write_end_fd, STDOUT_FILENO) == -1)
			forked_oops("could not redirect stdout", EXIT_FAILURE);
	}

	// finally, clean up all the pipe file descriptors
	// that you don't want `execlp` knowing about.
	for (int i = 0; i < info->pipe_count; i++) {
		close(info->pipe_fds[pipeline_index(i, PIPE_READ_END)]);
		close(info->pipe_fds[pipeline_index(i, PIPE_WRITE_END)]);
	}
}

void pipeline_run(pipeline_info *info, int proc_index) {
	// Transform into another program.
	char *program_name = info->arg_values[proc_index];
	execlp(program_name, program_name, NULL /* end of argv list */);
	// -> won't return from this function call.

	// ...but if it did...
	forked_oops(program_name, EXIT_FAILURE);

	// it's fine to print error messages even after reconfiguring
	// everything, since perror messages will be printed to stderr.
	// i didn't think stderr had a use until now! i've taken the liberty of
	// making a variant of oops (forked_oops) that prints the PID alongside the
	// specified error message, since we're dealing with multiple processes.
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "usage: pipe cmd1 cmd2 [cmd3 ... cmdN]\n");
		exit(EXIT_FAILURE);
	}

	pipeline_info info = pipeline_create(argc - 1, &argv[1]);

	// fork into several processes, the quantity needed to run the pipeline.
	int proc_index = become_n_processes(info.proc_count);
	// beyond this point, you're several processes. not just one.

	pipeline_set_io(&info, proc_index);
	pipeline_run(&info, proc_index);
	// -> won't return from this function call.
}
