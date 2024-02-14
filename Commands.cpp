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
const int SYS_FAIL = -1;

#if 0
#define FUNC_ENTRY() \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT() \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
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
  if (indexA != string::npos)
  {
    cmd = cmd.substr(0, indexA) + " >> " + cmd.substr(indexA + 2, cmd.length() - indexA - 1);
  }
  else if (indexW != string::npos)
  {
    cmd = cmd.substr(0, indexW) + " > " + cmd.substr(indexW + 1, cmd.length() - indexW);
  }
  std::istringstream iss(_trim(string(cmd)).c_str());
  for (std::string s; iss >> s;)
  {
    args[i] = (char *)malloc(s.length() + 1);
    memset(args[i], 0, s.length() + 1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
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

//-----------------------------------------------Helper Functions-------------------------------------------------------

/*
 * Parses/Extracts each individual argument and command
 * @param cmd_line - the CMD line received
 * @param numArgs - an int value to be filled with the number of args discovered
 * @return
 *      char** - an array of arguments
 */
char **getArgs(const char *cmd_line, int *numArgs)
{
  // Remove background sign if exists:
  char cmd[COMMAND_ARGS_MAX_LENGTH];
  strcpy(cmd, cmd_line);
  _removeBackgroundSign(cmd);
  const char *cmd_line_clean = cmd;

  // Parse arguments:
  char **args = (char **)malloc((COMMAND_ARGS_MAX_LENGTH + 1) * sizeof(char *));
  if (args == nullptr)
  {
    cerr << "smash error: malloc failed" << endl;
    free(args);
    return nullptr;
  }
  // Initialize args to avoid valgrind errors:
  std::fill_n(args, COMMAND_ARGS_MAX_LENGTH + 1, nullptr);
  *numArgs = _parseCommandLine(cmd_line_clean, args);
  return args;
}

/*
 * Delete the args array received from getArgs()
 * @param args - the args array
 * @return
 *      void
 */
void deleteArgs(char **args)
{
  // Delete each argument in args:
  for (int i = 0; i < COMMAND_MAX_ARGS + 1; i++)
  {
    free(args[i]);
  }
  // Delete args itself:
  free(args);
}

/*
 * Checks whether a given path is a full or partial path
 * @param newPath - the new path
 * @return
 *      bool - whether the new path is a full path
 */
bool checkFullPath(char *newPath)
{
  if (newPath[0] == '/')
    return true;
  return false;
}

/*
 * Splices a given directory to "go up" (remove last directory)
 * @param dir - the current directory
 * @return
 *      char* - a pointer to the directory after "going up"
 */
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

/*
 * Checks if the string received is a number
 * @param s - the string received
 * @return
 *      bool - whether the string is a number
 */
bool is_number(const std::string &s)
{
  std::string::const_iterator it = s.begin();
  while (it != s.end() && (std::isdigit(*it) || *it == '-'))
    ++it;
  return !s.empty() && it == s.end();
}

//-----------------------------------------------Command-----------------------------------------------

Command::Command(const char *cmd_line) : m_cmd_line(cmd_line) {}

Command::~Command()
{
  m_cmd_line = nullptr;
}

void Command::firstUpdateCurrDir()
{
  SmallShell &smash = SmallShell::getInstance();
  char *buffer = (char *)malloc(MAX_PATH_LENGTH * sizeof(char) + 1);
  if (!buffer)
  {
    free(buffer);
    cerr << "smash error: malloc failed" << endl;
    return;
  }
  buffer = getcwd(buffer, MAX_PATH_LENGTH);
  if (!buffer)
  {
    free(buffer);
    perror("smash error: getcwd failed");
    return;
  }
  smash.setCurrDir(buffer);
  free(buffer);
}

//-----------------------------------------------BuiltInCommand-----------------------------------------------

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command::Command(cmd_line) {}

//-----------------------------------------------Jobs-----------------------------------------------
JobsList::JobEntry::JobEntry(int id, pid_t pid, const char *cmd, bool isStopped) : m_id(id), m_pid(pid),
                                                                                   m_isStopped(isStopped)
{
  strcpy(m_cmd, cmd);
}

void JobsList::addJob(const char *cmd, pid_t pid, bool isStopped)
{
  removeFinishedJobs();
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
    std::cout << "[" << job.m_id << "] " << job.m_cmd << endl;
    i++;
  }
}

void JobsList::killAllJobs()
{
  removeFinishedJobs();
  cout << "smash: sending SIGKILL signal to " << m_list.size() << " jobs:" << endl;
  for (JobEntry element : m_list)
  {
    cout << element.m_pid << ": " << element.m_cmd << endl;
    if (kill(element.m_pid, SIGKILL) == SYS_FAIL)
    {
      perror("smash error: kill failed");
    }
  }
}

JobsList::JobEntry *JobsList::getJobById(int jobId)
{
  removeFinishedJobs();
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

bool JobsList::isEmpty()
{
  return !m_list.size();
}

int JobsList::getMaxId()
{
  return max_id;
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

//-------------------------------------Built-In Commands-------------------------------------
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
  deleteArgs(args);
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

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line),
                                                                            m_plastPwd(plastPwd) {}

void ChangeDirCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  if (!strcmp(smash.getCurrDir(), ""))
  {
    firstUpdateCurrDir();
  }
  int numArgs = 0;
  char **args = getArgs(this->m_cmd_line, &numArgs);
  if (numArgs > 2) // The command itself counts as an arg
  {
    cerr << "smash error: cd: too many arguments" << endl;
    deleteArgs(args);
    return;
  }
  else if (!strcmp(*m_plastPwd, "") && string(args[1]) == "-")
  {
    cerr << "smash error: cd: OLDPWD not set" << endl;
    deleteArgs(args);
    return;
  }
  else if (string(args[1]) == "-")
  {
    if (chdir(*m_plastPwd) == SYS_FAIL)
    {
      perror("smash error: chdir failed");
      deleteArgs(args);
      return;
    }
    // Switch current and previous directories
    char temp[MAX_PATH_LENGTH + 1];
    strcpy(temp, smash.getPrevDir());
    smash.setPrevDir();
    smash.setCurrDir(temp);
    deleteArgs(args);
    return;
  }
  if (chdir(args[1]) == SYS_FAIL)
  {
    perror("smash error: chdir failed");
    deleteArgs(args);
    return;
  }
  // If the given "path" is to go up, remove the last part of the current path
  if (string(args[1]) == "..")
  {
    smash.setPrevDir();
    goUp(smash.getCurrDir());
    deleteArgs(args);
    return;
  }

  // If the new path is the full path, set currDir equal to it
  if (checkFullPath(args[1]))
  {
    smash.setPrevDir();
    smash.setCurrDir(args[1]);
  }
  // If not, append the new folder to the end of the current path
  else
  {
    if (string(smash.getCurrDir()) == "/") {
      smash.setPrevDir();
      char newPath[MAX_PATH_LENGTH + 2];
      newPath[0] = '/';
      newPath[1] = *args[1];
      smash.setCurrDir(newPath);
      deleteArgs(args);
      return;
    }
    smash.setPrevDir();
    smash.setCurrDir(smash.getCurrDir(), args[1]);
  }
  deleteArgs(args);
}

//-------------------------------------JobsCommand-------------------------------------

JobsCommand::JobsCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void JobsCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  smash.getJobs()->printJobsList();
}

