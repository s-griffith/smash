#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY() \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT() \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#define SYS_FAIL -1
#endif

string _ltrim(const std::string &s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args)
{
  FUNC_ENTRY()
  int i = 0;
  string cmd = string(cmd_line);
  size_t indexA = string(cmd_line).find(">>");
  size_t indexW = string(cmd_line).find(">");
  if (indexA != string::npos) {
    cmd = cmd.substr(0, indexA) + " >> " + cmd.substr(indexA+1, cmd.length()-indexA);
  }
  else if (indexW != string::npos) {
    cmd = cmd.substr(0, indexW) + " > " + cmd.substr(indexW+1, cmd.length()-indexW);
  }
  std::istringstream iss(_trim(string(cmd)).c_str());
  for (std::string s; iss >> s;)
  {
    args[i] = (char *)malloc(s.length() + 1);
    memset(args[i], 0, s.length() + 1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
    // CHECK IF MALLOC FAILED IF SO PERROR
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line)
{
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line)
{
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos)
  {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&')
  {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

//-------------------------------------Helper Functions-------------------------------------

char **getArgs(const char *cmd_line, int *numArgs)
{
  // Remove background sign if exists:
  char cmd[COMMAND_ARGS_MAX_LENGTH];
  strcpy(cmd, cmd_line);
  _removeBackgroundSign(cmd);
  const char *cmd_line_clean = cmd;

  char **args = (char **)malloc((COMMAND_ARGS_MAX_LENGTH + 1) * sizeof(char *));
  if (args == nullptr)
  {
    perror("smash error: malloc failed");
    free(args);
    return nullptr;
  }
  *numArgs = _parseCommandLine(cmd_line_clean, args);
  return args;
}

void firstUpdateCurrDir()
{
  SmallShell &smash = SmallShell::getInstance();
  char *buffer = (char *)malloc(MAX_PATH_LENGTH * sizeof(char) + 1);
  if (!buffer)
  {
    free(buffer);
    perror("smash error: malloc failed");
  }
  buffer = getcwd(buffer, MAX_PATH_LENGTH);
  if (!buffer)
  {
    free(buffer);
    perror("smash error: getcwd failed");
  }
  smash.setCurrDir(buffer);
}

bool checkFullPath(char *currPath, char *newPath)
{
  int i = 0;
  int minLen = min(string(currPath).length(), string(newPath).length());
  for (; i < minLen; i++)
  {
    if (currPath[i] != newPath[i])
    {
      break;
    }
  }
  if (i > 1)
  {
    return true;
  }
  return false;
}

char *goUp(char *dir)
{
  if (!strcmp(dir, "/"))
  {
    return dir;
  }
  int cut = string(dir).find_last_of("/");
  dir[cut] = '\0';
  return dir;
}

bool is_number(const std::string &s)
{
  std::string::const_iterator it = s.begin();
  // if(*it == '-') ++it; //negative number
  while (it != s.end() && (std::isdigit(*it) || *it == '-'))
    ++it;
  return !s.empty() && it == s.end();
}

//-------------------------------------SmallShell-------------------------------------

pid_t SmallShell::m_pid = getpid();

SmallShell::SmallShell(std::string prompt) : m_prompt(prompt)
{
  m_prevDir = (char *)malloc((MAX_PATH_LENGTH + 1) * sizeof(char));
  strcpy(m_prevDir, "");
  m_currDirectory = (char *)malloc((MAX_PATH_LENGTH + 1) * sizeof(char));
  strcpy(m_currDirectory, "");
}

SmallShell::~SmallShell()
{
  free(m_prevDir);
  free(m_currDirectory);
}

/**
 * Creates and returns a pointer to Command class which matches the given command line (cmd_line)
 */
Command *SmallShell::CreateCommand(const char *cmd_line)
{
  if (cmd_line == nullptr)
  {
    return nullptr;
  }
  //Check if command is an IO redirection:
  SmallShell &shell = SmallShell::getInstance();
  if (strstr(cmd_line, ">") != nullptr || strstr(cmd_line, ">>") != nullptr) {
    int stat = 0;
    pid_t pid = fork();
    if (pid < 0)
    {
      perror("smash error: fork failed");
      return nullptr;
    }
    else if (pid > 0) {
      shell.m_pid_fg = pid;
      pid = waitpid(pid, &stat, WUNTRACED);
      return nullptr;
    }
    else {
      setpgrp();
      return new RedirectionCommand(cmd_line);
    }
  }
  // Removes background sign (if exists):
  char cmd[COMMAND_ARGS_MAX_LENGTH];
  strcpy(cmd, cmd_line);
  _removeBackgroundSign(cmd);
  const char *cmd_line_clean = cmd;
  // Compare command without background sign:
  string cmd_s = _trim(string(cmd_line_clean));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  // Find appropriate command:
  if(strchr(cmd, '|')){
    pid_t pid = fork();
    if(pid > 0){
      int status;
      shell.m_pid_fg = pid;
      pid = waitpid(pid, &status, WUNTRACED);
      return nullptr;
    }
    else{
      setpgrp();
      return new PipeCommand(cmd_line);
    
  }
  }
  if (firstWord.compare("pwd") == 0)
  {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0)
  {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("chprompt") == 0)
  {
    return new ChangePromptCommand(cmd_line);
  }
  else if (firstWord.compare("cd") == 0)
  {
    return new ChangeDirCommand(cmd_line, &m_prevDir);
  }
  else if (firstWord.compare("jobs") == 0)
  {
    return new JobsCommand(cmd_line);
  }
  else if (firstWord.compare("quit") == 0)
  {
    return new QuitCommand(cmd_line, &jobs);
  }
  else if (firstWord.compare("fg") == 0)
  {
    return new ForegroundCommand(cmd_line, &jobs);
  }
  else if (firstWord.compare("kill") == 0)
  {
    return new KillCommand(cmd_line, &jobs);
  }
  else if (firstWord.compare("chmod") == 0)
  {
    return new ChmodCommand(cmd_line);
  }
  // others
  else
  {
    bool isBackground = _isBackgroundComamnd(cmd_line);
    int stat = 0;
    pid_t pid = fork();
    if (pid < 0)
    {
      perror("smash error: fork failed");
      return nullptr;
    }
    if (pid > 0 && !isBackground)
    {
      shell.m_pid_fg = pid;
      pid = waitpid(pid, &stat, WUNTRACED);
      return nullptr;
    }
    if (pid == 0)
    {
      setpgrp();
      return new ExternalCommand(cmd_line);
    }
    else if (pid > 0 && isBackground)
    {
      shell.getJobs()->addJob(cmd_line, pid);
      return nullptr;
    }
  }
  return nullptr;
}

JobsList *SmallShell::getJobs()
{
  return &jobs;
}

void SmallShell::executeCommand(const char *cmd_line)
{
  // TODO: Add your implementation here
  // for example:
  Command *cmd = CreateCommand(cmd_line);
  if (cmd == nullptr)
  {
    return;
  }
  cmd->execute();
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}

std::string SmallShell::getPrompt() const
{
  return m_prompt;
}

void SmallShell::chngPrompt(const std::string newPrompt)
{
  m_prompt = newPrompt;
}

char *SmallShell::getCurrDir() const
{
  return m_currDirectory;
}
void SmallShell::setCurrDir(char *currDir, char *toCombine)
{
  if (toCombine == nullptr)
  {
    strcpy(m_currDirectory, currDir);
    return;
  }
  int length = string(currDir).length() + string(toCombine).length() + 1;
  char *temp = (char *)malloc(length * sizeof(char));
  if (temp == nullptr)
  {
    free(temp);
    cerr << "smash error: malloc failed" << endl;
    return;
  }
  strcpy(temp, currDir);
  strcat(temp, "/");
  strcat(temp, toCombine);
  strcpy(m_currDirectory, temp);
}

char *SmallShell::getPrevDir() const
{
  return m_prevDir;
}
void SmallShell::setPrevDir(char *prevDir)
{
  strcpy(m_prevDir, prevDir);
}




//-------------------------------------Pipe-------------------------------------
PipeCommand::PipeCommand(const char *cmd_line): Command(cmd_line){}
void PipeCommand::execute(){
  
  //char cmd1[COMMAND_ARGS_MAX_LENGTH];
 // char cmd2[COMMAND_ARGS_MAX_LENGTH];
  string str1 = string(this->m_cmd_line);
  int pipeIndex = str1.find('|');
   cout << pipeIndex<< endl;
  string first = str1.substr(0, pipeIndex);
  string sec = str1.substr(pipeIndex+1);
  int numArgs1;
  char **args1 = getArgs(first.c_str(), &numArgs1);
  int numArgs2;
  char **args2 = getArgs(sec.c_str(), &numArgs2);
  int my_pipe[2];
  pipe(my_pipe);
 if (fork()==0) { // son
    if (dup2(my_pipe[1], STDOUT_FILENO) == -1) {
        std::cerr << "Failed to redirect stdout to pipe." << std::endl;
        return;
    }
    close(my_pipe[0]);
    close(my_pipe[1]);
    string command = string(args1[0]);
    execvp(command.c_str(), args1);
    perror("failed 410");
   } 
  else {
    if (dup2(my_pipe[0], STDIN_FILENO) == -1) {
        //std::cerr << "Failed to redirect stdout to pipe." << std::endl;
        return;
    } 
    string command = string(args2[0]);
    execvp(command.c_str(), args2);
    perror("failed 419");
  }
}


//-------------------------------------Chmod-------------------------------------
ChmodCommand::ChmodCommand(const char* cmd_line): BuiltInCommand(cmd_line){}
void ChmodCommand::execute(){
   int permissionsNum;
   int numArgs;
    char **args = getArgs(this->m_cmd_line, &numArgs);
    if(numArgs !=3 ){
      cerr << "smash error: chmod: invalid arguments" << endl;
      return;
    }
    if (!is_number(args[1])){
        cerr << "smash error: chmod: invalid arguments" << endl;
         return;    
    }
    permissionsNum = stoi(args[1], nullptr, 8);
    if((permissionsNum<0 || permissionsNum>777) && !(permissionsNum<4777 && permissionsNum>4000)){
      cerr << "smash error: chmod: invalid arguments" << endl;
      return;
    }
    if(chmod(args[2], permissionsNum) != 0){
        perror("smash error: chmod failed");
    }

}


//-------------------------------------Jobs-------------------------------------
JobsList::JobEntry::JobEntry(int id, pid_t pid, const char *cmd, bool isStopped) : m_id(id), m_pid(pid), m_isStopped(isStopped)
{
  m_cmd = (char *)malloc((COMMAND_ARGS_MAX_LENGTH + 1) * sizeof(char));
  strcpy(m_cmd, cmd);
}

void JobsList::addJob(const char *cmd, pid_t pid, bool isStopped)
{
  removeFinishedJobs(); // think on a better way to update maxId!!!!
  JobEntry newJob(max_id + 1, pid, cmd, isStopped);
  this->m_list.push_back(newJob);
  max_id++;
}
void JobsList::printJobsList()
{
  removeFinishedJobs();
  int i = 1;
  for (JobEntry job : m_list)
  {
    // element.job.second
    std::cout << "[" << i << "] " << job.m_cmd << endl;
    i++;
  }
}

JobsList::JobEntry *JobsList::getJobById(int jobId)
{
  for (auto &job : m_list)
  {
    if (job.m_id == jobId)
    {
      return &job;
    }
  }
  return nullptr;
}

void JobsList::removeJobById(int jobId)
{
  for (auto it = m_list.begin(); it != m_list.end(); ++it)
  {
    auto job = *it;
    if (jobId == job.m_pid)
    {
      m_list.erase(it);
      --it;
      return;
    }
  }
}

void JobsList::removeFinishedJobs()
{
  if (m_list.empty())
  {
    max_id = 0;
    return;
  }
  int max = 0;
  for (auto it = m_list.begin(); it != m_list.end(); ++it)
  {
    auto job = *it;
    int status;
    int ret_wait = waitpid(job.m_pid, &status, WNOHANG);
    if (ret_wait == job.m_pid || ret_wait == -1)
    {
      m_list.erase(it);
      --it;
    }
    else if (job.m_id > max)
    {
      max = job.m_id;
    }
  }
  max_id = max;
}

bool JobsList::isEmpty()
{
  return !m_list.size();
}

int JobsList::getMaxId()
{
  return max_id;
}

JobsCommand::JobsCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
void JobsCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  smash.getJobs()->printJobsList();
}
void JobsList::killAllJobs()
{
  cout << "smash: sending SIGKILL signal to " << m_list.size() << " jobs:" << endl;
  removeFinishedJobs();
  for (JobEntry element : m_list)
  {
    cout << element.m_pid << ": " << element.m_cmd << endl; // remove space
    kill(element.m_pid, SIGKILL);
  }
}

void JobsList::sigJobById(int jobId, int signum)
{
  JobEntry *job = getJobById(jobId);
  if (!job)
  {
    cerr << "smash error: kill: job-id " << jobId << " does not exist" << endl;
    return;
  }
  if (kill(job->m_pid, signum) == SYS_FAIL)
  {
    perror("smash error: kill failed");
    return;
  }
  if (signum == SIGTSTP)
  {
    job->m_isStopped = true;
  }
  else if (signum == SIGCONT)
  {
    job->m_isStopped = false;
  }
  cout << "signal number " << signum << " was sent to pid " << job->m_pid << endl;
}
// JobsList(){}

//-------------------------------------ForeGround-------------------------------------
ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), m_jobs(jobs) {}
void ForegroundCommand::execute()
{
  int numArgs;
  char **args = getArgs(this->m_cmd_line, &numArgs);
  int job_id;
  if (numArgs == 1)
  {
    job_id = m_jobs->getMaxId();
  }
  else if (!is_number(args[1]))
    {
      cerr << "smash error: fg: invalid arguments" << endl;
      return;
      }
  else{
      job_id = stoi(args[1]);
    }
  JobsList::JobEntry *job = m_jobs->getJobById(job_id);
  if (!job)
  {
    cerr << "smash error: fg: job-id " << job_id << " does not exist" << endl;
    return;
  }
  if (m_jobs->isEmpty())
  {
    cerr << "smash error: fg: jobs list is empty" << endl;
    return;
  }
  if (numArgs > 2)
  {
    cerr << "smash error: fg: invalid arguments" << endl;
    return;
  }
  SmallShell &smash = SmallShell::getInstance();
  if (job_id >= 0 && job)
  {
    int job_pid = job->m_pid;
    if (job->m_isStopped)
    {
      if (kill(job_pid, SIGCONT) == SYS_FAIL)
      {
        perror("smash error: kill failed");
        // free_args(args, num_of_args);
        /// TODO: check free
        return;
      }
    }

    int status;
    cout << job->m_cmd << " " << job_pid << endl;
    smash.m_pid_fg = job_pid;
    m_jobs->removeJobById(job_id);
    if (waitpid(job_pid, &status, WUNTRACED) == SYS_FAIL)
    {
      perror("smash error: waitpid failed");
      // free_args(args, num_of_args);
      return;
    }
    smash.m_pid_fg = 0;
  }
}


//-------------------------------------Quit-------------------------------------
QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), m_jobs(jobs) {}
void QuitCommand::execute()
{
  int numArgs = 0;
  char **args = getArgs(this->m_cmd_line, &numArgs);
  if (numArgs > 1 && string(args[1]) == "kill")
  {
    m_jobs->killAllJobs();
  }
  exit(0);
}

//-------------------------------------Kill-------------------------------------
KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), m_jobs(jobs) {}
void KillCommand::execute()
{
  int num_of_args;
  int job_id;
  int signum;
  char **args = getArgs(this->m_cmd_line, &num_of_args);
  if (num_of_args < 3)
  {
    cerr << "smash error: kill: invalid arguments" << endl;
    free(args);
    return;
  }
  else
  {
    try
    {
      // Check for a valid job-id
      if (!is_number(args[2]))
        throw exception();
      char first_char = string(args[1]).at(0);
      char minus = '-';
      if (first_char != minus)
        throw exception();
      job_id = stoi(args[2]);

      // Check for a valid signal number

      if (!is_number(string(args[1]).erase(0, 1)))
        throw exception();
      signum = stoi(string(args[1]).erase(0, 1));
    }
    catch (exception &)
    {
      cerr << "smash error: kill: invalid arguments" << endl;
      return;
    }
    SmallShell &smash = SmallShell::getInstance();
    JobsList::JobEntry *job = smash.getJobs()->getJobById(job_id);
    if (!job)
    {
      cerr << "smash error: kill: job-id " << job_id << " does not exist" << endl;
      return;
    }
    if (num_of_args > 3)
    {
      cerr << "smash error: kill: invalid arguments" << endl;
      return;
    }
    m_jobs->sigJobById(job_id, signum);
  }
}

