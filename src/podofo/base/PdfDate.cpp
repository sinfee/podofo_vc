/***************************************************************************
 *   Copyright (C) 2005 by Dominik Seichter                                *
 *   domseichter@web.de                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                         *
 *   In addition, as a special exception, the copyright holders give       *
 *   permission to link the code of portions of this program with the      *
 *   OpenSSL library under certain conditions as described in each         *
 *   individual source file, and distribute linked combinations            *
 *   including the two.                                                    *
 *   You must obey the GNU General Public License in all respects          *
 *   for all of the code used other than OpenSSL.  If you modify           *
 *   file(s) with this exception, you may extend this exception to your    *
 *   version of the file(s), but you are not obligated to do so.  If you   *
 *   do not wish to do so, delete this exception statement from your       *
 *   version.  If you delete this exception statement from all source      *
 *   files in the program, then also delete it here.                       *
 ***************************************************************************/

#include "PdfDate.h"
#include "PdfDefinesPrivate.h"

#include <string.h>
#include <sstream>

#if _WIN32
#include <Windows.h>
#define timegm _mkgmtime
#endif

namespace  {

/** Parse fixed length number from string
  *  \param in string to read number from
  *  \param length exact number characters to read
  *  \param min minimal value of number
  *  \param max maximal value of number
  *  \param ret parsed number (updated only on success)
  */
enum EParseFixLenNumberResult {Ok, Missing, Error};
EParseFixLenNumberResult ParseFixLenNumber(const char *&in, unsigned int length, int min, int max, int &ret_)
{
    if ( in == NULL || *in == '\0' || *in == '+' || *in == '-' || *in == 'Z') return Missing;
    int ret = 0;
    for(unsigned int i=0;i<length;i++)
    {
        if ( in == NULL || !isdigit(*in)) return Error;
        ret = ret*10+ (*in-'0');
        in++;
    }
    if ( ret < min || ret > max ) return Error;
    ret_ = ret;
    return Ok;
}

bool ParseOptionalFields(const char *&pszDate, tm& _tm)
{
    EParseFixLenNumberResult res = ParseFixLenNumber(pszDate,2,1,12,_tm.tm_mon);
    if ( res == Error )
    {
        return false;
    }
    else if( res == Missing)
    {
        return true;
    }
    _tm.tm_mon--;

    res = ParseFixLenNumber(pszDate,2,1,31,_tm.tm_mday);
    if ( res == Error )
    {
        return false;
    }
    else if( res == Missing)
    {
        return true;
    }

    res = ParseFixLenNumber(pszDate,2,0,23,_tm.tm_hour);
    if ( res == Error )
    {
        return false;
    }
    else if( res == Missing)
    {
        return true;
    }

    res = ParseFixLenNumber(pszDate,2,0,59,_tm.tm_min);
    if ( res == Error )
    {
        return false;
    }
    else if( res == Missing)
    {
        return true;
    }

    res = ParseFixLenNumber(pszDate,2,0,59,_tm.tm_sec);
    if ( res == Error )
    {
        return false;
    }
    return true;
}


time_t ParseZoneShift(const char *&pszDate, tm& _tm)
{
    int nZoneShift = 0;
    int nZoneHour = 0;
    int nZoneMin = 0;

    if ( *pszDate != '\0' )
    {
        switch (*pszDate) {
        case '+':
            nZoneShift = -1;
            break;
        case '-':
            nZoneShift = 1;
            break;
        case 'Z':
            nZoneShift = 0;
            break;
        default:
            return time_t(-1);
        }
        pszDate++;
        if ( ParseFixLenNumber(pszDate,2,0,59,nZoneHour) != Ok)
        {
            return time_t(-1);
        }
        if (*pszDate == '\'') {
            pszDate++;
            if ( ParseFixLenNumber(pszDate,2,0,59,nZoneMin) != Ok)
            {
                return time_t(-1);
            }
            if (*pszDate != '\'')
            {
                return time_t(-1);
            }
            pszDate++;
        }
    }
    if ( *pszDate != '\0' )
    {
        return time_t(-1);
    }

    // convert to
    time_t m_time = timegm(&_tm);
    if ( m_time == -1 )
    {
        return m_time;
    }

    m_time += nZoneShift * (nZoneHour * 3600 + nZoneMin * 60);
    return m_time;
}

}