//-------------------------------------Foreground-------------------------------------

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), m_jobs(jobs) {}

void ForegroundCommand::execute()
{
  int numArgs;
  char **args = getArgs(this->m_cmd_line, &numArgs);
  int job_id;
  if (numArgs == 1)
  {
    if (m_jobs->isEmpty())
    {
      cerr << "smash error: fg: jobs list is empty" << endl;
      deleteArgs(args);
      return;
    }
    {
      job_id = m_jobs->getMaxId();
    }
  }
  else if (!is_number(args[1]))
  {
    cerr << "smash error: fg: invalid arguments" << endl;
    deleteArgs(args);
    return;
  }
  else
  {
    job_id = stoi(args[1]);
  }

  JobsList::JobEntry *job = m_jobs->getJobById(job_id);
  if (!job)
  {
    cerr << "smash error: fg: job-id " << job_id << " does not exist" << endl;
    deleteArgs(args);
    return;
  }
  if (m_jobs->isEmpty())
  {
    cerr << "smash error: fg: jobs list is empty" << endl;
    deleteArgs(args);
    return;
  }
  if (numArgs > 2)
  {
    cerr << "smash error: fg: invalid arguments" << endl;
    deleteArgs(args);
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
        deleteArgs(args);
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
      deleteArgs(args);
      return;
    }
    smash.m_pid_fg = 0;
  }
  deleteArgs(args);
}

//-------------------------------------QuitCommand-------------------------------------

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), m_jobs(jobs) {}

