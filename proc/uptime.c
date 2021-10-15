/*
 * uptime - uptime related functions - part of procps
 *
 * Copyright (C) 1992-1998 Michael K. Johnson <johnsonm@redhat.com>
 * Copyright (C) ???? Larry Greenfield <greenfie@gauss.rutgers.edu>
 * Copyright (C) 1993 J. Cowley
 * Copyright (C) 1998-2003 Albert Cahalan
 * Copyright (C) 2015 Craig Small <csmall@dropbear.xyz>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include <proc/misc.h>
#include "procps-private.h"

#define UPTIME_FILE "/proc/uptime"

static __thread char upbuf[256];
static __thread char shortbuf[256];

static int count_users(void)
{
    int numuser = 0;
    struct utmp *ut;

    setutent();
    while ((ut = getutent())) {
    if ((ut->ut_type == USER_PROCESS) && (ut->ut_name[0] != '\0'))
        numuser++;
    }
    endutent();

    return numuser;
}

static char* unit_string(int unit_value, char* singular, char* plural, int short_format, char* short_unit) {
    if (short_format == 1)
        return short_unit;

    if (unit_value > 1)
        return plural;
    else
        return singular;
}

/*
 * uptime:
 *
 * Find the uptime and idle time of the system.
 * These numbers are found in /proc/uptime
 * Unlike other procps functions this closes the file each time
 * Either uptime_secs or idle_secs can be null
 *
 * Returns: 0 on success and <0 on failure
 */
PROCPS_EXPORT int procps_uptime(
        double *restrict uptime_secs,
        double *restrict idle_secs)
{
    double up=0, idle=0;
    locale_t tmplocale;
    FILE *fp;
    int rc;

    if ((fp = fopen(UPTIME_FILE, "r")) == NULL)
        return -errno;

    tmplocale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    uselocale(tmplocale);
    rc = fscanf(fp, "%lf %lf", &up, &idle);
    fclose(fp);
    uselocale(LC_GLOBAL_LOCALE);
    freelocale(tmplocale);

    if (uptime_secs)
        *uptime_secs = up;
    if (idle_secs)
        *idle_secs = idle;

    if (rc < 2)
        return -ERANGE;
    return 0;
}

/*
 * procps_uptime_sprint:
 *
 * Print current time in nice format
 *
 * Returns a statically allocated upbuf or NULL on error
 */
PROCPS_EXPORT char *procps_uptime_sprint(void)
{
    int upminutes, uphours, updays, users;
    int pos;
    time_t realseconds;
    struct tm realtime;
    double uptime_secs, idle_secs;
    double av1, av5, av15;

    upbuf[0] = '\0';
    if (time(&realseconds) < 0)
        return upbuf;
    localtime_r(&realseconds, &realtime);

    if (procps_uptime(&uptime_secs, &idle_secs) < 0)
        return upbuf;

    updays  =   ((int) uptime_secs / (60*60*24));
    uphours =   ((int) uptime_secs / (60*60)) % 24;
    upminutes = ((int) uptime_secs / (60)) % 60;

    pos = sprintf(upbuf, " %02d:%02d:%02d up ",
        realtime.tm_hour, realtime.tm_min, realtime.tm_sec);

    if (updays)
        pos += sprintf(upbuf + pos, "%d %s, ", updays, (updays > 1) ? "days" : "day");

    if (uphours)
        pos += sprintf(upbuf + pos, "%2d:%02d, ", uphours, upminutes);
    else
        pos += sprintf(upbuf + pos, "%d min, ", upminutes);

    users = count_users();
    procps_loadavg(&av1, &av5, &av15);

    pos += sprintf(upbuf + pos, "%2d %s,  load average: %.2f, %.2f, %.2f",
        users, users > 1 ? "users" : "user",
        av1, av5, av15);

    return upbuf;
}

/*
 * procps_uptime_sprint_pretty:
 *
 * Print current time in nice format
 *
 * Returns a statically allocated buffer or NULL on error
 */
PROCPS_EXPORT char *procps_uptime_sprint_pretty(int short_format)
{
    int updecades, upyears, upweeks, updays, uphours, upminutes;
    int pos = 3;
    int comma = 0;
    double uptime_secs, idle_secs;

    if (short_format == 1) {
        // reset position counter when short format is requested
        // as we also want to omit the "up " string
        pos = 0;
    }

    shortbuf[0] = '\0';
    if (procps_uptime(&uptime_secs, &idle_secs) < 0)
        return shortbuf;

    if (uptime_secs>60*60*24*365*10) {
      updecades = (int) uptime_secs / (60*60*24*365*10);
      uptime_secs -= updecades*60*60*24*365*10;
    }
    else {
      updecades = 0;
    }
    if (uptime_secs>60*60*24*365) {
      upyears = (int) uptime_secs / (60*60*24*365);
      uptime_secs -= upyears*60*60*24*365;
    }
    else {
      upyears = 0;
    }
    if (uptime_secs>60*60*24*7) {
      upweeks = (int) uptime_secs / (60*60*24*7);
      uptime_secs -= upweeks*60*60*24*7;
    }
    else {
      upweeks = 0;
    }
    if (uptime_secs>60*60*24) {
      updays = (int) uptime_secs / (60*60*24);
      uptime_secs -= updays*60*60*24;
    }
    else {
      updays = 0;
    }
    if (uptime_secs>60*60) {
        uphours = (int) uptime_secs / (60*60);
        uptime_secs -= uphours*60*60;
    }
    if (uptime_secs>60) {
        upminutes = (int) uptime_secs / 60;
        uptime_secs -= upminutes*60;
    }
    /*updecades =  (int) uptime_secs / (60*60*24*365*10);
    upyears =   ((int) uptime_secs / (60*60*24*365)) % 10;
    upweeks =   ((int) uptime_secs / (60*60*24*7)) % 52;
    updays  =   ((int) uptime_secs / (60*60*24)) % 7;
    uphours =   ((int) uptime_secs / (60*60)) % 24;
    upminutes = ((int) uptime_secs / (60)) % 60;
*/
    if (short_format == 0) {
        strcat(shortbuf, "up ");
    }

    if (updecades) {
        pos += sprintf(shortbuf + pos, "%d%s",
                       updecades, 
                       unit_string(updecades, " decade", " decades", short_format, "D"));
        comma += 1;
    }

    if (upyears) {
        pos += sprintf(shortbuf + pos, "%s%d%s",
                       comma > 0 ? ", " : "", upyears,
                       unit_string(upyears, " year", " years", short_format, "y"));
        comma += 1;
    }

    if (upweeks) {
        pos += sprintf(shortbuf + pos, "%s%d%s",
                       comma  > 0 ? ", " : "", upweeks,
                       unit_string(upweeks, " week", " weeks", short_format, "w"));
        comma += 1;
    }

    if (updays) {
        pos += sprintf(shortbuf + pos, "%s%d%s",
                       comma  > 0 ? ", " : "", updays,
                       unit_string(updays, " day", " days", short_format, "d"));
        comma += 1;
    }

    if (uphours) {
        pos += sprintf(shortbuf + pos, "%s%d%s",
                       comma  > 0 ? ", " : "", uphours,
                       unit_string(uphours, " hour", " hours", short_format, "h"));
        comma += 1;
    }

    if (upminutes || (!upminutes && uptime_secs < 60)) {
        pos += sprintf(shortbuf + pos, "%s%d%s",
                       comma > 0 ? ", " : "", upminutes,
                       unit_string(upminutes, " minute", " minutes", short_format, "m"));
        comma += 1;
    }
    return shortbuf;
}



