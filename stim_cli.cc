#include "stim_cli.hh"
#include "stim.hh"


using std::string;
using std::vector;


const char g_szUsage[] =
"stim - Simple Task Information Manager\n"
"(c) Copyright 2003-2017 Drew Leske.\n\n"
"Usage: stim start <task>\n"
"       stim stop\n"
"       stim log <message>\n"
"       stim status [--raw]\n"
"       stim report <daterange> [taskpath...]\n";

/*
** Helper functions
*/

// interpret "32m20s" as a number of seconds
// This is a pretty dorky implementation
int interpret_relative_timespec(const char* timespec)
{
  int num[3];
  char ch[3];
  int seconds;

  seconds = 0;
  int items = sscanf(timespec, "%d%c%d%c%d%c", &num[0], &ch[0], &num[1], &ch[1], &num[2], &ch[2]);
  for (int i = 0; i < items / 2; i++)
  {
    switch(ch[i])
    {
      case 'h':
        seconds += num[i] * 3600;
        break;
      case 'm':
        seconds += num[i] * 60;
        break;
      case 's':
        seconds += num[i];
        break;
      default:
        throw "Invalid relative time specifications";
    }
  }

  return seconds;
}

time_t interpret_absolute_timespec(time_t now, const char* timespec)
{
  int year, month, day, hours, minutes, seconds;

  // initialize struct with current time as defaults
  struct tm* pTm = localtime(&now); 

  // get pointer to timespec so we can move it along
  char* bufptr = (char*) timespec;

  int items = sscanf(timespec, "%04d%02d%02d ", &year, &month, &day);
  if (items == 3)
  {
    // move buffer along nine characters in order to interpret time
    bufptr += sizeof(char) * 9;

    // record date in struct
    pTm->tm_mday = day;
    pTm->tm_mon = (month - 1);
    pTm->tm_year = (year - 1900); 
  }
  items = sscanf(bufptr, "%d:%02d:%02d", &hours, &minutes, &seconds);
  if (items == 3)
  {
    pTm->tm_hour = hours;
    pTm->tm_min = minutes;
    pTm->tm_sec = seconds;
  }
  else
  { // guess they didn't specify seconds
    items = sscanf(bufptr, "%d:%02d", &hours, &minutes);
    if (items == 2)
    {
      pTm->tm_hour = hours;
      pTm->tm_min = minutes;
    }
    else
      throw "Invalid absolute time specification";
  }

  // convert struct into timestamp
  return(mktime(pTm));
}


// interpret time spec, if provided, for logging operations; if not provided
// use current time
time_t interpret_timespec(time_t tNow, string sWhen)
{
  time_t tWhen;
  if (sWhen.empty())
    tWhen = tNow;
  else
  {
    if (sWhen.c_str()[0] == '-')
    { // relative time
      // TODO: fill this out
      // -30s means 30 sconds ago
      // -1h means 1 hour ago
      // -1m30s means one minute, 30 seconds ago
      time_t tBackWhen = interpret_relative_timespec(
        sWhen.c_str() + sizeof(char));

      // calculate back from now
      tWhen = tNow - tBackWhen;
    }
    else
    { // absolute time
      time_t tWhen = interpret_absolute_timespec(tNow, sWhen.c_str());
    }
  }

  return tWhen;
}


