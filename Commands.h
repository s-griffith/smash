#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string.h>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define MAX_PATH_LENGTH (80)

/*
 *  Command Class:
 *  This class represents a single Command of SmallShell.
 */
class Command
{
public:
  /*
   * Constructor of Command class
   * @param cmd_line - the given CMD line
   * @return
   *      A new instance of Command.
   */
  Command(const char *cmd_line);

  /*
   * Destructor of the Command class
   */
  virtual ~Command();

  /*
   * Execute function of the Command class:
   * Executes the command based on its type according to the appropriate child class.
   * Receives no parameters
   * @return
   *      void
   */
  virtual void execute() = 0;

protected:
  /*
   * The internal field associated with a Command:
   * m_cmd_line: The CMD line received from the user
   */
  const char *m_cmd_line;
};

/*
 *  BuiltInCommand Class:
 *  This class represents a built-in Command of SmallShell.
 */
class BuiltInCommand : public Command
{
public:
  /*
   * Constructor of BuiltInCommand class
   * @param cmd_line - the given CMD line
   * @return
   *      A new instance of BuiltInCommand.
   */
  BuiltInCommand(const char *cmd_line);

  /*
   * Destructor of the BuiltInCommand class
   */
  virtual ~BuiltInCommand() = default;
};

/*
 *  ExternalCommand Class:
 *  This class represents an external Command of SmallShell.
 */
class ExternalCommand : public Command
{
public:
  /*
   * Constructor of ExternalCommand class
   * @param cmd_line - the given CMD line
   * @return
   *      A new instance of ExternalCommand.
   */
  ExternalCommand(const char *cmd_line);

  /*
   * Destructor of the ExternalCommand class
   */
  virtual ~ExternalCommand() {}

  /*
   * Execute function of the ExternalCommand class:
   * Executes the external command.
   * Receives no parameters
   * @return
   *      void
   */
  void execute() override;
};

/*
 *  PipeCommand Class:
 *  This class represents a pipe Command of SmallShell.
 */
class PipeCommand : public Command
{
public:
  /*
   * Constructor of PipeCommand class
   * @param cmd_line - the given CMD line
   * @return
   *      A new instance of PipeCommand.
   */
  PipeCommand(const char *cmd_line);

  /*
   * Destructor of the PipeCommand class
   */
  virtual ~PipeCommand() {}

  /*
   * Execute function of the PipeCommand class:
   * Executes the pipe command.
   * Receives no parameters
   * @return
   *      void
   */
  void execute() override;
};

/*
 *  RedirectionCommand Class:
 *  This class represents a redirection Command of SmallShell.
 */
class RedirectionCommand : public Command
{
public:
  /*
   * Constructor of RedirectionCommand class
   * @param cmd_line - the given CMD line
   * @return
   *      A new instance of RedirectionCommand.
   */
  explicit RedirectionCommand(const char *cmd_line);

  /*
   * Destructor of the RedirectionCommand class
   */
  virtual ~RedirectionCommand() {}

  /*
   * Execute function of the RedirectionCommand class:
   * Executes the redirection command.
   * Receives no parameters
   * @return
   *      void
   */
  void execute() override;
};

/*
 *  ChangeDirCommand Class:
 *  This class represents a cd Command of SmallShell.
 */
class ChangeDirCommand : public BuiltInCommand
{
public:
  /*
   * Constructor of ChangeDirCommand class
   * @param cmd_line - the given CMD line
   * @param plastPwd - the previous directory
   * @return
   *      A new instance of ChangeDirCommand.
   */
  ChangeDirCommand(const char *cmd_line, char **plastPwd);

  /*
   * Destructor of the ChangeDirCommand class
   */
  virtual ~ChangeDirCommand() {}

  /*
   * Execute function of the ChangeDirCommand class:
   * Executes the change directory command.
   * Receives no parameters
   * @return
   *      void
   */
  void execute() override;

private:
  /*
   * The internal field associated with a ChangeDirCommand:
   * m_plastPwd: The last directory (last PWD)
   */
  char **m_plastPwd;
};

/*
 *  GetCurrDirCommand Class:
 *  This class represents a pwd Command of SmallShell.
 */
class GetCurrDirCommand : public BuiltInCommand
{
public:
  /*
   * Constructor of GetCurrDirCommand class
   * @param cmd_line - the given CMD line
   * @return
   *      A new instance of GetCurrDirCommand.
   */
  GetCurrDirCommand(const char *cmd_line);

  /*
   * Destructor of the GetCurrDirCommand class
   */
  virtual ~GetCurrDirCommand() {}

  /*
   * Execute function of the GetCurrDirCommand class:
   * Executes the get current directory (pwd) command.
   * Receives no parameters
   * @return
   *      void
   */
  void execute() override;
};

/*
 *  ShowPidCommand Class:
 *  This class represents a showpid Command of SmallShell.
 */
