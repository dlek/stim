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
"       stim report <daterange> [taskpath...]\n";


int main(int argc, char** argv)
{
    int iStatus;
    int iLoop;
    string sCommand;
    vector<string> vArgs;
    map<string, string> vOptions;
    string sStimDirectory;

    // TODO: this should be configurable, and a default
    string sContract("general");

    // assume the best
    iStatus = STIM_CLI_RETURN_SUCCESS;

    // determine home directory
    sStimDirectory = getenv("HOME");
    sStimDirectory += "/.stim";

    // initialise Stim
    Stim cStim(sStimDirectory.c_str(), sContract.c_str());

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

    // attempt to parse command
    try
    {
        if (sCommand == "" || sCommand == "help")
        {
            std::cout << g_szUsage << std::endl;
            exit(0);
        }
        else if (sCommand == "start")
        {
            // syntax: start <task>
            if (vArgs.size() != 1)
                throw "start <task>";
            string sTask = vArgs[0];

            // specified time?
            time_t tWhen;
            if (vOptions["when"].empty())
              tWhen = time(0);
            else
            {
              string& sWhen = vOptions["when"];
              if (sWhen.c_str()[0] == '-')
              {
                // TODO: fill this out  
              }
            }


            // now start the task
            cStim.StartTask(tWhen, sTask);
        }
        else if (sCommand == "stop")
        {
            // syntax: stop
            if (vArgs.size() > 0)
                throw "stop";
            
            // now stop the current task
            cStim.StopTask(time(0));
        }
        else if (sCommand == "log")
        {
            // syntax: log <message>
            if (vArgs.size() != 1)
                throw "log <message>";
            string sMessage = vArgs[0];

            // now log the message
            cStim.LogTask(time(0), sMessage);
        }
        else if (sCommand == "status")
        {
            // syntax: status
            if (vArgs.size() > 0)
            {
                throw "status";
            }

            cStim.Status();
        }
        else if (sCommand == "report")
        {
            // syntax: report <daterange> [taskpath...]
            if (vArgs.size() == 0)
                throw "report <daterange> [taskpath...]";
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
            if (!cStim.ReportTime(sDateRange, vTaskPaths, vTimeSpent))
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
              if (vOptions["dateformat"].empty())
              {
                GkMakeTimestamp(it3->aStartTime, szStartTimestamp);
                GkMakeTimestamp(it3->aStopTime, szStopTimestamp);
              }
              else
              {
                strftime(szStartTimestamp, 255, vOptions["dateformat"].c_str(), localtime(&it3->aStartTime));
                strftime(szStopTimestamp, 255, vOptions["dateformat"].c_str(), localtime(&it3->aStopTime));
              }

              // determine separator (TODO: use constant defined elsewhere)
              if (vOptions["separator"].empty())
                sSeparator = " :: ";
              else
                sSeparator = vOptions["separator"];
              
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
                  if (vOptions["dateformat"].empty())
                    GkMakeTimestamp(it5->aLogTime, szLogTimestamp);
                  else
                    strftime(szLogTimestamp, 255, vOptions["dateformat"].c_str(), localtime(&it5->aLogTime));

                  sLogMessages.append(szLogTimestamp);
                  sLogMessages += ": " + it5->sLogMessage;
                  if (++it5 == it3->vLogMessages.end())
                    break;
                  else
                    sLogMessages += sSeparator;
                }
              }

              if (vOptions["format"].empty())
              {
                printf("%17s to %17s | %8s | %s\n", 
                  szStartTimestamp, 
                  szStopTimestamp, 
                  sElapsed.c_str(), 
                  it3->sTaskPath.c_str());
              }
              else
              {
                string sFormat = vOptions["format"];
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
    catch (const char* szError)
    {
        std::cerr << szError << std::endl;
        std::cerr << g_szUsage << std::endl;
        iStatus = STIM_CLI_RETURN_USAGE_ERROR;
   }

   exit(iStatus);
}
