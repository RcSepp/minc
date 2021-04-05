#include <assert.h>
#include <stack>
#include "json.h"

namespace Json
{
	bool parse(std::istream& file, Value* out)
	{
		char c;
		bool escape = false, err = false;
		enum Mode
		{
			BEGIN, END, STRING, COLON, KEY, ANY, VALUE, ARRAY, LIST
		};
		std::stack<Mode> mode;
		mode.push(END);
		mode.push(BEGIN);
		std::stack<Value*> value;
		value.push(out);
		std::string *currentString, currentKey;
		while (!err && (c = file.get()) != EOF)
			switch (c)
			{
			case ' ': case '\t': case '\n': case '\r': break;
			default:
				switch (mode.top())
				{
				case BEGIN:
					switch (c)
					{
					case '{': mode.top() = KEY; break;
					default: err = true; break;
					}
					break;
				case END:
					err = true;
					break;
				case KEY:
					switch (c)
					{
					case '"': mode.top() = COLON; mode.push(STRING); currentString = &currentKey; break;
					case '}': mode.pop(); break;
					default: err = true; break;
					}
					break;
				case ANY:
					switch (c)
					{
					case '"': mode.top() = STRING; currentString = &value.top()->str; break;
					case '[': mode.top() = ARRAY; mode.push(ANY); value.top()->arr.push_back(Value()); value.push(&value.top()->arr.back()); break;
					case '{': mode.top() = KEY; break;
					case ']': mode.pop(); if (mode.top() != ARRAY) err = true; mode.pop(); value.pop(); value.top()->arr.clear(); break;
					case 'f':
						mode.pop();
						err = file.get() != 'a' || file.get() != 'l' || file.get() != 's' || file.get() != 'e';
						value.top()->str = "false";
						break;
					case 't':
						mode.pop();
						err = file.get() != 'r' || file.get() != 'u' || file.get() != 'e';
						value.top()->str = "true";
						break;
					default: if (std::isalnum(c)) *currentString += c; else err = true; mode.top() = VALUE; currentString = &value.top()->str; break;
					}
					break;
				case VALUE:
					if (std::isalnum(c)) *currentString += c; else err = true;
					break;
				case STRING:
					if (escape)
					{
						*currentString += c;
						break;
					}
					switch (c)
					{
					case '\\': escape = true; break;
					case '"': mode.pop(); break;
					default: *currentString += c; break;
					}
					break;
				case COLON:
					value.push(&(value.top()->lst[currentKey] = Value()));
					currentKey = "";
					switch (c)
					{
					case ':': mode.top() = LIST; mode.push(ANY); break;
					default: err = true; break;
					}
					break;
				case ARRAY:
					value.pop();
					switch (c)
					{
					case ',': mode.top() = ARRAY; mode.push(ANY); value.top()->arr.push_back(Value()); value.push(&value.top()->arr.back()); break;
					case ']': mode.pop(); break;
					default: err = true; break;
					}
					break;
				case LIST:
					value.pop();
					switch (c)
					{
					case ',': mode.top() = KEY; break;
					case '}': mode.pop(); break;
					default: err = true; break;
					}
					break;
				}
				escape = false;
			}
		assert(err == true || (mode.top() == END && value.top() == out));
		return !err;
	}
}