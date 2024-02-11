#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

#define SYS_FAIL -1

using namespace std;

void ctrlCHandler(int sig_num) {
   SmallShell& smash = SmallShell::getInstance();

   if(smash.m_pid_fg){
    if (kill(smash.getJobs()->getJobById(smash.m_pid_fg)->m_pid, SIGINT) == SYS_FAIL) {
        perror("smash error: kill failed");
        return;
     }
   }
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

