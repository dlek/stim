#include "stim.hh"


// -----------------------------------------------------------------------
// TEMPORARY
// -----------------------------------------------------------------------

#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unistd.h>
using std::vector;


// Ensure Stim environment exists and is useful
int EnsureStimEnvironment(const char* szHomeDir, const char* szLogFile, bool bInitialise)
{ 
  struct stat sb;
  string s;

  // check that home directory exists
  s = szHomeDir;
  if (stat(szHomeDir, &sb) == 0)
  { 
    // check whether it's a directory
    if (!S_ISDIR(sb.st_mode))
    { 
      // we can't do anything with this
      throw(s + " exists but is not a directory");
    }
  }
  else
  {
    if (bInitialise)
    { 
      // create directory
      if (mkdir(szHomeDir, 0700) != 0)
      { 
        string s = szHomeDir;
        throw("mkdir " + s + " failed");
      }
    }
    else
    {
      throw(s + " does not exist");
    }
  }

  // check that the log file exists
  s = szLogFile;
  if (stat(szLogFile, &sb) == 0)
  {
    // check whether it's a readable/writable file
    if (access(szLogFile, R_OK | W_OK) != 0)
    {
      // we can't do anything with this
      throw(s + " cannot be read and written");
    }
  }
  else
  {
    if (bInitialise)
    {
      // create file
      if (mknod(szLogFile, S_IFREG | 0600, 0) != 0)
      {
        throw(s + " could not be created");
      }
    }
    else
    {
      throw(s + " does not exist");
    }
  }
}


vector<string> split(const string& sSplittee, char cSplitter)
{
    int iIndex, iLast = 0;
    vector<string> vChunks;

    // split off chunks until we can split no more
    while ((iIndex = sSplittee.find_first_of(cSplitter, iLast)) 
           != string::npos)
    {
        vChunks.push_back(sSplittee.substr(iLast, iIndex-iLast));
        iLast = iIndex + 1;
    }
    vChunks.push_back(sSplittee.substr(iLast));

    return vChunks;
}


bool ReadNextLine(fstream& fTextFile, string& sLine)
{
    // create line buffer and read next line into it
    std::stringbuf sLineBuffer;
    fTextFile.get(sLineBuffer);

    // load string with string contents of line buffer
    sLine = sLineBuffer.str();

    // current character is either a newline, or EOF
    if (fTextFile.peek() != EOF)
    {
        // advance input stream past newline
        fTextFile.seekg(sizeof(char), ios::cur);
    }

    // return whether there's any more data
    return (fTextFile.peek() != EOF);
}


void SecondsToHms(int iSeconds, string& sHms)
{
    // break down seconds into hours, minutes, seconds
    int iMinutes = iSeconds / 60;
    iSeconds %= 60;
    int iHours = iMinutes / 60;
    iMinutes %= 60;

    // build into string of format HH:MM:SS
    char szHms[9];
    sprintf(szHms, "%02d:%02d:%02d", iHours, iMinutes, iSeconds);
    sHms = szHms;
}


// create sortable timestamp from given time
// szDate should be allocated at least 18 characters
void GkMakeTimestamp(time_t aTime, char* szDate)
{
    // convert to struct we can examine
    struct tm* pTm = localtime(&aTime); 
    
    // build time string in the format "20041027 00:26:23"
    sprintf(szDate,"%4d%02d%02d %02d:%02d:%02d", 
            pTm->tm_year + 1900,
            pTm->tm_mon + 1,
            pTm->tm_mday,
            pTm->tm_hour,
            pTm->tm_min,
            pTm->tm_sec);
}


void GkGrokTimestamp(time_t& aTime, const char* szDate)
{
    // break down timestamp
    int iYear, iMonth, iDay, iHour, iMinute, iSecond;
    sscanf(szDate, "%04d%02d%02d %02d:%02d:%02d",
           &iYear,
           &iMonth,
           &iDay,
           &iHour,
           &iMinute,
           &iSecond);
    
    // convert timestamp into calendar time
    struct tm tTm;
    tTm.tm_hour = iHour;
    tTm.tm_min = iMinute;
    tTm.tm_sec = iSecond;
    tTm.tm_mday = iDay;
    tTm.tm_mon = (iMonth - 1);
    tTm.tm_year = (iYear - 1900);
    tTm.tm_isdst = -1;
/* struct tm tTm =  
    { 
        iSecond, iMinute, iHour, 
        iDay, (iMonth - 1), (iYear - 1900) 
        };*/
    aTime = mktime(&tTm);
    if (aTime < 0)
        throw "Stim::GkGrokTimestamp: mktime() returned -1!";
}


