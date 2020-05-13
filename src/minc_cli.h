#ifndef __MINC_CLI_H
#define __MINC_CLI_H

struct ExitException
{
	const int code;
	ExitException(int code) : code(code) {}
};

void getCommandLineArgs(int* argc, char*** argv);
void setCommandLineArgs(int argc, char** argv);
void quit(int code);

#endif