//-------------------------------------Command-------------------------------------

Command::Command(const char *cmd_line) : m_cmd_line(cmd_line) {}

Command::~Command()
{
  m_cmd_line = nullptr;
}

const char *Command::gedCmdLine()
{
  return this->m_cmd_line;
}

//-------------------------------------BuiltInCommand-------------------------------------

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command::Command(cmd_line) {}

//-------------------------------------ChangePromptCommand-------------------------------------

ChangePromptCommand::ChangePromptCommand(const char *cmd_line) : BuiltInCommand::BuiltInCommand(cmd_line) {}

ChangePromptCommand::~ChangePromptCommand() {}

void ChangePromptCommand::execute()
{
  int numArgs = 0;
  char **args = getArgs(this->m_cmd_line, &numArgs);
  SmallShell &smash = SmallShell::getInstance();
  if (numArgs == 1)
  {
    smash.chngPrompt();
  }
  else
  {
    smash.chngPrompt(string(args[1]));
  }
  free(args);
}

//-------------------------------------ShowPidCommand-------------------------------------

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  cout << "smash pid is " << smash.m_pid << endl;
}

//-------------------------------------GetCurrDirCommand-------------------------------------

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  if (!strcmp(smash.getCurrDir(), ""))
  {
    firstUpdateCurrDir();
  }
  cout << string(smash.getCurrDir()) << endl;
}

