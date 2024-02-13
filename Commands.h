#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string.h>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define MAX_PATH_LENGTH (80)

class Command
{
protected:
  const char *m_cmd_line;

public:
  Command(const char *cmd_line);
  virtual ~Command();
  virtual void execute() = 0;
  const char *gedCmdLine();
  // virtual void prepare();
  // virtual void cleanup();
  //  TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command
{
public:
  BuiltInCommand(const char *cmd_line);
  virtual ~BuiltInCommand() = default;
};

class ExternalCommand : public Command
{
public:
  ExternalCommand(const char *cmd_line);
  virtual ~ExternalCommand() {}
  void execute() override;
};

class PipeCommand : public Command
{
  // TODO: Add your data members
public:
  PipeCommand(const char *cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command
{
  // TODO: Add your data members
public:
  explicit RedirectionCommand(const char *cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  // void prepare() override;
  // void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand
{
  char **m_plastPwd;

public:
  ChangeDirCommand(const char *cmd_line, char **plastPwd);
  virtual ~ChangeDirCommand() {}
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand
{
public:
  GetCurrDirCommand(const char *cmd_line);
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand
{
public:
  ShowPidCommand(const char *cmd_line);
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class ChangePromptCommand : public BuiltInCommand
{
public:
  explicit ChangePromptCommand(const char *cmd_line);
  virtual ~ChangePromptCommand();
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand
{
  // TODO: Add your data members public:
public:
  // const char* m_cmd_line;
  JobsList *m_jobs;
  QuitCommand(const char *cmd_line, JobsList *jobs);
  virtual ~QuitCommand() {}
  void execute() override;
};

class JobsList
{
public:
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
