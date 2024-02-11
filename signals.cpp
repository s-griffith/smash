#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

#define SYS_FAIL -1

using namespace std;

void ctrlCHandler(int sig_num) {
   SmallShell& smash = SmallShell::getInstance();
  cout << "smash: got ctrl-C" << endl;
   if(smash.m_pid_fg){
    if (kill(smash.m_pid_fg, SIGINT) == SYS_FAIL) {
        perror("smash error: kill failed");
        return;
     }
     cout << "smash: process " << smash.m_pid_fg << " was killed" << endl;
   }
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

