#ifndef _STIM_HH_
#define _STIM_HH_

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <sys/stat.h>


//#define DEBUG


#define STIM_TASK_START "start"
#define STIM_TASK_STOP  "stop"
#define STIM_TASK_LOG   "log"

#define STIM_DATE_TODAY "today"
#define STIM_DATE_YESTERDAY "yesterday"

#define STIM_TIME_NOTIME -1


#define SECONDS_IN_DAY (60 * 60 * 24)


using namespace std;


void GkMakeTimestamp(time_t aTime, char* szDate);
void SecondsToHms(int iSeconds, string& sHms);

struct TLogEntry
{
  time_t aLogTime;
  string sLogMessage;
};

struct TTimeChunk
{
  time_t aStartTime;
  time_t aStopTime;
  string sTaskPath;
  vector<TLogEntry> vLogMessages;

  TTimeChunk(void)
  {
    aStartTime = STIM_TIME_NOTIME;
  }
};

typedef vector<TTimeChunk> TTimeSpent;


/*
 * TSessionStatus - for returning information about current session
 */
struct TSessionStatus
{
  time_t aSessionTime;    // session time excluding current work period
  time_t aTaskTime;       // task time for session excluding current work period
  time_t aTransitionTime; // start time of current work period
  string sCurrentTask;    // current task or last task worked on
  bool   bRunning;        // whether timer is currently running
};


class Stim
{
public:

    // static constructor
    static Stim* Create(const char* szConnection);

    // constructor, destructor
    Stim(const char* szStimName, const char* szContract);
    virtual ~Stim(void);

    // initialise environment
    virtual void Initialise(void);

    // start and stop tasks
    virtual void StartTask(
        time_t aTime, 
        const string& sTaskPath);
    virtual void StopTask(time_t aTime);
    virtual void LogTask(time_t aTime, const string &sMessage);

    // report time spent
    virtual bool Status(time_t tNow, TSessionStatus& tSession);
    virtual bool ReportTime(
        time_t tNow,
        const string& sDateRange, 
        vector<string>& vTaskPaths,
        TTimeSpent& vTimeSpent);

    static void Trace(const char* szMessage);

protected:

    // internal 
    virtual void EnsureInitialised(void);
    virtual void Destroy(void);
    virtual void WriteLog(
        const string& sTimestamp, 
        const string& sEvent, 
        const string& sDetail);
    virtual bool ReadLog(
        string& sTimestamp, 
        string& sEvent, 
        string& sDetail);
    virtual bool FindPeriodStart(
        time_t aPeriodStart,
        time_t aPeriodEnd);

private:

    string m_sStimDir;
    string m_sContract;
    bool m_bInitialise;

    // log file
    string m_sStimLog;
    fstream m_fLog;
    bool m_bLogReadOnly;
};


// handy helpers
void AddToTaskTotals(
    map<string, time_t>& vPeriodTime, 
    string sTask, 
    time_t tTimeSpent);
void PrintOutTotals(const string& sStart, map<string, time_t>& vTaskTime);


#endif // _STIM_HH_