namespace PoDoFo {

PdfDate::PdfDate()
    : m_bValid( false )
{
    m_time = time( &m_time );
    CreateStringRepresentation();
}

PdfDate::PdfDate( const time_t & t )
    : m_time( t ), m_bValid( false )
{
    CreateStringRepresentation();
}

PdfDate::PdfDate( const PdfString & sDate )
    : m_time( -1 ), m_bValid( false )
{
    if ( !sDate.IsValid() )
    {
        m_szDate[0] = 0;
        return;
    }

    strncpy(m_szDate,sDate.GetString(),PDF_DATE_BUFFER_SIZE);

    struct tm _tm/*{}*/;
	memset((void*)&_tm,0,sizeof(struct tm));
    _tm.tm_mday = 1;

    const char * pszDate = sDate.GetString();
    if ( pszDate == NULL ) return;
    if ( *pszDate == 'D' ) {
        pszDate++;
        if ( *pszDate++ != ':' ) return;
    }

    // year is not optional
    if ( ParseFixLenNumber(pszDate,4,0,9999,_tm.tm_year) != Ok)
        return;
    _tm.tm_year -= 1900;

    // all other values are optional, if not set they are 0-init (except mday)
    if(!ParseOptionalFields(pszDate, _tm))
    {
        return;
    }

    // zone is optional
    m_time = ParseZoneShift(pszDate, _tm);
    m_bValid = ( m_time != -1);
}

PdfDate::~PdfDate()
{
}

void PdfDate::CreateStringRepresentation()
{
    const int   ZONE_STRING_SIZE = 6;
    const char* INVALIDDATE     = "INVALIDDATE";

    char szZone[ZONE_STRING_SIZE];
    char szDate[PDF_DATE_BUFFER_SIZE];

    struct tm* pstm = localtime( &m_time );
    if( !pstm )
    {
        std::ostringstream ss;
        ss << "Invalid date specified with time_t value " << m_time << "\n";
        PdfError::DebugMessage( ss.str().c_str() );
        strcpy( m_szDate, INVALIDDATE );
        return;
    }

    struct tm stm = *pstm;

#ifdef _WIN32
    // On win32, strftime with %z returns a verbose time zone name
        // like "W. Australia Standard time". We use TIME_ZONE_INFORMATION
        // instead.

        TIME_ZONE_INFORMATION timezoninfo;
        GetTimeZoneInformation(&timezoninfo);

        // TODO: Handle bias minutes
        int bias = -(timezoninfo.Bias + (pstm->tm_isdst ? timezoninfo.DaylightBias : timezoninfo.StandardBias)) / 60;
        snprintf(szZone, ZONE_STRING_SIZE, "%+03d", bias);
#else
    if( strftime( szZone, ZONE_STRING_SIZE, "%z", &stm ) == 0 )
    {
        std::ostringstream ss;
        ss << "Generated invalid date from time_t value " << m_time
           << " (couldn't determine time zone)\n";
        PdfError::DebugMessage( ss.str().c_str() );
        strcpy( m_szDate, INVALIDDATE );
        return;
    }
#endif

    // only the first 3 characters are important for the pdf date representation
    // e.g. +01 instead off +0100
    szZone[3] = '\0';

    if( strftime( szDate, PDF_DATE_BUFFER_SIZE, "D:%Y%m%d%H%M%S", &stm ) == 0 )
    {
        std::ostringstream ss;
        ss << "Generated invalid date from time_t value " << m_time
           << "\n";
        PdfError::DebugMessage( ss.str().c_str() );
        strcpy( m_szDate, INVALIDDATE );
        return;
    }

    snprintf( m_szDate, PDF_DATE_BUFFER_SIZE, "%s%s'00'", szDate, szZone );
    m_bValid = true;
}

};