// -----------------------------------------------------------------------
//                                        INITIALISATION AND DESTRUCTION
// -----------------------------------------------------------------------


Stim::Stim(const char* szStimName, const char* szContract, bool bInitialise)
{
    m_sStimDir = szStimName;
    m_sContract = szContract;
    m_bInitialise = bInitialise;

    // determine file names
    m_sStimLog = m_sStimDir + "/" + m_sContract + ".log";

    // basic initialisation
    m_bLogReadOnly = true;

    Stim::Trace(("Log file: " + m_sStimLog).c_str());
}


Stim::~Stim(void)
{
    // destroy self
    this->Destroy();
}


void Stim::Initialise(void)
{
    Stim::Trace("Initialising Stim (trace on)");

    // if we're in an initialisation mode, ensure the home environment is set up
    EnsureStimEnvironment(m_sStimDir.c_str(), m_sStimLog.c_str(), m_bInitialise);

    // open the log document
    m_fLog.open(m_sStimLog.c_str(), ios::in);
    if (!m_fLog)
        throw ("Failed to open log file: " + m_sStimLog).c_str();

    // only opening for reads by default
    m_bLogReadOnly = true;
}


void Stim::Destroy(void)
{
    Stim::Trace("Destroying Stim");

    // close log file
    m_fLog.close();
}



void Stim::WriteLog(
    const string& sTimestamp,
    const string& sEvent, 
    const string& sDetail = "")
{
    // make sure containers are initialised
    this->Initialise();

    // ensure log is open in read/write
    if (m_bLogReadOnly)
    {
        // TODO: would be nice if there was a better way to do this
        // TODO: verify open worked
        m_fLog.close();
        m_fLog.clear();
        m_fLog.open(m_sStimLog.c_str(), ios::in | ios::out);
        m_bLogReadOnly = false;
    }

    // record event to log file
    m_fLog.seekp(0, ios::end);
    m_fLog << sTimestamp << " " << sEvent;
    if (sDetail.length() > 0)
        m_fLog << " " << sDetail;
    m_fLog << endl;
}


bool Stim::ReadLog(
    string& sTimestamp, 
    string& sEvent, 
    string& sDetail)
{
    Stim::Trace("Beginning of ReadLog");

    // read next line
    string sLogLine; 
    bool bMoreData = ReadNextLine(this->m_fLog, sLogLine);

    // parse out timestamp
    sTimestamp = sLogLine.substr(0, 17);

    // parse out event
    int iEndOfEvent = sLogLine.find_first_of(' ', 18);
    int iEventLength = iEndOfEvent - 18;
    sEvent = sLogLine.substr(18, iEventLength);

    // the rest is detail
    sDetail = sLogLine.substr(iEndOfEvent + 1);

    Stim::Trace("Leaving ReadLog");

    return bMoreData;
}

void Stim::StartTask(time_t aStartTime, const string& sTaskPath)
{
    char szDate[18];
    GkMakeTimestamp(aStartTime, szDate);
    WriteLog(szDate, STIM_TASK_START, sTaskPath);
}


void Stim::StopTask(time_t aStartTime)
{
    char szDate[18];
    GkMakeTimestamp(aStartTime, szDate);
    WriteLog(szDate, STIM_TASK_STOP);
}


void Stim::LogTask(time_t aStartTime, const string& sMessage)
{
    char szDate[18];
    GkMakeTimestamp(aStartTime, szDate);
    WriteLog(szDate, STIM_TASK_LOG, sMessage);
}


