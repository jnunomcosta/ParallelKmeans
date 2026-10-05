#ifndef KMEANS_CLI_COMMANDS_H
#define KMEANS_CLI_COMMANDS_H

/* argv[0] is the subcommand name. Return values are process exit codes. */
int cmd_gen(int argc, char **argv);
int cmd_run(int argc, char **argv);
int cmd_bench(int argc, char **argv);

#endif
