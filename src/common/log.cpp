
/*
 *   Copyright 2021 Huawei Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "log.hpp"
#include "config.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <cstring>

#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "suppressions.h"


namespace lpf {

    std::string log_timestamp()
    {
      ::std::ostringstream s;
      struct timeval tv; std::memset( &tv, 0, sizeof(tv));
      struct tm tm;      std::memset( &tm, 0, sizeof(tm));
      if (  0    != gettimeofday( &tv, NULL)
         || NULL == localtime_r( &tv.tv_sec, &tm )
         )
      {
          s << "[ no time available ] ";
      }
      else
      {
          using ::std::setfill; using ::std::setw;
          s << "[ " << setfill('0') << setw(4) << (1900+tm.tm_year)
                  << "-" << setw(2) << (tm.tm_mon+1)
                  << "-" << setw(2) << (tm.tm_mday)
                  << " " << setw(2) << tm.tm_hour
                  << ":" << setw(2) << tm.tm_min
                  << ":" << setw(2) << tm.tm_sec
                  << "." << setw(6) << tv.tv_usec << " ] "    ;
      }
      return s.str();
    }

    int log_level()
    {
        static int level = Config::instance().getLogLevel();
        return level;
    }

    void log_error()
    {
        char msg[] = "ERROR: CANNOT WRITE LOG MESSAGES\n";
	//any error the below call to write may report matches exactly the error we
	//wish to report using write; therefore it is correct to ignore.
	LPFLIB_IGNORE_UNUSED_RESULT
        (void) write(2, msg, strlen(msg));
	LPFLIB_RESTORE_WARNINGS
    }

}

