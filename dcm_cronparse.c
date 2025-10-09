/*
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2024 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Code adapted from staticlabs/ccronexpr which is:
 * Copyright 2015, alex at staticlibs.net
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>

#include "dcm_types.h"
#include "dcm_cronparse.h"

#define CRON_SUCCESS  0
#define CRON_FAILURE -1

#define CRON_INVALID_INSTANT ((time_t) -1)

#define CRON_MAX_SECONDS       60
#define CRON_MAX_MINUTES       60
#define CRON_MAX_HOURS         24
#define CRON_MAX_DAYS_OF_WEEK  8
#define CRON_MAX_DAYS_OF_MONTH 32
#define CRON_MAX_MONTHS        12
#define CRON_MAX_YEARS_DIFF    4

#define CRON_CF_SECOND         0
#define CRON_CF_MINUTE         1
#define CRON_CF_HOUR_OF_DAY    2
#define CRON_CF_DAY_OF_WEEK    3
#define CRON_CF_DAY_OF_MONTH   4
#define CRON_CF_MONTH          5
#define CRON_CF_YEAR           6

#define CRON_CF_ARR_LEN           7
#define CRON_MAX_STR_LEN_TO_SPLIT 256

static const INT8* const DAYS_ARR[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
#define CRON_DAYS_ARR_LEN 7
static const INT8* const MONTHS_ARR[] = { "FOO", "JAN", "FEB", "MAR", "APR",
                                          "MAY", "JUN", "JUL", "AUG", "SEP",
                                          "OCT", "NOV", "DEC" };
#define CRON_MONTHS_ARR_LEN 13

static INT32 dcmCronParseToUpper(INT8* str)
{
    if (!str) return 1;
    INT32 i;
    for (i = 0; '\0' != str[i]; i++) {
        INT32 c = (INT32)str[i];
        str[i] = (INT8)toupper(c);
    }
    return 0;
}

static UINT32 dcmCronParseParseUint(const INT8* str, INT32* errcode)
{
    INT8* endptr;
    errno = 0;
    INT64 l = strtol(str, &endptr, 10);
    if (errno == ERANGE || *endptr != '\0' || l < 0 || l > INT_MAX) {
        *errcode = 1;
        return 0;
    }
    else {
        *errcode = 0;
        return (UINT32)l;
    }
}

static VOID dcmCronParseFreeSplit(INT8** splitted, UINT32 len)
{
    UINT32 i;
    if (!splitted) return;
    for (i = 0; i < len; i++) {
        if (splitted[i]) {
            free(splitted[i]);
        }
    }
    free(splitted);
}

static INT8* dcmCronParseStrDupl(const INT8* str, UINT32 len)
{
    if (!str) return NULL;
    INT8* res = (INT8*)malloc(len + 1);
    if (!res) return NULL;
    memset(res, 0, len + 1);
    memcpy(res, str, len);
    return res;
}

static INT8** dcmCronParseStrSplit(const INT8* str, INT8 del, UINT32* len_out)
{
    UINT32 i;
    UINT32 stlen = 0;
    UINT32 len = 0;
    INT32 accum = 0;
    INT8* buf = NULL;
    INT8** res = NULL;
    UINT32 bi = 0;
    UINT32 ri = 0;
    INT8* tmp;

    if (!str) goto return_error;
    for (i = 0; '\0' != str[i]; i++) {
        stlen += 1;
        if (stlen >= CRON_MAX_STR_LEN_TO_SPLIT) goto return_error;
    }

    for (i = 0; i < stlen; i++) {
        INT32 c = str[i];
        if (del == str[i]) {
            if (accum > 0) {
                len += 1;
                accum = 0;
            }
        }
        else if (!isspace(c)) {
            accum += 1;
        }
    }
    /* tail */
    if (accum > 0) {
        len += 1;
    }
    if (0 == len) return NULL;

    buf = (INT8*)malloc(stlen + 1);
    if (!buf) goto return_error;
    memset(buf, 0, stlen + 1);
    res = (INT8**)malloc(len * sizeof(INT8*));
    if (!res) goto return_error;
    memset(res, 0, len * sizeof(INT8*));

    for (i = 0; i < stlen; i++) {
        INT32 c = str[i];
        if (del == str[i]) {
            if (bi > 0) {
                tmp = dcmCronParseStrDupl(buf, bi);
                if (!tmp) goto return_error;
                res[ri++] = tmp;
                memset(buf, 0, stlen + 1);
                bi = 0;
            }
        }
        else if (!isspace(c)) {
            buf[bi++] = str[i];
        }
    }
    /* tail */
    if (bi > 0) {
        tmp = dcmCronParseStrDupl(buf, bi);
        if (!tmp) goto return_error;
        res[ri++] = tmp;
    }
    free(buf);
    *len_out = len;
    return res;

