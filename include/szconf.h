#ifndef __SZ_CONF_H__
#define __SZ_CONF_H__

#include <string>
#include <unordered_map>
#include <string.h>
#include <stdlib.h>
#include <vector>


/*
//example:

# key = value;
# id = 100;
# name = hello;
id = 123;
name = assd;
ip_name = 127.0.0.1;
float_name = 12.45 ;
array_name= 127.0.0.1 , 192.168.34.67 , 192.168.34.12 ,, ;
*/


struct SZValue
{
    uint16_t len{0};
    uint16_t size{0};
    char* data{nullptr};

    SZValue():len(0), size(0), data{nullptr} {
    
    }

    SZValue(char* buf):len(0), size(0), data{nullptr} {
        Copy(buf);  
    }

    void Copy(const char* buf) {
        int n = strlen(buf);
        if (n > size) {
	    if (data)  
	        free(data);
	  
	    size = 2 * n;    
            data = (char*)malloc(size);
        }
	
        if (data) { 
            len = n;     
            memcpy(data, buf, len);
            data[n] = '\0';
        }
    }

    void Clean() {
        if (size > 0) { 	    
            free(data);
            len = 0;
	    size = 0;
        }
    }	    
};

typedef std::unordered_map<std::string, SZValue>  TSzMap;

class SZConfig
{
public:
    SZConfig();
    ~SZConfig();    
    int Open(const char* fileName);

    void WriteString(const char* name, const char* value);
    void WriteInt(const char* name, int value);
    int Flush();

    void Remove(const char* name);

    const char* ReadString(const char* key);
    int ReadInt(const char* key);
    double ReadDouble(const char* key);
    int ReadArray(const char* key, std::vector<std::string>& vecValue);
    
    void GetDataStr(std::string& str);
private:
    int ParseArray(char* buf, std::vector<std::string>& vecValue);
    char* ParseLine(char* buf, char** token, char flag);
    TSzMap _map;
    std::string _sfilename;
};



#endif