// cribbed from "Thinking in C++", 2nd Ed., Vol. 2
int stringcmpi(const string& s1, const string& s2) 
{
    // select the first element of each string
    string::const_iterator p1 = s1.begin(), p2 = s2.begin();

    // iterate through characters in each string
    while(p1 != s1.end() && p2 != s2.end())
    {
        // convert each to uppercase and compare
        if(toupper(*p1) != toupper(*p2))
        {
            // report which was lexically greater
            return (toupper(*p1)<toupper(*p2))? -1 : 1;
        }

        // move on
        p1++;
        p2++;
    }

    // they either match up exactly, or one is longer than the other and 
    // they match up to the end of the shorter.  In the latter case, say
    // which was longer.
    return(s2.size() - s1.size());
}


time_t DetermineStartOfDay(time_t aTime)
{
    // convert to struct we can examine
    struct tm* pTm = localtime(&aTime); 
    
    // clear out hour, minute, second
    pTm->tm_hour = pTm->tm_min = pTm->tm_sec = 0;

    // convert back into timestamp
    time_t aMidnight = mktime(pTm);
    if (aMidnight < -1)
        throw "mktime() returned -1";

    return aMidnight;
}


time_t DetermineStartOfDay(const string& sDate)
{
    // break down datestamp
    int iYear, iMonth, iDay;
    sscanf(sDate.c_str(), "%04d%02d%02d", &iYear, &iMonth, &iDay);
    
    // convert datestamp into calendar time
    struct tm tTm;
    tTm.tm_hour = 0;
    tTm.tm_min = 0;
    tTm.tm_sec = 0;
    tTm.tm_mday = iDay;
    tTm.tm_mon = (iMonth - 1);
    tTm.tm_year = (iYear - 1900);
/*    struct tm tTm = 
    { 
        0, 0, 0, 
        iDay, (iMonth - 1), (iYear - 1900) 
        };*/
    time_t aTime = mktime(&tTm);
    if (aTime < 0)
        throw "Stim::GkGrokTimestamp: mktime() returned -1!";

    return aTime;
}


bool stringtrim(string& s)
{
    s.erase(0, s.find_first_not_of(" \t"));
    s.erase(s.find_last_not_of(" \t") + 1);
    if (s.length() == 0)
        return false;
    return true;
}


void DeterminePeriod(
    const string& sDateRange, 
    time_t& aPeriodStart, 
    time_t& aPeriodEnd)
{
    if (stringcmpi(sDateRange, STIM_DATE_TODAY) == 0)
    {
        aPeriodStart = DetermineStartOfDay(time(0));
        aPeriodEnd = aPeriodStart + SECONDS_IN_DAY - 1;
    }
    else if (stringcmpi(sDateRange, STIM_DATE_YESTERDAY) == 0)
    {
        aPeriodStart = DetermineStartOfDay(time(0) - SECONDS_IN_DAY);
        aPeriodEnd = aPeriodStart + SECONDS_IN_DAY - 1;        
    }
    else // format is "<date>", "-<date>", "<date>-", "<date1>-<date2>", or ""
    {
        int iDash = sDateRange.find('-');
        if (iDash == string::npos)
        {
            // one day specified
            aPeriodStart = DetermineStartOfDay(sDateRange);
            aPeriodEnd = aPeriodStart + SECONDS_IN_DAY - 1;
        }
        else
        {
            // split into "<date1>-<date2>"
            string sStart = sDateRange.substr(0, iDash);
            string sEnd = sDateRange.substr(iDash + 1);

            // if start specified
            if (stringtrim(sStart))
                aPeriodStart = DetermineStartOfDay(sStart);
            else
                aPeriodStart = 0;

            // if end specified
            if (stringtrim(sEnd))
                aPeriodEnd = DetermineStartOfDay(sEnd) + SECONDS_IN_DAY - 1;
            else
                aPeriodEnd = time(0);
        }
    }
}


