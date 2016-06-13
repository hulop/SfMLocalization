/*******************************************************************************
* Copyright (c) 2015 IBM Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*******************************************************************************/

#include "StringUtils.h"

using namespace std;

string hulo::trim(string s) {
	string a(s);
	string whitespace(" \t\f\v\n\r");

	size_t first = a.find_first_not_of(whitespace);
	if (first != string::npos) {
		a = a.substr(first);
	} else {
		return "";
	}

	size_t last = a.find_last_not_of(whitespace);
	if (last != string::npos) {
		a.erase(last + 1);
	}

	return a;
}

string hulo::stringTok(string &in) {
	string whitespace(" \t\f\v\n\r,");
	size_t outind = in.find_first_of(whitespace);
	string out;
	if (outind == string::npos) {
		out = string(in);
		in = "";
		return out;
	} else {
		out = in.substr(0, outind);
		in = in.substr(outind + 1);
		in = hulo::trim(in);
		return out;
	}
}