return_error:
    if (buf) {
        free(buf);
    }
    dcmCronParseFreeSplit(res, len);
    *len_out = 0;
    return NULL;

}

static INT8* dcmCronParseStrReplace(INT8 *orig, const INT8 *rep, const INT8 *with)
{
    INT8 *result; /* the return string */
    INT8 *ins; /* the next insert point */
    INT8 *tmp; /* varies */
    UINT32 len_rep; /* length of rep */
    UINT32 len_with; /* length of with */
    UINT32 len_front; /* distance between rep and end of last rep */
    INT32 count; /* number of replacements */
    if (!orig) return NULL;
    if (!rep) rep = "";
    if (!with) with = "";
    len_rep = strlen(rep);
    len_with = strlen(with);

    ins = orig;
    for (count = 0; NULL != (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    /* first time through the loop, all the variable are set correctly
     from here on,
     tmp points to the end of the result string
     ins points to the next occurrence of rep in orig
     orig points to the remainder of orig after "end of rep"
     */
    tmp = result = (INT8*)malloc(strlen(orig) + (len_with - len_rep) * count + 1);
    if (!result) return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; /* move to next "end of rep" */
    }
    strcpy(tmp, orig);
    return result;
}

static UINT8 dcmCronParseGetBit(UINT8* rbyte, INT32 idx)
{
    UINT8 j = (UINT8)(idx / 8);
    UINT8 k = (UINT8)(idx % 8);

    if (rbyte[j] & (1 << k)) {
        return 1;
    }
    else {
        return 0;
    }
}

static VOID dcmCronParseDelBit(UINT8* rbyte, INT32 idx)
{
    UINT8 j = (UINT8)(idx / 8);
    UINT8 k = (UINT8)(idx % 8);

    rbyte[j] &= ~(1 << k);
}

static UINT32 dcmCronParseNextSetBit(UINT8* bits, UINT32 max,
                                     UINT32 from_index, INT32* notfound)
{
    UINT32 i;
    if (!bits) {
        *notfound = 1;
        return 0;
    }
    for (i = from_index; i < max; i++) {
        if (dcmCronParseGetBit(bits, i)) return i;
    }
    *notfound = 1;
    return 0;
}

static UINT32* dcmCronParseGetRange(INT8* field, UINT32 min, UINT32 max, INT32 *ret)
{

    INT8** parts = NULL;
    UINT32 len = 0;
    *ret = CRON_SUCCESS;

    UINT32* res = (UINT32*)malloc(2 * sizeof(UINT32));
    if (!res) goto return_error;

    res[0] = 0;
    res[1] = 0;
    if (1 == strlen(field) && '*' == field[0]) {
        res[0] = min;
        res[1] = max - 1;
    }
    else if (!strchr(field, '-')){
        INT32 err = 0;
        UINT32 val = dcmCronParseParseUint(field, &err);
        if (err) {
            goto return_error;
        }

        res[0] = val;
        res[1] = val;
    }
    else {
        parts = dcmCronParseStrSplit(field, '-', &len);
        if (2 != len) {
            goto return_error;
        }
        INT32 err = 0;
        res[0] = dcmCronParseParseUint(parts[0], &err);
        if (err) {
            goto return_error;
        }
        res[1] = dcmCronParseParseUint(parts[1], &err);
        if (err) {
            goto return_error;
        }
    }
    if (res[0] >= max || res[1] >= max) {
        goto return_error;
    }
    if (res[0] < min || res[1] < min) {
        goto return_error;
    }
    if (res[0] > res[1]) {
        goto return_error;
    }

    dcmCronParseFreeSplit(parts, len);

    return res;

return_error:
    *ret = CRON_FAILURE;
    dcmCronParseFreeSplit(parts, len);
    if (res) {
        free(res);
    }

    return NULL;
}

static VOID dcmCronParseSetBit(UINT8* rbyte, INT32 idx)
{
    UINT8 j = (UINT8)(idx / 8);
    UINT8 k = (UINT8)(idx % 8);

    rbyte[j] |= (1 << k);
}

static time_t dcmCronParseMktime(struct tm* tm)
{
    return timegm(tm);
}

static struct tm* dcmCronParseTime(time_t* date, struct tm* out)
{
    return gmtime_r(date, out);
}

static INT8* dcmCronParseReplaceOrdinals(INT8* value, const INT8* const * arr, UINT32 arr_len)
{
    UINT32 i;
    INT8* cur = value;
    INT8* res = NULL;
    INT32 first = 1;
    for (i = 0; i < arr_len; i++) {
        INT8 strnum[4] = {0};
        sprintf(strnum, "%d", (INT32)i);
        res = dcmCronParseStrReplace(cur, arr[i], strnum);

        if (!first) {
            free(cur);
        }
        if (!res) {
            return NULL;
        }
        cur = res;
        if (first) {
            first = 0;
        }
    }
    return res;
}
/**
 * Reset the calendar setting all the fields provided to zero.
 */
static INT32 dcmCronParseResetMin(struct tm* calendar, INT32 field)
{
    if (!calendar || -1 == field) {
        return 1;
    }
    switch (field) {
    case CRON_CF_SECOND:
        calendar->tm_sec = 0;
        break;
    case CRON_CF_MINUTE:
        calendar->tm_min = 0;
        break;
    case CRON_CF_HOUR_OF_DAY:
        calendar->tm_hour = 0;
        break;
    case CRON_CF_DAY_OF_WEEK:
        calendar->tm_wday = 0;
        break;
    case CRON_CF_DAY_OF_MONTH:
        calendar->tm_mday = 1;
        break;
    case CRON_CF_MONTH:
        calendar->tm_mon = 0;
        break;
    case CRON_CF_YEAR:
        calendar->tm_year = 0;
        break;
    default:
        return 1; /* unknown field */
    }
    time_t res = dcmCronParseMktime(calendar);
    if (CRON_INVALID_INSTANT == res) {
        return 1;
    }
    return 0;
}

static INT32 dcmCronParseResetAllMin(struct tm* calendar, INT32* fields)
{
    INT32 i;
    INT32 res = 0;
    if (!calendar || !fields) {
        return 1;
    }
    for (i = 0; i < CRON_CF_ARR_LEN; i++) {
        if (-1 != fields[i]) {
            res = dcmCronParseResetMin(calendar, fields[i]);
            if (0 != res) return res;
        }
    }
    return 0;
}

static INT32 dcmCronParseSetField(struct tm* calendar, INT32 field, INT32 val)
{
    if (!calendar || -1 == field) {
        return 1;
    }
    switch (field) {
    case CRON_CF_SECOND:
        calendar->tm_sec = val;
        break;
    case CRON_CF_MINUTE:
        calendar->tm_min = val;
        break;
    case CRON_CF_HOUR_OF_DAY:
        calendar->tm_hour = val;
        break;
    case CRON_CF_DAY_OF_WEEK:
        calendar->tm_wday = val;
        break;
    case CRON_CF_DAY_OF_MONTH:
        calendar->tm_mday = val;
        break;
    case CRON_CF_MONTH:
        calendar->tm_mon = val;
        break;
    case CRON_CF_YEAR:
        calendar->tm_year = val;
        break;
    default:
        return 1; /* unknown field */
    }
    time_t res = dcmCronParseMktime(calendar);
    if (CRON_INVALID_INSTANT == res) {
        return 1;
    }
    return 0;
}

static INT32 dcmCronParseSetNumberHits(const INT8* value, UINT8* target,
                                       UINT32 min, UINT32 max)
{
    UINT32 i;
    INT32 ret = CRON_SUCCESS;
    UINT32 i1;
    UINT32 len = 0;

    INT8** fields = dcmCronParseStrSplit(value, ',', &len);
    if (!fields) {
        ret = CRON_FAILURE;
        goto return_result;
    }

    for (i = 0; i < len; i++) {
        if (!strchr(fields[i], '/')) {
            /* Not an incrementer so it must be a range (possibly empty) */

            UINT32* range = dcmCronParseGetRange(fields[i], min, max, &ret);

            if (ret) {
                if (range) {
                    free(range);
                }
                ret = CRON_FAILURE;
                goto return_result;

            }

            for (i1 = range[0]; i1 <= range[1]; i1++) {
                dcmCronParseSetBit(target, i1);

            }
            free(range);

        }
        else {
            UINT32 len2 = 0;
            INT8** split = dcmCronParseStrSplit(fields[i], '/', &len2);
            if (2 != len2) {
                ret = CRON_FAILURE;
                dcmCronParseFreeSplit(split, len2);
                goto return_result;
            }
            UINT32* range = dcmCronParseGetRange(split[0], min, max, &ret);
            if (ret) {
                if (range) {
                    free(range);
                }
                dcmCronParseFreeSplit(split, len2);
                goto return_result;
            }
            if (!strchr(split[0], '-')) {
                range[1] = max - 1;
            }
            INT32 err = 0;
            UINT32 delta = dcmCronParseParseUint(split[1], &err);
            if (err) {
                ret = CRON_FAILURE;
                free(range);
                dcmCronParseFreeSplit(split, len2);
                goto return_result;
            }
            if (0 == delta) {
                ret = CRON_FAILURE;
                free(range);
                dcmCronParseFreeSplit(split, len2);
                goto return_result;
            }
            for (i1 = range[0]; i1 <= range[1]; i1 += delta) {
                dcmCronParseSetBit(target, i1);
            }
            dcmCronParseFreeSplit(split, len2);
            free(range);

        }
    }
    goto return_result;

return_result:
    dcmCronParseFreeSplit(fields, len);
    return ret;

}

static VOID dcmCronParsePushToFieldsArr(INT32* arr, INT32 fi)
{
    INT32 i;
    if (!arr || -1 == fi) {
        return;
    }
    for (i = 0; i < CRON_CF_ARR_LEN; i++) {
        if (arr[i] == fi) return;
    }
    for (i = 0; i < CRON_CF_ARR_LEN; i++) {
        if (-1 == arr[i]) {
            arr[i] = fi;
            return;
        }
    }
}

static INT32 dcmCronParseAddToField(struct tm* calendar, INT32 field, INT32 val)
{
    if (!calendar || -1 == field) {
        return 1;
    }
    switch (field) {
    case CRON_CF_SECOND:
        calendar->tm_sec = calendar->tm_sec + val;
        break;
    case CRON_CF_MINUTE:
        calendar->tm_min = calendar->tm_min + val;
        break;
    case CRON_CF_HOUR_OF_DAY:
        calendar->tm_hour = calendar->tm_hour + val;
        break;
    case CRON_CF_DAY_OF_WEEK: /* mkgmtime ignores this field */
    case CRON_CF_DAY_OF_MONTH:
        calendar->tm_mday = calendar->tm_mday + val;
        break;
    case CRON_CF_MONTH:
        calendar->tm_mon = calendar->tm_mon + val;
        break;
    case CRON_CF_YEAR:
        calendar->tm_year = calendar->tm_year + val;
        break;
    default:
        return 1; /* unknown field */
    }
    time_t res = dcmCronParseMktime(calendar);
    if (CRON_INVALID_INSTANT == res) {
        return 1;
    }
    return 0;
}
static INT32 dcmCronParseSetMonths(INT8* value, UINT8* targ)
{
    UINT32 i;
    UINT32 max = 12;
    INT32 ret = CRON_SUCCESS;

    INT8* replaced = NULL;

    dcmCronParseToUpper(value);
    replaced = dcmCronParseReplaceOrdinals(value, MONTHS_ARR, CRON_MONTHS_ARR_LEN);
    if (!replaced) {
        return CRON_FAILURE;
    }
    ret = dcmCronParseSetNumberHits(replaced, targ, 1, max + 1);
    free(replaced);

    /* ... and then rotate it to the front of the months */
    for (i = 1; i <= max; i++) {
        if (dcmCronParseGetBit(targ, i)) {
            dcmCronParseSetBit(targ, i - 1);
            dcmCronParseDelBit(targ, i);
        }
    }
    return ret;
}

static INT32 dcmCronParseSetDaysOfWeek(INT8* field, UINT8* targ)
{
    UINT32 max = 7;
    INT8* replaced = NULL;
    INT32 ret = CRON_SUCCESS;

    if (1 == strlen(field) && '?' == field[0]) {
        field[0] = '*';
    }
    dcmCronParseToUpper(field);
    replaced = dcmCronParseReplaceOrdinals(field, DAYS_ARR, CRON_DAYS_ARR_LEN);
    if (!replaced) {
        return CRON_FAILURE;
    }
    ret = dcmCronParseSetNumberHits(replaced, targ, 0, max + 1);
    free(replaced);
    if (dcmCronParseGetBit(targ, 7)) {
        /* Sunday can be represented as 0 or 7*/
        dcmCronParseSetBit(targ, 0);
        dcmCronParseDelBit(targ, 7);
    }
    return ret;
}

static INT32 dcmCronParseSetDaysOfMonth(INT8* field, UINT8* targ)
{
    INT32 ret = CRON_SUCCESS;
    /* Days of month start with 1 (in Cron and Calendar) so add one */
    if (1 == strlen(field) && '?' == field[0]) {
        field[0] = '*';
    }
    ret = dcmCronParseSetNumberHits(field, targ, 1, CRON_MAX_DAYS_OF_MONTH);

    return ret;
}

/**
 * Search the bits provided for the next set bit after the value provided,
 * and reset the calendar.
 */
static UINT32 dcmCronParseFindNext(UINT8* bits, UINT32 max,
                                   UINT32 value, struct tm* calendar,
                                   UINT32 field, UINT32 nextField,
                                   INT32* lower_orders, INT32* res_out)
{
    INT32 notfound = 0;
    INT32 err = 0;
    UINT32 next_value = dcmCronParseNextSetBit(bits, max, value, &notfound);
    /* roll over if needed */
    if (notfound) {
        err = dcmCronParseAddToField(calendar, nextField, 1);
        if (err) goto return_error;
        err = dcmCronParseResetMin(calendar, field);
        if (err) goto return_error;
        notfound = 0;
        next_value = dcmCronParseNextSetBit(bits, max, 0, &notfound);
    }
    if (notfound || next_value != value) {
        err = dcmCronParseSetField(calendar, field, next_value);
        if (err) goto return_error;
        err = dcmCronParseResetAllMin(calendar, lower_orders);
        if (err) goto return_error;
    }
    return next_value;

return_error:
    *res_out = 1;
    return 0;
}


static UINT32 dcmCronParseFindNextDay(struct tm* calendar, UINT8* days_of_month,
                                      UINT32 day_of_month, UINT8* days_of_week,
                                      UINT32 day_of_week, INT32* resets, INT32* res_out)
{
    INT32 err;
    UINT32 count = 0;
    UINT32 max = 366;
    while ((!dcmCronParseGetBit(days_of_month, day_of_month) ||
            !dcmCronParseGetBit(days_of_week, day_of_week)) && count++ < max) {
        err = dcmCronParseAddToField(calendar, CRON_CF_DAY_OF_MONTH, 1);

        if (err) goto return_error;
        day_of_month = calendar->tm_mday;
        day_of_week = calendar->tm_wday;
        dcmCronParseResetAllMin(calendar, resets);
    }
    return day_of_month;

return_error:
    *res_out = 1;
    return 0;
}

static INT32 dcmCronParseDoNext(dcmCronExpr* expr, struct tm* calendar, UINT32 dot)
{
    INT32 i;
    INT32 res = 0;
    INT32* resets = NULL;
    INT32* empty_list = NULL;
    UINT32 second = 0;
    UINT32 update_second = 0;
    UINT32 minute = 0;
    UINT32 update_minute = 0;
    UINT32 hour = 0;
    UINT32 update_hour = 0;
    UINT32 day_of_week = 0;
    UINT32 day_of_month = 0;
    UINT32 update_day_of_month = 0;
    UINT32 month = 0;
    UINT32 update_month = 0;

    resets = (INT32*)malloc(CRON_CF_ARR_LEN * sizeof(INT32));
    if (!resets) goto return_result;
    empty_list = (INT32*)malloc(CRON_CF_ARR_LEN * sizeof(INT32));
    if (!empty_list) goto return_result;
    for (i = 0; i < CRON_CF_ARR_LEN; i++) {
        resets[i] = -1;
        empty_list[i] = -1;
    }

    second = calendar->tm_sec;
    update_second = dcmCronParseFindNext(expr->seconds, CRON_MAX_SECONDS,
                                         second, calendar,
                                         CRON_CF_SECOND, CRON_CF_MINUTE,
                                         empty_list, &res);
    if (0 != res) goto return_result;
    if (second == update_second) {
        dcmCronParsePushToFieldsArr(resets, CRON_CF_SECOND);
    }

    minute = calendar->tm_min;
    update_minute = dcmCronParseFindNext(expr->minutes, CRON_MAX_MINUTES,
                                         minute, calendar,
                                         CRON_CF_MINUTE, CRON_CF_HOUR_OF_DAY,
                                         resets, &res);
    if (0 != res) goto return_result;
    if (minute == update_minute) {
        dcmCronParsePushToFieldsArr(resets, CRON_CF_MINUTE);
    }
    else {
        res = dcmCronParseDoNext(expr, calendar, dot);
        if (0 != res) goto return_result;
    }

    hour = calendar->tm_hour;
    update_hour = dcmCronParseFindNext(expr->hours, CRON_MAX_HOURS,
                                       hour, calendar,
                                       CRON_CF_HOUR_OF_DAY, CRON_CF_DAY_OF_WEEK,
                                       resets, &res);
    if (0 != res) goto return_result;
    if (hour == update_hour) {
        dcmCronParsePushToFieldsArr(resets, CRON_CF_HOUR_OF_DAY);
    }
    else {
        res = dcmCronParseDoNext(expr, calendar, dot);
        if (0 != res) goto return_result;
    }

    day_of_week = calendar->tm_wday;
    day_of_month = calendar->tm_mday;
    update_day_of_month = dcmCronParseFindNextDay(calendar, expr->days_of_month,
                                                  day_of_month, expr->days_of_week,
                                                  day_of_week, resets, &res);
    if (0 != res) goto return_result;
    if (day_of_month == update_day_of_month) {
        dcmCronParsePushToFieldsArr(resets, CRON_CF_DAY_OF_MONTH);
    }
    else {
        res = dcmCronParseDoNext(expr, calendar, dot);
        if (0 != res) goto return_result;
    }

    month = calendar->tm_mon; /*day already adds one if no day in same month is found*/
    update_month = dcmCronParseFindNext(expr->months, CRON_MAX_MONTHS,
                                        month, calendar,
                                        CRON_CF_MONTH, CRON_CF_YEAR,
                                        resets, &res);
    if (0 != res) goto return_result;
    if (month != update_month) {
        if (calendar->tm_year - dot > 4) {
            res = -1;
            goto return_result;
        }
        res = dcmCronParseDoNext(expr, calendar, dot);
        if (0 != res) goto return_result;
    }
    goto return_result;

return_result:
    if (!resets || !empty_list) {
        res = -1;
    }
    if (resets) {
        free(resets);
    }
    if (empty_list) {
        free(empty_list);
    }
    return res;
}

/** @brief This function gets the next in the pattern
 *
 *  @param[in]  expr      Parsed Cron pattern
 *  @param[out] date      current date
 *
 *  @return  Returns the time.
 *  @retval  Returns the time.
 */
time_t dcmCronParseGetNext(dcmCronExpr* expr, time_t date)
{
    if (!expr) return CRON_INVALID_INSTANT;
    struct tm calval;
    memset(&calval, 0, sizeof(struct tm));
    struct tm* calendar = dcmCronParseTime(&date, &calval);
    if (!calendar) return CRON_INVALID_INSTANT;
    time_t original = dcmCronParseMktime(calendar);
    if (CRON_INVALID_INSTANT == original) return CRON_INVALID_INSTANT;

    INT32 res = dcmCronParseDoNext(expr, calendar, calendar->tm_year);
    if (0 != res) return CRON_INVALID_INSTANT;

    time_t calculated = dcmCronParseMktime(calendar);
    if (CRON_INVALID_INSTANT == calculated) return CRON_INVALID_INSTANT;
    if (calculated == original) {
        /* We arrived at the original timestamp - round up to the next whole second and try again... */
        res = dcmCronParseAddToField(calendar, CRON_CF_SECOND, 1);
        if (0 != res) return CRON_INVALID_INSTANT;
        res = dcmCronParseDoNext(expr, calendar, calendar->tm_year);
        if (0 != res) return CRON_INVALID_INSTANT;
    }

    return dcmCronParseMktime(calendar);
}

/** @brief This function Parses the cron pattern
 *
 *  @param[in]  expression   Cron pattern
 *  @param[out] target       Parsed pattern structure
 *
 *  @return  Returns Status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32 dcmCronParseExp(const INT8* expression, dcmCronExpr* target)
{
    UINT32 len = 0;
    INT8** fields = NULL;
    INT32 i = 0;
    INT32 ret = CRON_SUCCESS;

    if (!expression) {
        ret = CRON_FAILURE;
        goto return_res;
    }
    if (!target) {
        ret = CRON_FAILURE;
        goto return_res;
    }

    fields = dcmCronParseStrSplit(expression, ' ', &len);
    if (len < 5 || len > 6) {
        ret = CRON_FAILURE;
        goto return_res;
    }

    memset(target, 0, sizeof(*target));

    if (len == 6) {
        ret = dcmCronParseSetNumberHits(fields[0], target->seconds, 0, 60);
        if (ret) {
            goto return_res;
        }
        i = 1;
    }
    else if (len == 5) {
        i = 0;
        target->seconds[0] = 1;
    }

    ret = dcmCronParseSetNumberHits(fields[i], target->minutes, 0, 60);
    if (ret) {
        goto return_res;
    }
    ret = dcmCronParseSetNumberHits(fields[i + 1], target->hours, 0, 24);
    if (ret) {
        goto return_res;
    }
    ret = dcmCronParseSetDaysOfMonth(fields[i + 2], target->days_of_month);
    if (ret) {
        goto return_res;
    }
    ret = dcmCronParseSetMonths(fields[i + 3], target->months);
    if (ret) {
        goto return_res;
    }
    ret = dcmCronParseSetDaysOfWeek(fields[i + 4], target->days_of_week);
    if (ret) {
        goto return_res;
    }

return_res:
    dcmCronParseFreeSplit(fields, len);
    return ret;
}

INT32 (*getdcmCronParseToUpper(void)) (INT8*) {
	return &dcmCronParseToUpper;
}