bool Stim::FindPeriodStart(time_t aPeriodStart, time_t aPeriodEnd)
{
    // seek to beginning of log file
    this->m_fLog.seekg(0, ios::beg);

    // and here we have a linear search, folks
    string sTimestamp, sEvent, sDetail;
    time_t aTime;
    bool bMoreLog = true;
    while (bMoreLog)
    {
        // get next line
        streampos tLogPos = this->m_fLog.tellg();      
        bMoreLog = ReadLog(sTimestamp, sEvent, sDetail);

        // compare timestamp
        GkGrokTimestamp(aTime, sTimestamp.c_str());
        if (aTime >= aPeriodStart)
        {
            // check that we haven't overshot
            if (aTime >= aPeriodEnd)
                return false;
            
            // rewind to beginning of record
            this->m_fLog.clear(); // this is necessary to reset the eof bit
            this->m_fLog.seekg(tLogPos);

            return true;            
        }
    }

    return false;
}


void AddToTaskTotals(
    map<string, time_t>& vPeriodTime, 
    string sTask, 
    time_t tTimeSpent)
{
    map<string, time_t>::iterator it;

    // first time with this task, this period?
    if ((it = vPeriodTime.find(sTask)) == vPeriodTime.end())
        vPeriodTime[sTask] = tTimeSpent;
    else
        vPeriodTime[sTask] += tTimeSpent;
}


time_t GetTotalTime(map<string, time_t>& vTaskTime)
{
    map<string, time_t>::iterator it;
    time_t tTotal = 0;

    for (it = vTaskTime.begin(); 
         it != vTaskTime.end(); 
         it++)
    {
        tTotal += it->second;
    }

    return tTotal;
}

void PrintOutTotals(const string& sStart, map<string, time_t>& vTaskTime)
{
    map<string, time_t>::iterator it;
    time_t tTotal = 0;

    for (it = vTaskTime.begin(); 
         it != vTaskTime.end(); 
         it++)
    {
        time_t tSeconds = it->second;
        string sElapsed;
        SecondsToHms(tSeconds, sElapsed);
        printf("%-60s  %s\n", it->first.c_str(), sElapsed.c_str());
        tTotal += it->second;
    }

    string sTotalElapsed;
    SecondsToHms(tTotal, sTotalElapsed);
    printf("%60s  %s\n", "TOTAL", sTotalElapsed.c_str());
}


bool Stim::Status(TSessionStatus& tSession)
{
    // make sure containers are initialised
    this->Initialise();

    // determine period for reporting
    string sDateRange = "today";
    time_t aPeriodStart, aPeriodEnd;
    DeterminePeriod(sDateRange, aPeriodStart, aPeriodEnd);

    // seek to beginning of range
    /* TODO: this should seek to beginning of session; it currently seeks
     * to first entry of today.  Consider working late and starting a new 
     * session at 11:45.  Work starts on a new task at 1:40 in the morning,
     * but it's still the same session.  Status will report only on the
     * later task.
     */
    Stim::Trace("Seeking to beginning of range");
    if (!FindPeriodStart(aPeriodStart, aPeriodEnd))
        return false;
    Stim::Trace("Found beginning of range.");

    // parse line by line
    string sLogLine;
    time_t tLastTime = STIM_TIME_NOTIME;
    string sLastTask;

    map<string, time_t> vSessionTime;
    string sSessionStartDate = ""; 
    string sTimestamp, sEvent, sDetail;
    bool bContinue = true;
    bool bRunning;
    while (bContinue)
    {
        Stim::Trace("here we are... about to read a log line...");

        // read the next record
        bContinue = ReadLog(sTimestamp, sEvent, sDetail);

        if (bContinue)
          Stim::Trace("finished reading log line (more to follow)");
        else
          Stim::Trace("finished reading log line (that's it)");

        // trace the line
        Stim::Trace(("LOG>" 
            + sTimestamp + " > " + sEvent + " > " + sDetail).c_str());

        // skip log messages
        if (sEvent == STIM_TASK_LOG)
          continue;

        // parse timestamp
        time_t tTimeStamp;
        GkGrokTimestamp(tTimeStamp, sTimestamp.c_str());

        // if starting a new session
        if (tLastTime == STIM_TIME_NOTIME)
        {
            // reports don't distinguish between sessions
            // nothing to report, just go on to next task
            tLastTime = tTimeStamp;
            sLastTask = sDetail;
            continue;
        }

        // otherwise, was working on a task, done with it for now
        else
        {
            // calculate time difference
            time_t tTimeDiff = tTimeStamp - tLastTime;

            // add to task totals
            AddToTaskTotals(vSessionTime, sLastTask, tTimeDiff);
        }

        // starting or stopping?
        if (sEvent == STIM_TASK_START)
        { 
            Stim::Trace("Starting new task");

            // this task becomes the previous task
            sLastTask = sDetail;
        }
        else if (sEvent == STIM_TASK_STOP)
        {
            Stim::Trace("Stopping task");

            // if it were true, you'd better catch it
            bRunning = false;
        }

        // this is the start time of the current time chunk
        tLastTime = tTimeStamp;
    }

    // determine total session time, excluding current task
    time_t tTotalTime = 0;
    if (!vSessionTime.empty())
    {
        tTotalTime = GetTotalTime(vSessionTime);
    }

    // fill out struct
    tSession.aSessionTime = tTotalTime;
    tSession.aTaskTime = vSessionTime[sLastTask];
    tSession.aTransitionTime = tLastTime;
    tSession.sCurrentTask = sLastTask;
    tSession.bRunning = bRunning;

    return true;
}


