#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#define SYS_FAIL -1
#endif

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
    //CHECK IF MALLOC FAILED IF SO PERROR
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

//-------------------------------------Helper Functions-------------------------------------



char** getArgs(const char* cmd_line, int* numArgs) {
  char** args = (char**)malloc(COMMAND_ARGS_MAX_LENGTH * sizeof(char*) + 1 + 1);
  if (args == nullptr) {
   perror("smash error: malloc failed");
   free(args);
   return nullptr; 
  }
  *numArgs = _parseCommandLine(cmd_line, args);
  return args;
}

void firstUpdateCurrDir() {
  SmallShell& smash = SmallShell::getInstance();
  char* buffer = (char*)malloc(MAX_PATH_LENGTH * sizeof(char) + 1);
  if (!buffer) {
    free(buffer);
    perror("smash error: malloc failed"); 
  }
  buffer = getcwd(buffer, MAX_PATH_LENGTH);
  if (!buffer) {
    free(buffer);
    perror("smash error: getcwd failed"); 
  }
  smash.setCurrDir(buffer);
}

bool checkFullPath(char* currPath, char* newPath) {
  int i = 0;
  int minLen = min(string(currPath).length(), string(newPath).length());
  for (; i < minLen; i++) {
    if (currPath[i] != newPath[i]) {
      break;
    }
  }
  if (i > 1) {
    return true;
  }
  return false;
}

char* goUp(char* dir) {
  if (!strcmp(dir, "/")) {
    return dir;
  }
  int cut = string(dir).find_last_of("/");
  dir[cut] = '\0';
  return dir;
}


//-------------------------------------SmallShell-------------------------------------

pid_t SmallShell::m_pid = getppid();//maybe getpid?

SmallShell::SmallShell(std::string prompt) : m_prompt(prompt) {
  m_prevDir = (char*)malloc((MAX_PATH_LENGTH + 1)*sizeof(char));
  strcpy(m_prevDir, "");
  m_currDirectory = (char*)malloc((MAX_PATH_LENGTH + 1)*sizeof(char));
  strcpy(m_currDirectory, "");
}

SmallShell::~SmallShell() {
  free(m_prevDir);
  free(m_currDirectory);
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
  if (cmd_line == nullptr) {
    return nullptr;
  }
  //Removes background sign (if exists):
  char cmd[COMMAND_ARGS_MAX_LENGTH];
  strcpy(cmd, cmd_line);
  _removeBackgroundSign(cmd);
  const char* cmd_line_clean = cmd;
  //Compare command without background sign:
  string cmd_s = _trim(string(cmd_line_clean));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  //Find appropriate command:
  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("chprompt") == 0) {
    return new ChangePromptCommand(cmd_line);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line, &m_prevDir);
  }
  else if (firstWord.compare("jobs") == 0) {
    return new JobsCommand(cmd_line);
  }
  else if (firstWord.compare("quit") == 0) {
   return new QuitCommand(cmd_line, &jobs);
  }
  else if (firstWord.compare("fg") == 0) {
   return new ForegroundCommand(cmd_line, &jobs);
  }
   else if (firstWord.compare("kill") == 0) {
   return new KillCommand(cmd_line, &jobs);
  }
//others
  else {    
    bool isBackground = _isBackgroundComamnd(cmd_line);
    int stat = 0;
    pid_t pid = fork();
    if (pid < 0) {
      perror("smash error: fork failed");
      return nullptr;
    }
    if (pid > 0 && !isBackground) {
      while ((pid = wait(&stat)) > 0);
      return nullptr;
    }
    if (pid == 0 && !isBackground) {
      setpgrp();
      return new ExternalCommand(cmd_line);
    }
    else if (pid == 0 && isBackground) {
      setpgrp();
      //Add to jobs list!!!!
      //Can a complex command be run in the background?
    int stat = 0;
    ExternalCommand *cmd = new ExternalCommand(fixed_cmd);
    SmallShell &shell = SmallShell::getInstance();
    pid_t pid = fork();
    if (pid < 0) {
      perror("smash error: fork failed");
    }
    if (pid > 0 && !isBackground) {
      shell.m_pid_fg = pid;
      while ((pid = wait(&stat)) > 0);//if a background son will finish? maybe waitpid
      return nullptr;
    }
    if (pid == 0 && !isBackground) {
      setpgrp();
      return cmd;
    }
    else if  (pid > 0 && isBackground){
      shell.getJobs()->addJob(cmd, pid);
    }
    else if (pid == 0 && isBackground) {
      setpgrp();
      return cmd;
    }
    }
  }
  return nullptr;
}
JobsList* SmallShell::getJobs(){
  return &jobs;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  Command* cmd = CreateCommand(cmd_line);
  if (cmd == nullptr) {
    return;
  }
  cmd->execute();
  if (dynamic_cast<ExternalCommand*>(cmd) != nullptr) {
    exit(0);
  }
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}

std::string SmallShell::getPrompt() const {
  return m_prompt;
}

void SmallShell::chngPrompt(const std::string newPrompt) {
  m_prompt = newPrompt;
}

char* SmallShell::getCurrDir() const {
  return m_currDirectory;
}
void SmallShell::setCurrDir(char* currDir, char* toCombine) {
  if (toCombine == nullptr) {
    strcpy(m_currDirectory, currDir);
    return;
  }
  int length = string(currDir).length() + string(toCombine).length() + 1;
  char* temp = (char*)malloc(length * sizeof(char));
  if (temp == nullptr) {
    free(temp);
    cerr << "smash error: malloc failed" << endl;
    return;
  }
  strcpy(temp, currDir);
  strcat(temp, "/");
  strcat(temp, toCombine);
  strcpy(m_currDirectory, temp);
  cout << m_currDirectory << endl;
}