void QuitCommand::execute()
{
  int numArgs = 0;
  char **args = getArgs(this->m_cmd_line, &numArgs);
  if (numArgs > 1 && string(args[1]) == "kill")
  {
    m_jobs->killAllJobs();
  }
  deleteArgs(args);
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
    deleteArgs(args);
    return;
  }
  try
  {
    SmallShell &smash = SmallShell::getInstance();
    // Check for a valid job-id
    if (!is_number(args[2]))
      throw exception();
    job_id = stoi(args[2]);

    JobsList::JobEntry *job = smash.getJobs()->getJobById(job_id);
    if (!job)
    {
      cerr << "smash error: kill: job-id " << job_id << " does not exist" << endl;
      deleteArgs(args);
      return;
    }

    // Check for a valid signal number
    char first_char = string(args[1]).at(0);
    char minus = '-';
    if (first_char != minus)
      throw exception();

    if (!is_number(string(args[1]).erase(0, 1)))
      throw exception();
    signum = stoi(string(args[1]).erase(0, 1));
  }
  catch (exception &)
  {
    cerr << "smash error: kill: invalid arguments" << endl;
    deleteArgs(args);
    return;
  }

  if (num_of_args > 3)
  {
    cerr << "smash error: kill: invalid arguments" << endl;
    deleteArgs(args);
    return;
  }
  m_jobs->sigJobById(job_id, signum);
  deleteArgs(args);
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
      perror("smash error: execvp failed");
      deleteArgs(args);
      exit(0);
    }
    deleteArgs(args);
  }
}

//-------------------------------------Special Commands-------------------------------------
//-------------------------------------Redirection Command-------------------------------------

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {}

void RedirectionCommand::execute()
{
  int numArgs = 0;
  char **args = getArgs(this->m_cmd_line, &numArgs);
  SmallShell &smash = SmallShell::getInstance();
  char cmd[COMMAND_ARGS_MAX_LENGTH + 1];
  strcpy(cmd, this->m_cmd_line);
  char *over = strstr(cmd, ">");
  char *app = strstr(cmd, ">>");
  for (int i = 0; i < COMMAND_MAX_ARGS; i++)
  {
    if (app != nullptr && strcmp(">>", args[i]) == 0)
    {
      *app = '\0';
      if (close(1) == SYS_FAIL)
      {
        perror("smash error: close failed");
        deleteArgs(args);
        exit(0);
      }
      if (open(args[i + 1], O_WRONLY | O_APPEND | O_CREAT, 0777) == SYS_FAIL)
      {
        perror("smash error: open failed");
        deleteArgs(args);
        exit(0);
      }
      break;
    }
    else if (over != nullptr && strcmp(">", args[i]) == 0)
    {
      *over = '\0';
      if (close(1) == SYS_FAIL)
      {
        perror("smash error: close failed");
        deleteArgs(args);
        exit(0);
      }
      if (open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777) == SYS_FAIL)
      {
        perror("smash error: open failed");
        deleteArgs(args);
        exit(0);
      }
      break;
    }
  }
  smash.executeCommand(cmd);
  deleteArgs(args);
  exit(0);
}

//--------------------------------------------------------Pipe----------------------------------------------------------

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {}

void PipeCommand::execute()
{
  string str1 = string(this->m_cmd_line);
  int pipeIndex = str1.find('|');
  int isAmpersand = 0;
  if (str1.find('&') != string::npos)
  {
    isAmpersand = 1;
  }
  string first = str1.substr(0, pipeIndex + isAmpersand);
  string sec = str1.substr(pipeIndex + isAmpersand + 1);
  int numArgs1;
  char **args1 = getArgs(first.c_str(), &numArgs1);
  int numArgs2;
  char **args2 = getArgs(sec.c_str(), &numArgs2);
  int my_pipe[2];
  pipe(my_pipe);
  if (fork() == 0) // Child
  {
    if (setpgrp() == SYS_FAIL)
    {
      perror("smash error: setpgrp failed");
      return;
    }
    if (!isAmpersand)
    {
      if (dup2(my_pipe[1], STDOUT_FILENO) == -1)
      {
        deleteArgs(args1);
        deleteArgs(args2);
        perror("smash error: dup2 failed");
        exit(0);
      }
    }
    else
    {
      if (dup2(my_pipe[1], 2) == -1)
      {
        deleteArgs(args1);
        deleteArgs(args2);
        perror("smash error: dup2 failed");
        exit(0);
      }
    }
    close(my_pipe[0]);
    close(my_pipe[1]);
    string command = string(args1[0]);
    if (execvp(command.c_str(), args1) == SYS_FAIL)
    {
      perror("smash error: evecvp failed");
      deleteArgs(args1);
      deleteArgs(args2);
      exit(0);
    }
  }
  else
  {
    if (dup2(my_pipe[0], STDIN_FILENO) == SYS_FAIL)
    {
      perror("smash error: dup2 failed");
      exit(0);
    }
    close(my_pipe[0]);
    close(my_pipe[1]);
    string command = string(args2[0]);
    if (execvp(command.c_str(), args2) == SYS_FAIL)
    {
      perror("smash error: evecvp failed");
      deleteArgs(args1);
      deleteArgs(args2);
      exit(0);
    }
  }
}

//------------------------------------------------Chmod----------------------------------------------------------------

