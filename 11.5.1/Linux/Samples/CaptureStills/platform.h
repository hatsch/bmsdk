/* -LICENSE-START-
** Copyright (c) 2018 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation covered by
** this license (the "Software") to use, reproduce, display, distribute,
** execute, and transmit the Software, and to prepare derivative works of the
** Software, and to permit third-parties to whom the Software is furnished to
** do so, all subject to the following:
**
** The copyright notices in the Software and this entire statement, including
** the above license grant, this restriction and the following disclaimer,
** must be included in all copies of the Software, in whole or in part, and
** all derivative works of the Software, unless such copies or derivative
** works are solely in the form of machine-executable object code generated by
** a source language processor.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
** -LICENSE-END-
*/

#pragma once

#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <functional>
#include <stdint.h>
#include "DeckLinkAPI.h"

HRESULT GetDeckLinkIterator(IDeckLinkIterator **deckLinkIterator);
HRESULT GetDeckLinkVideoConversion(IDeckLinkVideoConversion **deckLinkVideoConversion);


#define dlbool_t	bool
#define dlstring_t	const char*

// DeckLink String conversion functions
const auto DeleteString = [](dlstring_t dl_str) { free((void*)dl_str); };

const auto DlToStdString = [](dlstring_t dl_str) -> std::string { return dl_str; };

const auto StdToDlString = [](std::string std_str) -> dlstring_t {
	return strcpy((char*)malloc(std_str.length() + 1), std_str.c_str());
};

const auto DlToCString = [](dlstring_t dl_str) -> const char * { return dl_str; };

const auto IsPathDirectory = [](std::string path_str) -> bool {
	struct stat dirStat;
	return (stat(path_str.c_str(), &dirStat) == 0) && ((dirStat.st_mode & S_IFMT) == S_IFDIR);
};


