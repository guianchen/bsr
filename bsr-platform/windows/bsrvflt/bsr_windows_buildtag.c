﻿#include <string.h>

/* automatically generated. DO NOT EDIT. */
#define BUILD_VERSION ""
#define COMMIT ""
#define BUILD_USER ""
#define BUILD_HOST ""

const char *drbd_buildtag(void)
{
	if (strcmp(BUILD_VERSION, "") == 0)
	{
		return "";
	}
	else
	{
		return BUILD_VERSION " GIT-hash: " COMMIT " build by " BUILD_USER"@"BUILD_HOST", " __TIMESTAMP__;
	}
	
}