bool Stim::ReportTime(
  const string& sDateRange, 
  vector<string>& vTaskPaths,
  TTimeSpent& vTimeSpent)
{
    // make sure containers are initialised
    this->Initialise();

    // determine period for reporting
    time_t aPeriodStart, aPeriodEnd;
    DeterminePeriod(sDateRange, aPeriodStart, aPeriodEnd);

    // seek to beginning of range
    if (!FindPeriodStart(aPeriodStart, aPeriodEnd))
        return false;

    // parse line by line
    string sLogLine;
    time_t tLastTime = STIM_TIME_NOTIME;
    string sLastTask;

    map<string, time_t> vSessionTime, vPeriodTime;
    string sSessionStartDate = "";
    string sTimestamp, sEvent, sDetail;
    bool bContinue = true;
    TTimeChunk tCurrentTimeChunk;
    while (bContinue)
    {
        // read the next record
        bContinue = ReadLog(sTimestamp, sEvent, sDetail);

        // trace the line
        Stim::Trace(("LOG>" 
            + sTimestamp + " > " + sEvent + " > " + sDetail).c_str());

        // parse timestamp
        time_t tTimeStamp;
        GkGrokTimestamp(tTimeStamp, sTimestamp.c_str());

        // if new chunk of time
        if (tCurrentTimeChunk.aStartTime == STIM_TIME_NOTIME)
        {
          // check if this is outside of period bounds
          if (tTimeStamp > aPeriodEnd)
            break;

          // new time; new session?
          if (sEvent == STIM_TASK_START)
          {
            tCurrentTimeChunk.aStartTime = tTimeStamp;
            tCurrentTimeChunk.sTaskPath = sDetail;
          }
          //else
          //  throw "Stim::ReportTime: somehow continued nonexistent session";
        }

        // more records for current chunk of time
        else
        {
          // are we logging something for the task?
          if (sEvent == STIM_TASK_LOG)
          {
            TLogEntry tLogEntry = { tTimeStamp, sDetail };
            tCurrentTimeChunk.vLogMessages.push_back(tLogEntry);
          }
          else // assume we're stopping (or starting a new task)
          {
            // assign stop time
            tCurrentTimeChunk.aStopTime = tTimeStamp;

            // add time record to vector thereof
            vTimeSpent.push_back(tCurrentTimeChunk);

            // reset time record
            tCurrentTimeChunk.vLogMessages.clear();
            if (sEvent == STIM_TASK_START)
            {
              tCurrentTimeChunk.aStartTime = tTimeStamp;
              tCurrentTimeChunk.sTaskPath = sDetail;
            }
            else
              tCurrentTimeChunk.aStartTime = STIM_TIME_NOTIME;
          }
        }
    }

    // check if we've logged time
    return !(vTimeSpent.empty());
}

// -----------------------------------------------------------------------
//                                                               HELPERS
// -----------------------------------------------------------------------


void Stim::Trace(const char* szMessage)
{
#ifdef DEBUG
    cerr << "STIM>" << szMessage << endl;
#endif
}

