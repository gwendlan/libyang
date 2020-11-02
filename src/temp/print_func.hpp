#ifndef PRINT_FUNC_H_
#define PRINT_FUNC_H_

#include <vector>
#include <string>
#include <stdarg.h>
#include <initializer_list>
extern "C"
{
#include "new.h"
}

namespace Out
{

using std::vector;
using std::string;

class VecStr
{
    public:

    VecStr(): data(){}

    VecStr(std::initializer_list<string> l): data(l){}

    VecStr& operator=(std::initializer_list<string> l)
    {
        data = l;
        return *this;
    }

    bool operator==(VecStr const& other) const
    {
        return data == other.data;
    }

    friend std::ostream& operator<<(std::ostream& os, VecStr const& out)
    {
        os << string("{");
        if(out.data.empty()) {
            os << string("}");
            return os;
        }
        for(auto iter = out.data.begin(); iter != std::prev(out.data.end()); ++iter)
            os << *iter << string(",");

        os << *std::prev(out.data.end()) << string("}");
        return os;
    }

    void push_back(string const& val)
    {
        data.push_back(val);
    }

    string& back()
    {
        return data.back();
    }

    bool empty() const
    {
        return data.empty();
    }

    void clear()
    {
        data.clear();
    }

    private:
    vector<string> data;
};

void print_VecStr(void* out, int arg_count, va_list ap)
{
    VecStr& m_out = *(VecStr*)out;

    if(arg_count <= 0)
        return;

    for(int i = 0; i < arg_count; i++) {

        char* ptr = va_arg(ap, char*);
        if(ptr == NULL || ptr[0] == '\0')
            continue;
        string str = string(ptr);

        /* if str is space token */
        if(str[0] == ' ') { 
            /* if last stored token was space token */
            if(!m_out.empty() && m_out.back()[0] == ' ')
                /* concatenate space token to space token */
                m_out.back() += str;
            else
                /* add space token behind word token */
                m_out.push_back(str);
        } else {
            /* str is word token */
            /* if last stored token was space token */
            if(!m_out.empty() && m_out.back()[0] == ' ')
                /* add word token behind space token */
                m_out.push_back(str);
            else
                /* concatenate word token with word token */
                if(m_out.empty())
                    m_out.push_back(str);
                else
                    m_out.back() += str;
        }
    }
}

void print_string(void* out, int arg_count, va_list ap)
{
    string& m_out = *(string*)out;

    if(arg_count <= 0)
        return;

    for(int i = 0; i < arg_count; i++) {

        char* ptr = va_arg(ap, char*);
        if(ptr == NULL || ptr[0] == '\0')
            continue;
        m_out += string(ptr);
    }
}

using VecLines = VecStr;

void print_vecLines(void* out, int arg_count, va_list ap)
{
    VecLines& m_out = *(VecLines*)out;

    if(arg_count <= 0)
        return;

    for(int i = 0; i < arg_count; i++) {

        char* ptr = va_arg(ap, char*);
        if(ptr == NULL || ptr[0] == '\0')
            continue;

        if(ptr[0] == trd_separator_linebreak[0])
            m_out.push_back("");
        else if(!m_out.empty())
            m_out.back() += string(ptr);
        else
            m_out.push_back(string(ptr));
    }
}

}

#endif