ChmodCommand::ChmodCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ChmodCommand::execute()
{
  int permissionsNum;
  int numArgs;
  char **args = getArgs(this->m_cmd_line, &numArgs);
  if (numArgs != 3)
  {
    cerr << "smash error: chmod: invalid arguments" << endl;
    deleteArgs(args);
    return;
  }
  if (!is_number(args[1]))
  {
    cerr << "smash error: chmod: invalid arguments" << endl;
    deleteArgs(args);
    return;
  }
  permissionsNum = stoi(args[1], nullptr, 8);
  if ((permissionsNum < 0 || permissionsNum > 777) && !(permissionsNum < 4777 && permissionsNum > 4000))
  {
    cerr << "smash error: chmod: invalid arguments" << endl;
    deleteArgs(args);
    return;
  }
  if (chmod(args[2], permissionsNum) == SYS_FAIL)
  {
    deleteArgs(args);
    perror("smash error: chmod failed");
    return;
  }
  deleteArgs(args);
}

//-------------------------------------SmallShell-------------------------------------

pid_t SmallShell::m_pid = getpid();

SmallShell::SmallShell(std::string prompt) : m_pid_fg(0), m_prompt(prompt)
{
  m_prevDir = (char *)malloc((MAX_PATH_LENGTH + 1) * sizeof(char));
  if (m_prevDir == nullptr)
  {
    free(m_prevDir);
    cerr << "smash error: malloc failed" << endl;
    return;
  }
  strcpy(m_prevDir, "");
  m_currDir = (char *)malloc((MAX_PATH_LENGTH + 1) * sizeof(char));
  if (m_currDir == nullptr)
  {
    free(m_currDir);
    cerr << "smash error: malloc failed" << endl;
    return;
  }
  strcpy(m_currDir, "");
}

SmallShell::~SmallShell()
{
  free(m_prevDir);
  free(m_currDir);
}

Command *SmallShell::CreateCommand(const char *cmd_line)
{
  if (string(cmd_line).empty())
  {
    return nullptr;
  }
  // Check if command is an IO redirection:
  SmallShell &shell = SmallShell::getInstance();
  if (strstr(cmd_line, ">") != nullptr || strstr(cmd_line, ">>") != nullptr)
  {
    int stat = 0;
    pid_t pid = fork();
    if (pid < 0)
    {
      perror("smash error: fork failed");
      return nullptr;
    }
    else if (pid > 0)
    {
      shell.m_pid_fg = pid;
      pid = waitpid(pid, &stat, WUNTRACED);
      if (pid == SYS_FAIL)
      {
        perror("smash error: waitpid failed");
        return nullptr;
      }
      shell.m_pid_fg = 0;
      return nullptr;
    }
    else
    {
      if (setpgrp() == SYS_FAIL)
      {
        perror("smash error: setpgrp failed");
        return nullptr;
      }
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
  if (strchr(cmd, '|'))
  {
    pid_t pid = fork();
    if (pid < 0)
    {
      perror("smash error: fork failed");
      return nullptr;
    }
    else if (pid > 0)
    {
      int status;
      shell.m_pid_fg = pid;
      pid = waitpid(pid, &status, WUNTRACED);
      if (pid == SYS_FAIL)
      {
        perror("smash error: waitpid failed");
        return nullptr;
      }
      shell.m_pid_fg = 0;
      return nullptr;
    }
    else
    {
      if (setpgrp() == SYS_FAIL)
      {
        perror("smash error: setpgrp failed");
        return nullptr;
      }
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
      if (pid == SYS_FAIL)
      {
        perror("smash error: waitpid failed");
        return nullptr;
      }
      shell.m_pid_fg = 0;
      return nullptr;
    }
    if (pid == 0)
    {
      if (setpgrp() == SYS_FAIL)
      {
        perror("smash error: setpgrp failed");
        return nullptr;
      }
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
  Command *cmd = CreateCommand(cmd_line);
  if (cmd == nullptr)
  {
    delete cmd;
    return;
  }
  cmd->execute();
  if (dynamic_cast<QuitCommand *>(cmd) != nullptr)
  {
    delete cmd;
    exit(0);
  }
  delete cmd;
}

void SmallShell::chngPrompt(const std::string newPrompt)
{
  m_prompt = newPrompt;
}

std::string SmallShell::getPrompt() const
{
  return m_prompt;
}

char *SmallShell::getCurrDir() const
{
  return m_currDir;
}

void SmallShell::setCurrDir(char *currDir, char *toCombine)
{
  if (toCombine == nullptr)
  {
    strcpy(m_currDir, currDir);
    return;
  }
  int length = string(currDir).length() + string(toCombine).length() + 2;
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
  strcpy(m_currDir, temp);
  free(temp);
}

char *SmallShell::getPrevDir() const
{
  return m_prevDir;
}

void SmallShell::setPrevDir()
{
  strcpy(m_prevDir, m_currDir);
}