class ShowPidCommand : public BuiltInCommand
{
public:
  /*
   * Constructor of ShowPidCommand class
   * @param cmd_line - the given CMD line
   * @return
   *      A new instance of ShowPidCommand.
   */
  ShowPidCommand(const char *cmd_line);

  /*
   * Destructor of the ShowPidCommand class
   */
  virtual ~ShowPidCommand() {}

  /*
   * Execute function of the ShowPidCommand class:
   * Executes the showpid command.
   * Receives no parameters
   * @return
   *      void
   */
  void execute() override;
};

/*
 *  ChangePromptCommand Class:
 *  This class represents a chprompt Command of SmallShell.
 */
class ChangePromptCommand : public BuiltInCommand
{
public:
  /*
   * Constructor of ChangePromptCommand class
   * @param cmd_line - the given CMD line
   * @return
   *      A new instance of ChangePromptCommand.
   */
  explicit ChangePromptCommand(const char *cmd_line);

  /*
   * Destructor of the ChangePromptCommand class
   */
  virtual ~ChangePromptCommand();

  /*
   * Execute function of the ChangePromptCommand class:
   * Executes the chprompt command.
   * Receives no parameters
   * @return
   *      void
   */
  void execute() override;
};

/*
 *  JobsList Class:
 *  This class represents the list of jobs in SmallShell.
 */
class JobsList;

/*
 *  QuitCommand Class:
 *  This class represents a quit Command of SmallShell.
 */
class QuitCommand : public BuiltInCommand
{
public:
  /*
   * Constructor of QuitCommand class
   * @param cmd_line - the given CMD line
   * @param jobs - the list of jobs in SmallShell
   * @return
   *      A new instance of QuitCommand.
   */
  QuitCommand(const char *cmd_line, JobsList *jobs);

  /*
   * Destructor of the QuitCommand class
   */
  virtual ~QuitCommand() {}

  /*
   * Execute function of the QuitCommand class:
   * Executes the quit command.
   * Receives no parameters
   * @return
   *      void
   */
  void execute() override;

private:
  /*
   * The internal field associated with a QuitCommand:
   * m_jobs: The list of jobs in SmallShell
   */
  JobsList *m_jobs;
};

/*
 *  JobsList Class:
 *  This class represents the list of jobs in SmallShell.
 */
class JobsList
{
private:
  /*
   *  JobEntry Class:
   *  This class represents a job in the list of jobs in SmallShell.
   */
  class JobEntry
  {
  public:
    int m_id;
    pid_t m_pid;
    char m_cmd[COMMAND_ARGS_MAX_LENGTH + 1];
    bool m_isStopped;
    JobEntry() {}
    JobEntry(int id, pid_t pid, const char *cmd, bool isStopped = false);
    ~JobEntry() = default;
  };

private:
  std::vector<JobEntry> m_list;
  int max_id = 0;
  // TODO: Add your data members
public:
  JobsList() = default;
  ~JobsList() = default;
  void addJob(const char *cmd, pid_t pid, bool isStopped = false);
  void printJobsList();
  void killAllJobs();
  void removeFinishedJobs();
  JobEntry *getJobById(int jobId);
  void sigJobById(int jobId, int signum);
  void removeJobById(int jobId);
  JobEntry *getLastJob(int *lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
  bool isEmpty();
  int getMaxId();
  // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand
{
  // TODO: Add your data members
public:
  JobsCommand(const char *cmd_line);
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand
{
  JobsList *m_jobs;
  // TODO: Add your data members
public:
  KillCommand(const char *cmd_line, JobsList *jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand
{
  // TODO: Add your data members
  JobsList *m_jobs;

public:
  ForegroundCommand(const char *cmd_line, JobsList *jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class ChmodCommand : public BuiltInCommand
{
public:
  ChmodCommand(const char *cmd_line);
  virtual ~ChmodCommand() {}
  void execute() override;
};

class SmallShell
{
private:
  std::string m_prompt;
  char *m_prevDir;
  /// TODO: MIGHT MAKE PROBLEMS LATER ON. IF DOES, GET RID OF FLAG AND RUN SYSCALL EVERY TIME FOR PWD
  char *m_currDirectory;

  SmallShell(const std::string prompt = "smash");

  JobsList jobs;

public:
  static pid_t m_pid;

  int m_pid_fg = 0;

  Command *CreateCommand(const char *cmd_line);

  SmallShell(SmallShell const &) = delete; // disable copy ctor

  void operator=(SmallShell const &) = delete; // disable = operator

  static SmallShell &getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  JobsList *getJobs();
  void executeCommand(const char *cmd_line);
  void chngPrompt(const std::string newPrompt = "smash");
  std::string getPrompt() const;
  char *getCurrDir() const;
  void setCurrDir(char *currDir, char *toCombine = nullptr);
  char *getPrevDir() const;
  void setPrevDir(char *prevDir);
};

#endif // SMASH_COMMAND_H_
