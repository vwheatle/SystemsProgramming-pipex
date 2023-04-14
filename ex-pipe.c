/* pipe.c
 * * Demonstrates how to create a pipeline from one process to another
 * * Takes two args, each a command, and connects
 *   argv[1]'s output to input of argv[2]
 * * usage: pipe command1 command2
 *   effect: command1 | command2
 * * Limitations: commands do not take arguments
 * * uses execlp() since known number of args
 * * Note: exchange child and parent and watch fun
 */

#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>

#define oops(m, x) \
	{ \
		perror(m); \
		exit(x); \
	}

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

int main(int argc, char *argv[]) {
	if (argc != 3) {
		// the aim of the "pipex" project is
		// to accept more than two commands,
		// and pipe them all together in sequence.
		//
		// But this is just the pipe example program.
		fprintf(stderr, "usage: pipe cmd1 cmd2\n");
		exit(EXIT_FAILURE);
	}

	// pipes have two ends.
	// anything written to the write end will show up in the read end.
	// [0]: read end; [1]: write end
	int pipe_fd[2];
	if (pipe(pipe_fd) == -1) oops("couldn't get pipe", 1);

	// now we have a pipe. let's get two processes!
	pid_t pid = fork();
	if (pid == -1) oops("couldn't fork", 2);

	// Fork call makes a new child process, and...
	// ...gives the parent the PID of the new child,
	// ...gives the child the integer value "0".
	if (pid > 0) {
		// Parent's code
		// (The right side of the pipe)

		// Both parent and child will have the same pipe "file" handles open,
		// even when one of them has no use for them. This means we probably
		// should close the unused one in each respective process.
		// (This only is done to the two ends of the pipeline.)

		close(pipe_fd[PIPE_WRITE_END]);
		// The rightmost side of the pipeline doesn't write into a pipe,
		// it writes into the terminal's regular everyday stdout!
		// The write end of the pipe should *not* be visible to the
		// parent's program when we `execlp` it.

		// redirect standard input so that it reads from the pipe.
		// (this closes the "real" stdin
		//  and replaces it with the pipe's read end.)
		if (dup2(pipe_fd[PIPE_READ_END], STDIN_FILENO) == -1)
			oops("could not redirect stdin", 3);

		close(pipe_fd[PIPE_READ_END]);
		// the pipe's read end has been set up as the source of stdin's input,
		// so we don't need the file descriptor that points to the pipe's
		// read end. this doesn't destroy anything about the pipe, it's
		// like leaking memory -- the pointer is gone but the entity remains.

		// Cool, now we have a process whose stdout is still connected to the
		// terminal, and whose stdin is taken from the "write" end of the pipe!

		// Transform into another program,
		// one that will read from standard input.
		execlp(argv[2], argv[2], NULL /* end of argv list */);
		// -> won't return from this function call.

		oops(argv[2], 4); // If you're still here, something went wrong.
		// it's fine to print error messages even after reconfiguring
		// everything, since perror messages will be printed to stderr.
		// i didn't think stderr has a use until now! i guess the only
		// pain point is that now you don't know which program in the pipeline
		// printed to stderr...
	} else {
		// Child's code
		// (The left side of the pipe)

		close(pipe_fd[PIPE_READ_END]);
		// The leftmost side of the pipeline doesn't read from a pipe,
		// it reads from the terminal's regular everyday stdin!
		// The read end of the pipe should *not* be visible to the
		// child's program when we `execlp` it.

		// redirect standard output so that it writes into the pipe.
		// (this closes the "real" stdout
		//  and replaces it with the pipe's write end.)
		if (dup2(pipe_fd[PIPE_WRITE_END], STDOUT_FILENO) == -1)
			oops("could not redirect stdout", 4);

		close(pipe_fd[PIPE_WRITE_END]);
		// the pipe's write end is now set as the destination of stdout output,
		// so we don't need the file descriptor that points to the pipe's
		// write end. this doesn't destroy anything about the pipe, it's
		// like leaking memory -- the pointer is gone but the entity remains.

		// Cool, now we have a process whose stdin is still read from the
		// terminal, and whose stdout is taken from the "read" end of the pipe!

		// Transform into another program,
		// one that will write to standard output.
		execlp(argv[1], argv[1], NULL /* end of argv list */);
		// -> won't return from this function call.
		oops(argv[1], 5); // If you're still here, something went wrong.
		// it's fine to print error messages even after reconfiguring
		// everything, since perror messages will be printed to stderr.
		// i didn't think stderr has a use until now! i guess the only
		// pain point is that now you don't know which program in the pipeline
		// printed to stderr...
	}
}