char* SmallShell::getPrevDir() const {
  return m_prevDir;
}
void SmallShell::setPrevDir(char* prevDir){
  strcpy(m_prevDir, prevDir);
}

//-------------------------------------Command-------------------------------------

Command::Command(const char* cmd_line) : m_cmd_line(cmd_line) {}

Command::~Command() {
  m_cmd_line = nullptr;
}

const char* Command::gedCmdLine(){
  return this->m_cmd_line;
}

//-------------------------------------BuiltInCommand-------------------------------------

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command::Command(cmd_line) {}


//-------------------------------------ChangePromptCommand-------------------------------------

ChangePromptCommand::ChangePromptCommand(const char* cmd_line) : BuiltInCommand::BuiltInCommand(cmd_line) {}

ChangePromptCommand::~ChangePromptCommand() {}

void ChangePromptCommand::execute() {
  //Remove background sign if exists:
  char cmd[COMMAND_ARGS_MAX_LENGTH];
  strcpy(cmd, this->m_cmd_line);
  _removeBackgroundSign(cmd);
  const char* cmd_line_clean = cmd;
  int numArgs = 0;
  char** args = getArgs(cmd_line_clean, &numArgs);
  SmallShell& smash = SmallShell::getInstance();
  if (numArgs == 1) {
    smash.chngPrompt();
  }
  else {
    smash.chngPrompt(string(args[1]));
  }
  free(args);
}

//-------------------------------------ShowPidCommand-------------------------------------

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  cout << "smash pid is " << smash.m_pid << endl;
}

//-------------------------------------GetCurrDirCommand-------------------------------------

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  if(!strcmp(smash.getCurrDir(), "")) {
    firstUpdateCurrDir();
  }
  cout << string(smash.getCurrDir()) << endl;
}

//-------------------------------------ChangeDirCommand-------------------------------------

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd) : BuiltInCommand(cmd_line), m_plastPwd(plastPwd) {}

///TODO: IF WANT TO MAKE THINGS MORE EFFICIENT - TRY TO SPLICE TOGETHER CURRDIR INSTEAD OF USING SYSCALL
void ChangeDirCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  //Removes background sign (if exists):
  char cmd[COMMAND_ARGS_MAX_LENGTH];
  strcpy(cmd, this->m_cmd_line);
  _removeBackgroundSign(cmd);
  const char* cmd_line_clean = cmd;
  
  if(!strcmp(smash.getCurrDir(), "")) {
    firstUpdateCurrDir();
  }
  int numArgs = 0;
  char** args = getArgs(cmd_line_clean, &numArgs);
  if (numArgs > 2) { //the command itself counts as an arg
    cerr << "smash error: cd: too many arguments" << endl;
    free(args);
    return;
  }
  else if (*m_plastPwd == nullptr && string(args[1]) == "-") {
    cerr << "smash error: cd: OLDPWD not set" << endl;
    free(args);
    return;
  }
  else if (string(args[1]) == "-") {
    if (chdir(*m_plastPwd) != 0) {
      perror("smash error: chdir failed");
      free(args);
      return;
    }
    //switch current and previous directories
    char temp[MAX_PATH_LENGTH + 1];
    strcpy(temp, smash.getCurrDir());
    //char* temp = smash.getCurrDir();
    smash.setCurrDir(smash.getPrevDir());
    smash.setPrevDir(temp);
    return;
  }
  if (chdir(args[1]) != 0) {
    perror("smash error: chdir failed");
    free(args);
    return;
  }
  //If the given "path" is to go up, remove the last part of the current path
  if (string(args[1]) == "..") {
    smash.setPrevDir(smash.getCurrDir());
    smash.setCurrDir(goUp(smash.getCurrDir()));
    return;
  }
  //If the new path is the full path, set currDir equal to it
  
  if (checkFullPath(smash.getCurrDir(), args[1])) {
    smash.setPrevDir(smash.getCurrDir());
    smash.setCurrDir(args[1]);
  }
  //If not, append the new folder to the end of the current path
  else {
    smash.setPrevDir(smash.getCurrDir());
    //Figure out how to move the string into a char without allocating memory here and not being able to delete it
    smash.setCurrDir(smash.getCurrDir(), args[1]);
  }
}

//-------------------------------------ExternalCommand-------------------------------------
ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute() {
  bool isComplex = string(this->m_cmd_line).find("*") != string::npos || string(this->m_cmd_line).find("?")!= string::npos;
  if (isComplex) {
    char cmd_trimmed[COMMAND_ARGS_MAX_LENGTH];
    strcpy(cmd_trimmed, _trim(string(this->m_cmd_line)).c_str());
    char c[] = "-c";
    char path[] = "/bin/bash";
    char* complexArgs[] = {path, c, cmd_trimmed, nullptr};
    if (execv(path, complexArgs) == -1) {
      perror("smash error: execv failed");
      return;
    }
  }
  else {
    int numArgs = 0;
    char** args = getArgs(this->m_cmd_line, &numArgs);
    string command = string(args[0]);
    if (execvp(command.c_str(), args) == -1) {
      perror("smash error: evecvp failed");
      free(args);
      return;
    }
    free(args);
  }
}