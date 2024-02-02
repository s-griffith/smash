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

char** getArgs(const char* cmd_line, int* numArgs) {
  char** args = (char**)malloc(COMMAND_MAX_ARGS * sizeof(char**));
  //initialize to nullptr?
  if (!args) {
   perror("smash error: malloc failed"); 
  }
  *numArgs = _parseCommandLine(cmd_line, args);
  return args;
}

void firstUpdateCurrDir() {
  SmallShell& smash = SmallShell::getInstance();
    char* buffer = (char*)malloc(MAX_PATH_LEGNTH * sizeof(char));
    if (!buffer) {
      free(buffer);
      perror("smash error: malloc failed"); 
    }
    buffer = getcwd(buffer, MAX_PATH_LEGNTH);
    if (!buffer) {
      free(buffer);
      perror("smash error: getcwd failed"); 
    }
    smash.setCurrDir(buffer);
}


pid_t SmallShell::m_pid = getppid();

// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell(std::string prompt) : m_prompt(prompt), m_prevDir(nullptr), m_currDirectory(nullptr) { cout << "Constructor called..........\n";  }

SmallShell::~SmallShell() {
  free(m_prevDir);
  free(m_currDirectory);
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	// For example:
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

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
//others
  // else {
  //   return new ExternalCommand(cmd_line);
  // }
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  Command* cmd = CreateCommand(cmd_line);
  cmd->execute();
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}

std::string SmallShell::getPrompt() const {
  return m_prompt;
}

ChangePromptCommand::ChangePromptCommand(const char* cmd_line) : BuiltInCommand::BuiltInCommand(cmd_line) {}

ChangePromptCommand::~ChangePromptCommand() {}

void ChangePromptCommand::execute() {
  ///TODO: NOT SUPPOSED TO CHANGE ERROR MESSAGES
  int numArgs = 0;
  char** args = getArgs(this->m_cmd_line, &numArgs);
  SmallShell& smash = SmallShell::getInstance();
  if (numArgs == 1) {
    smash.chngPrompt();
  }
  else {
    smash.chngPrompt(string(args[1]));
  }
}

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command::Command(cmd_line) {}

Command::Command(const char* cmd_line) : m_cmd_line(cmd_line) {}

Command::~Command() {
  m_cmd_line = nullptr;
}


void SmallShell::chngPrompt(const std::string newPrompt) {
  m_prompt = newPrompt;
}

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  cout << "smash pid is " << smash.m_pid << endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  if(smash.getCurrDir() == nullptr) {
    firstUpdateCurrDir();
  }
  cout << string(smash.getCurrDir()) << endl;
}

char* SmallShell::getCurrDir() const {
  return m_currDirectory;
}
void SmallShell::setCurrDir(char* currDir, char* toCombine) {
  if (toCombine == nullptr) {
    m_currDirectory = currDir;
    return;
  }
  int length = string(currDir).length() + string(toCombine).length() + 1;
  char* temp = (char*)malloc(length * sizeof(char));
  strcpy(temp, currDir);
  strcat(temp, "/");
  strcat(temp, toCombine);
  m_currDirectory = temp;
}

char* SmallShell::getPrevDir() const {
  return m_prevDir;
}
void SmallShell::setPrevDir(char* prevDir){
  m_prevDir = prevDir;
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
 // dir = (string(dir)).substr(0, cut).c_str();
  dir[cut] = '/0';
  printf("%s", dir);
  return dir;
}

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd) : BuiltInCommand(cmd_line), m_plastPwd(plastPwd) {}

///TODO: IF WANT TO MAKE THINGS MORE EFFICIENT - TRY TO SPLICE TOGETHER CURRDIR INSTEAD OF USING SYSCALL
void ChangeDirCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  if(smash.getCurrDir() == nullptr) {
    firstUpdateCurrDir();
  }
  int numArgs = 0;
  char** args = getArgs(this->m_cmd_line, &numArgs);
  if (numArgs > 2) {
    perror("smash error: cd: too many arguments");
  }
  else if (*m_plastPwd == nullptr && string(args[1]) == "-") {
    perror("smash error: cd: OLDPWD not set");
  }
  else if (string(args[1]) == "-") {
    if (chdir(*m_plastPwd) != 0) {
      perror("smash error: chdir failed");
    }
    //switch current and previous directories
    char* temp = smash.getCurrDir();
    smash.setCurrDir(smash.getPrevDir());
    smash.setPrevDir(temp);
    return;
  }
  if (chdir(args[1]) != 0) {
    perror("smash error: chdir failed");
  }
  //If the given "path" is to go up, remove the last part of the current path
  if (string(args[1]) == "..") {
    smash.setPrevDir(smash.getCurrDir());
    smash.setCurrDir(goUp(smash.getCurrDir()));
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