//-------------------------------------ChangeDirCommand-------------------------------------

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line), m_plastPwd(plastPwd) {}

/// TODO: IF WANT TO MAKE THINGS MORE EFFICIENT - TRY TO SPLICE TOGETHER CURRDIR INSTEAD OF USING SYSCALL
void ChangeDirCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  if (!strcmp(smash.getCurrDir(), ""))
  {
    firstUpdateCurrDir();
  }
  int numArgs = 0;
  char **args = getArgs(this->m_cmd_line, &numArgs);
  if (numArgs > 2)
  { // the command itself counts as an arg
    cerr << "smash error: cd: too many arguments" << endl;
    free(args);
    return;
  }
  else if (!strcmp(*m_plastPwd, "") && string(args[1]) == "-")
  {
    cerr << "smash error: cd: OLDPWD not set" << endl;
    free(args);
    return;
  }
  else if (string(args[1]) == "-")
  {
    if (chdir(*m_plastPwd) != 0)
    {
      perror("smash error: chdir failed");
      free(args);
      return;
    }
    // switch current and previous directories
    char temp[MAX_PATH_LENGTH + 1];
    strcpy(temp, smash.getCurrDir());
    // char* temp = smash.getCurrDir();
    smash.setCurrDir(smash.getPrevDir());
    smash.setPrevDir(temp);
    return;
  }
  if (chdir(args[1]) != 0)
  {
    perror("smash error: chdir failed");
    free(args);
    return;
  }
  // If the given "path" is to go up, remove the last part of the current path
  if (string(args[1]) == "..")
  {
    smash.setPrevDir(smash.getCurrDir());
    smash.setCurrDir(goUp(smash.getCurrDir()));
    return;
  }
  // If the new path is the full path, set currDir equal to it

  if (checkFullPath(smash.getCurrDir(), args[1]))
  {
    smash.setPrevDir(smash.getCurrDir());
    smash.setCurrDir(args[1]);
  }
  // If not, append the new folder to the end of the current path
  else
  {
    smash.setPrevDir(smash.getCurrDir());
    // Figure out how to move the string into a char without allocating memory here and not being able to delete it
    smash.setCurrDir(smash.getCurrDir(), args[1]);
  }
}

