#include <stdio.h>
#include <iostream>

class unix_exception: public std::exception
{
public:
    explicit unix_exception(const char* message): msg(message)
    {
        
    }
    
    explicit unix_exception(const std::string& message): msg(message)
    {
        
    }
        
    virtual const char* what() const noexcept
    {
        return this->msg.c_str();
    }
    
private:
    std::string msg;
};
