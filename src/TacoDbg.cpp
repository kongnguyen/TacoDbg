#include "TacoDbg.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include "Constant.h"
#include "util.h"

#include "../inttypes.h"

TacoDbg::TacoDbg()
{
	cs_open(CS_ARCH_X86, CS_MODE_32, &_csHandle);
	cs_option(_csHandle, CS_OPT_DETAIL, CS_OPT_ON);
}

TacoDbg::~TacoDbg()
{
	cs_close(&_csHandle);
}

void TacoDbg::attachToProcess(pid_t childPid)
{
	_childPid = childPid;

	procmsg("debugger started\n");

	/* Wait for child to stop on its first instruction */
	wait(0);
	procmsg("child now at EIP = 0x%08x\n", get_child_eip(childPid));
}

void TacoDbg::startCommandLoop()
{
	std::string buf;
	std::string command;

	while(command != CMD_QUIT && command != CMD_S_QUIT)
	{
		log("$ ");
		std::getline(std::cin, buf);
		std::vector<std::string> tokens = split(buf, ' ');

		if(tokens.size() > 0)
		{
			command = tokens.at(0);
			std::vector<std::string> args = std::vector<std::string>(tokens.begin()+1, tokens.end());

			handleCommand(command.c_str(), args);
		}
		else
		{
			//TODO: raise error
		}
	}
}

void TacoDbg::handleCommand(const char* cmd, std::vector<std::string> args)
{
	std::string command = std::string(cmd);

	if(command == CMD_NEXT || command == CMD_S_NEXT)
	{
		stepOver();
	}
	else if(command == CMD_STEP || command == CMD_S_STEP)
	{
		stepInto();
	}
	else if(command == CMD_INFO || command == CMD_S_INFO)
	{
		std::string infoType = args.at(0);
		if(infoType == INFO_REGISTERS)
		{
			struct user_regs_struct regs;
			ptrace(PTRACE_GETREGS, _childPid, 0, &regs);
			log(
					"eax: 0x%x\necx: 0x%x\nedx: 0x%x\nebx: 0x%x\n"
					"esp: 0x%x\nebp: 0x%x\neip: 0x%x\n",
					regs.eax, regs.ecx, regs.edx, regs.ebx,
					regs.esp, regs.ebp, regs.eip);
		}
	}
	else if(command == CMD_DISASSEMBLE)
	{
//		std::string saddress = args.at(0);
//		int addr = 0x0;
//		sscanf(saddress.c_str(), "%d", &addr);
		disassemble(0x0);
	}
}

void TacoDbg::stepOver()
{

}

bool TacoDbg::stepInto()
{
	int waitStatus;
	ptrace(PTRACE_SINGLESTEP, _childPid, 0, 0);
	wait(&waitStatus);

	if(WIFEXITED(waitStatus))
	{
		return false;
	}

	return true;
}

void TacoDbg::disassemble(int addr, int numBytes)
{
	struct user_regs_struct regs;
	ptrace(PTRACE_GETREGS, _childPid, 0, &regs);

#define CODE_SIZE 64
	unsigned char code[CODE_SIZE];

	for(int i = 0; i < CODE_SIZE/4; i++)
	{
		unsigned word =	ptrace(PTRACE_PEEKTEXT, _childPid, regs.eip + 4*i, 0);
		memcpy(code + 4*i, &word, sizeof(word));
	}

	uint64_t address = 0x1000;
	cs_insn *insn;

	int count = cs_disasm(_csHandle, (unsigned char*) code, sizeof(code), address, 0, &insn);

	if(count > 0)
	{
		for (int j = 0; j < count; j++) {
			log("0x%"PRIx64":\t%s\t%s\n", insn[j].address, insn[j].mnemonic, insn[j].op_str);
		}
	}
	else
	{
		//TODO:
		log("ERROR: Can't decode instruction\n");
	}
}