//-------------------------------------ExternalCommand-------------------------------------
ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute()
{
  bool isComplex = string(this->m_cmd_line).find("*") != string::npos || string(this->m_cmd_line).find("?") != string::npos;
  if (isComplex)
  {
    char cmd_trimmed[COMMAND_ARGS_MAX_LENGTH];
    strcpy(cmd_trimmed, _trim(string(this->m_cmd_line)).c_str());
    char c[] = "-c";
    char path[] = "/bin/bash";
    char *complexArgs[] = {path, c, cmd_trimmed, nullptr};
    if (execv(path, complexArgs) == -1)
    {
      perror("smash error: execv failed");
      exit(0);
    }
  }
  else
  {
    int numArgs = 0;
    char **args = getArgs(this->m_cmd_line, &numArgs);
    string command = string(args[0]);
    if (execvp(command.c_str(), args) == -1)
    {
      perror("smash error: evecvp failed");
      free(args);
      exit(0);
    }
    free(args);
  }
}

//-------------------------------------Redirection Command-------------------------------------
RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {}

void RedirectionCommand::execute()
{
  int numArgs = 0;
  char** args = getArgs(this->m_cmd_line, &numArgs);
  SmallShell& smash = SmallShell::getInstance();
  char cmd[COMMAND_ARGS_MAX_LENGTH + 1];
  strcpy(cmd, this->m_cmd_line);
  char* over = strstr(cmd, ">");
  char* app = strstr(cmd, ">>");
  if (app != nullptr) {
    for (int i = 0; i < COMMAND_MAX_ARGS; i++) {
      cout << args[i] << endl;
      if (strcmp(">>", args[i]) == 0 ) {
        *app = '\0';
        cout << cmd << endl;
        close(1);
        open(args[i+1], O_RDWR | O_APPEND | O_CREAT);
        smash.executeCommand(cmd);
        exit(0);
      }
    }
  }

  else   if (over != nullptr) {
    for (int i = 0; i < COMMAND_MAX_ARGS; i++) {
      if (strcmp(">", args[i]) == 0 ) {
        *over = '\0';
        fclose(stdout);
        fopen(args[i+1], "w");
        smash.executeCommand(cmd);
        exit(0);
      }
    }
  }
}