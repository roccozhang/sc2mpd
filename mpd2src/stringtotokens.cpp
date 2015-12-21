/* Copyright (C) 2015 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "stringtotokens.h"

using namespace std;

void stringToTokens(const string& str, vector<string>& tokens,
                    const string& delims, bool skipinit)
{
    string::size_type startPos = 0, pos;

    if (skipinit && 
        // Skip initial delims, return empty if this eats all.
	(startPos = str.find_first_not_of(delims, 0)) == string::npos) {
	return;
    }
    while (startPos < str.size()) { 
        // Find next delimiter or end of string (end of token)
        pos = str.find_first_of(delims, startPos);

        // Add token to the vector and adjust start
	if (pos == string::npos) {
	    tokens.push_back(str.substr(startPos));
	    break;
	} else if (pos == startPos) {
	    // Dont' push empty tokens after first
	    if (tokens.empty())
		tokens.push_back(string());
	    startPos = ++pos;
	} else {
	    tokens.push_back(str.substr(startPos, pos - startPos));
	    startPos = ++pos;
	}
    }
}
