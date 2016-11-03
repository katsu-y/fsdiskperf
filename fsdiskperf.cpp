//
// fssamples - fsdiskperf
// 2016.11.03
//
#include <SDKDDKVer.h>
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>

//  "9223372036854775807" (LLONG_MAX as string)
// "18446744073709551615" (ULLONG_MAX as string)
#define STR_ULLONG_MAX  (20)
#define CSZ_ULLONG_MAX  (STR_ULLONG_MAX+1)
// "18,446,744,073,709,551,615" (STR_ULLONG_MAX plus Thousand Separator)
#define STR_ULLONG_MAX_THOUSAND_SEP (STR_ULLONG_MAX+6)
#define CSZ_ULLONG_MAX_THOUSAND_SEP (STR_ULLONG_MAX_THOUSAND_SEP+1)
// "512409557:36:10.955161" (ULLONG_MAX as time format string)
#define STR_TIME_MAX    (22)
#define CSTR_TIME_MAX   (STR_TIME_MAX+1)

LPWSTR commaFormatString(ULONGLONG val,LPWSTR pszOut,int cchOut)
{
	NUMBERFMT numfmt = {0};
	WCHAR szValue[CSZ_ULLONG_MAX];
	swprintf_s(szValue,ARRAYSIZE(szValue),L"%I64u",val);

    numfmt.NumDigits     = 0;
    numfmt.LeadingZero   = 0;
    numfmt.Grouping      = 3; 
    numfmt.lpDecimalSep  = L"."; 
    numfmt.lpThousandSep = L","; 
    numfmt.NegativeOrder = 0;
	GetNumberFormat(LOCALE_USER_DEFAULT,0,szValue,&numfmt,pszOut,cchOut);

	return pszOut;
}

LPWSTR durationFormatString(ULONGLONG val,LPWSTR pszOut,int cchOut)
{
#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	GetDurationFormat(LOCALE_USER_DEFAULT,0,NULL,val,L"H:mm:ss.ffffff",pszOut,cchOut);
#else
	swprintf_s(pszOut,cchOut,L"0x%I64X",val);
#endif
	return pszOut;
}

LPWSTR datetimeFormatString(ULONGLONG val,LPTSTR pszOut,int cchOut,BOOL bUTC)
{
	SYSTEMTIME st;
	FILETIME ftLocal;

	if( bUTC )
	{
		FileTimeToSystemTime((FILETIME*)&val,&st);
	}
	else
	{
		FileTimeToLocalFileTime((FILETIME*)&val,&ftLocal);
		FileTimeToSystemTime(&ftLocal,&st);
	}

	int cch;
	cch = GetDateFormat(LOCALE_USER_DEFAULT,
				0,
				&st, 
				NULL,
				pszOut,cchOut);

	if( cch > 0 )
	{
		pszOut[cch-1] = L' ';

		GetTimeFormat(LOCALE_USER_DEFAULT,
					TIME_FORCE24HOURFORMAT,
					&st, 
					NULL,
					&pszOut[cch],cchOut-cch);
	}

	return pszOut;
}

void printLargeInteger(char *title, LARGE_INTEGER& li)
{
	WCHAR sz[CSZ_ULLONG_MAX_THOUSAND_SEP];
	printf("%-*s : %*S\n",15,title,STR_ULLONG_MAX_THOUSAND_SEP,
		commaFormatString(li.QuadPart,sz,ARRAYSIZE(sz)));
}

void printDword(char *title, DWORD dw)
{
	WCHAR sz[CSZ_ULLONG_MAX_THOUSAND_SEP];
	printf("%-*s : %*S\n",15,title,STR_ULLONG_MAX_THOUSAND_SEP,
		commaFormatString(dw,sz,ARRAYSIZE(sz)));
}

void printLargeIntegerTime(char *title, LARGE_INTEGER& li)
{
	WCHAR sz[CSZ_ULLONG_MAX_THOUSAND_SEP];
	printf("%-*s : %*S\n",15,title,STR_ULLONG_MAX_THOUSAND_SEP,
		durationFormatString(li.QuadPart,sz,ARRAYSIZE(sz)));
}

void printQueryTime(char *title, LARGE_INTEGER& li)
{
	WCHAR sz[100];
	datetimeFormatString(li.QuadPart,sz,ARRAYSIZE(sz),FALSE);
	printf("%-*s : %S\n",15,title,sz);
}

HANDLE OpenTarget(PCWSTR lpName)
{
	HANDLE hTarget;
	hTarget = CreateFile(lpName,
				0,
				FILE_SHARE_READ|FILE_SHARE_WRITE,	
				NULL,
				OPEN_EXISTING,
				0,
				NULL);
	return hTarget;
}

int __cdecl wmain(int argc, WCHAR* argv[])
{
	int nRet = -1;

	if( argc < 2 )
	{
		printf("usage: fsdiskperf <target>\n"
			   "\n"
			   "<target>  Volume or Device name.\n"
			   "          e.g. \\\\?\\C:\n"
			   "               \\\\?\\PhysicalDevice1\n"
			   "               \\\\?\\Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\n"
			   "\n");
		return 0;
	}


	LPWSTR pszTarget = argv[1];

	HANDLE hTarget = OpenTarget(pszTarget);

	if( hTarget == INVALID_HANDLE_VALUE )
	{
		printf("error: Open volume/device (%u)\n",(nRet = GetLastError()));
		return nRet;
	}

	DISK_PERFORMANCE dp = {0};
	DWORD cbOutBufferSize = sizeof(DISK_PERFORMANCE);
	DWORD cbBytesReturned = 0;
	LPVOID lpOutBuffer = (LPVOID)&dp;

	if( DeviceIoControl(hTarget,IOCTL_DISK_PERFORMANCE,NULL,0,lpOutBuffer,cbOutBufferSize,&cbBytesReturned,NULL) )
	{
		printf("\n");

		printf("Storage Manager : %S (Device #%u)\n",dp.StorageManagerName,dp.StorageDeviceNumber);
		printQueryTime( "Query Time", dp.QueryTime );

		printf("\n");

		printDword( "Read Count", dp.ReadCount );
		printDword( "Write Count", dp.WriteCount );
		printLargeInteger( "Bytes Reads", dp.BytesRead );
		printLargeInteger( "Bytes Written", dp.BytesWritten );
		printLargeIntegerTime( "Read Time", dp.ReadTime );
		printLargeIntegerTime( "Write Time", dp.WriteTime );
		printLargeIntegerTime( "Idle Time", dp.IdleTime );
		printDword( "Queue Depth", dp.QueueDepth );
		printDword( "Split Count", dp.SplitCount );

		nRet = ERROR_SUCCESS;
	}
	else
	{
		printf("error: DeviceIoControl (%u)\n",(nRet = GetLastError()));
	}

	CloseHandle(hTarget);

	return nRet;
}