int main(int argc, char** argv)
{
    int iStatus;
    int iLoop;
    string sCommand;
    vector<string> vArgs;
    map<string, string> vOptions;
    string sStimDirectory;

    // assume the best
    iStatus = STIM_CLI_RETURN_SUCCESS;

    // check if we're using fake now for debugging or testing
    time_t tNow;
    const char* szFakeNow = getenv(STIM_ENV_FAKENOW);
    if (szFakeNow == NULL)
      tNow = time(0);
    else
      tNow = (time_t) atoi(szFakeNow);

    // set operating parameters from environment or from defaults
    // timestamp format
    const char* szTimestampFormat = getenv(STIM_ENV_TIMESTAMP_FORMAT);
    if (szTimestampFormat == NULL)
      szTimestampFormat = STIM_DEFAULT_TIMESTAMP_FORMAT;

    // reporting format
    const char* szReportFormat = getenv(STIM_ENV_REPORT_FORMAT);
    if (szReportFormat == NULL)
      szReportFormat = STIM_DEFAULT_REPORT_FORMAT;

    // log reporting format
    const char* szLogFormat = getenv(STIM_ENV_LOG_FORMAT);
    if (szLogFormat == NULL)
      szLogFormat = STIM_DEFAULT_LOG_FORMAT;

    // get parameters into vector of strings
    if (argc > 1)
    {
        sCommand = argv[1];
        for (iLoop = 2; iLoop < argc; iLoop++)
        {
          if (strncmp(argv[iLoop],"--", 2) == 0)
          {
            char* szValue = strstr(argv[iLoop], "=");
            if (szValue == NULL)
              vOptions[argv[iLoop]+2] = "yes";
            else
            {
              szValue++;
              char szOption[255];
              size_t iLen = strlen(argv[iLoop]) - strlen(szValue) - 3;
              strncpy(szOption, argv[iLoop]+2, iLen);
              szOption[iLen] = 0;
              vOptions[szOption] = szValue;
            }
          }
          else
            vArgs.push_back(argv[iLoop]);
        }
    }

    // determine stim directory
    const char* szStimHome = getenv(STIM_ENV_HOME);
    if (szStimHome)
    {
      sStimDirectory = szStimHome;
    }
    else
    {
      sStimDirectory = getenv("HOME");
      sStimDirectory += "/.stim";
    }

    // determine contract
    const char* szContract = getenv(STIM_ENV_CONTRACT);
    string sContract;
    if (szContract)
    {
      sContract = szContract;
    }
    else
    {
      sContract = "general";
    }

    // attempt to parse command
    try
    {
        if (sCommand == "" || sCommand == "help")
        {
            std::cout << g_szUsage << std::endl;
            exit(0);
        }
        else if (sCommand == "init")
        {
          // initialise Stim environment
          Stim cStim(sStimDirectory.c_str(), sContract.c_str());
          cStim.Initialise();
        }
        else
        {
          // ensure Stim environment
          Stim cStim(sStimDirectory.c_str(), sContract.c_str());

          // handle Stim command
          if (sCommand == "start")
          {
              // syntax: start <task>
              if (vArgs.size() != 1)
                  throw "Usage: start <task>";
              string sTask = vArgs[0];

              // specified time?
              time_t tWhen = interpret_timespec(tNow, vOptions["when"]);
              
              // now start the task
              cStim.StartTask(tWhen, sTask);
          }
          else if (sCommand == "stop")
          {
              // syntax: stop
              if (vArgs.size() > 0)
                  throw "Usage: stop";

              // specified time?
              time_t tWhen = interpret_timespec(tNow, vOptions["when"]);
              
              // now stop the current task
              cStim.StopTask(tWhen);
          }
          else if (sCommand == "log")
          {
              // syntax: log <message>
              if (vArgs.size() != 1)
                  throw "Usage: log <message>";
              string sMessage = vArgs[0];

              // specified time?
              time_t tWhen = interpret_timespec(tNow, vOptions["when"]);

              // now log the message
              cStim.LogTask(tWhen, sMessage);
          }
          else if (sCommand == "status")
          {
              // syntax: status [--raw]
              if (vArgs.size() > 0)
              {
                  throw "Usage: status";
              }
            
              // raw or readable?
              bool bRaw = false;
              if (!vOptions["raw"].empty())
                bRaw = true;

              // get status
              TSessionStatus tSession;
              bool bHaveResults = cStim.Status(tNow, tSession);

              // report
              if (bRaw)
              {
                if (!bHaveResults)
                {
                  cout << "0 0 -1 stopped Nothing" << endl;
                  iStatus = STIM_CLI_RETURN_NO_RESULTS;
                }
                else
                {
                  cout 
                    << tSession.aSessionTime << " " 
                    << tSession.aTaskTime << " " 
                    << tSession.aTransitionTime 
                    << (tSession.bRunning ? " running " : " stopped ")
                    << tSession.sCurrentTask << endl; 
                }
              }
              else
              {
                if (!bHaveResults)
                {
                  cout << "Nothing today." << endl;
                  iStatus = STIM_CLI_RETURN_NO_RESULTS;
                }
                else
                {
                  // unlike the "raw" output, the human-readable output 
                  // calculates the session time with the current task's time
                  // included
                  time_t aElapsedSinceTransition = tNow - tSession.aTransitionTime;
                  // make session and task times readable
                  string sSessionTime;
                  string sTaskTime;
                  string sTimerStatus;
                  if (tSession.bRunning)
                  {
                    SecondsToHms(tSession.aSessionTime + aElapsedSinceTransition, 
                      sSessionTime);
                    SecondsToHms(tSession.aTaskTime + aElapsedSinceTransition, 
                      sTaskTime);

                    sTimerStatus = "running";
                  }
                  else
                  {
                    SecondsToHms(tSession.aSessionTime, sSessionTime);
                    SecondsToHms(tSession.aTaskTime, sTaskTime);
                    SecondsToHms(aElapsedSinceTransition, sTimerStatus);
                    sTimerStatus.insert(0, "stopped for ");
                  }

                  // echo session and task times, timer state and current or last task
                  cout
                    << "Session time: " << sSessionTime << std::endl
                    << "Task time:    " << sTaskTime << std::endl
                    << "Task:         " << tSession.sCurrentTask  << std::endl
                    << "Timer is      " << sTimerStatus
                    << endl;
                }
              }
          }
          else if (sCommand == "report")
          {
            // syntax: report <daterange> [taskpath...]
            if (vArgs.size() == 0)
                throw "Usage: report <daterange> [taskpath...]";
            string sDateRange = vArgs[0];

            // get optional task paths
            vector<string> vTaskPaths;
            if (vArgs.size() > 1)
            {
                // task path vector = vArgs[1]..vArgs[N]
                vTaskPaths.assign(vArgs.begin() + 1, vArgs.end());
            }

            // get time spent
            TTimeSpent vTimeSpent;
            if (!cStim.ReportTime(tNow, sDateRange, vTaskPaths, vTimeSpent))
            {
                std::cerr << "Nothing to report." << std::endl;
                iStatus = STIM_CLI_RETURN_NO_RESULTS;
            }

            // iterate through results
            TTimeSpent::iterator it3;
            map<string, string>::iterator it4;
            vector<TLogEntry>::iterator it5;
            char szStartTimestamp[255];
            char szStopTimestamp[255];
            char szLogTimestamp[255];
            string sElapsed;
            time_t aElapsed;
            string sSeparator;
            map<string, time_t> vPeriodTime;
            for (it3 = vTimeSpent.begin(); it3 != vTimeSpent.end(); it3++)
            {
              strftime(szStartTimestamp, 255, 
                szTimestampFormat, localtime(&it3->aStartTime));
              strftime(szStopTimestamp, 255, 
                szTimestampFormat, localtime(&it3->aStopTime));

              // calculate elapsed time
              aElapsed = it3->aStopTime - it3->aStartTime;
              
              // format elapsed time as readable string
              SecondsToHms(aElapsed, sElapsed);
              
              // add to period totals
              AddToTaskTotals(vPeriodTime, it3->sTaskPath, aElapsed);

              string sLogMessages = "";
              if (!it3->vLogMessages.empty())
              {
                it5 = it3->vLogMessages.begin();
                while (1) // only do comparison (below) once
                {
                  strftime(szLogTimestamp, 255, 
                    szTimestampFormat, localtime(&it5->aLogTime));
                  
                  // TODO: this should be generalized; create a dictionary
                  // and send it off 
                  string sLogFormat = szLogFormat;
                  string::size_type pos;
                  pos = sLogFormat.find("%WHEN%");
                  if (pos != string::npos)
                    sLogFormat.replace(pos, 6, szLogTimestamp);
                  pos = sLogFormat.find("%LOG%");
                  if (pos != string::npos)
                    sLogFormat.replace(pos, 5, it5->sLogMessage);

                  sLogMessages += sLogFormat;
                  if (++it5 == it3->vLogMessages.end())
                    break;
                }
              }

              // TODO: this should be generalized; create a dictionary
              // and send it off 
              string sFormat = szReportFormat;
              string::size_type pos;
              pos = sFormat.find("%BEGIN%");
              if (pos != string::npos)
                sFormat.replace(pos, 7, szStartTimestamp);
              pos = sFormat.find("%END%");
              if (pos != string::npos)
                sFormat.replace(pos, 5, szStopTimestamp);
              pos = sFormat.find("%DETAIL%");
              if (pos != string::npos)
                sFormat.replace(pos, 8, it3->sTaskPath.c_str());
              pos = sFormat.find("%ELAPSED%");
              if (pos != string::npos)
                sFormat.replace(pos, 9, sElapsed.c_str());
              pos = sFormat.find("%LOG%");
              if (pos != string::npos)
                sFormat.replace(pos, 5, sLogMessages.c_str());
              pos = sFormat.find("\\n");
              if (pos != string::npos)
                sFormat.replace(pos, 2, "\n");
              printf("%s", sFormat.c_str());
              //printf("%s\n", sFormat.c_str());
            }
            
            if (!vPeriodTime.empty() && vOptions["no-summary"].empty())
            {
              cout << endl;
              PrintOutTotals(sDateRange, vPeriodTime);
            }
          }
          else
          {
              throw ("No such command: " + sCommand).c_str();
          }
        }
    }
    catch (const string sError)
    {
        std::cerr << sError << std::endl << std::endl;
        std::cerr << g_szUsage << std::endl;
        iStatus = STIM_CLI_RETURN_USAGE_ERROR;
    }
    catch (const char* szError)
    {
        std::cerr << szError << std::endl << std::endl;
        std::cerr << g_szUsage << std::endl;
        iStatus = STIM_CLI_RETURN_USAGE_ERROR;
    }

    exit(iStatus);
}
