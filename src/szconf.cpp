#include "szconf.h"
#include <errno.h>
#include <stdio.h>



SZConfig::SZConfig()
{

}

SZConfig::~SZConfig()
{
    TSzMap::iterator it = _map.begin();
    while (it != _map.end()) {
       it->second.Clean();
       ++it;
    }
    _map.clear();
}

int SZConfig::Open(const char* fileName)
{
    _sfilename = fileName;	
    FILE* file = fopen(fileName, "r");   
    if (file == nullptr) {
       printf("%s open %s error=%s\n", __func__, fileName, strerror(errno));
       return -1; 
    }

    size_t len = 1024;
    char szBuf[len];
    char* pBuf = &szBuf[0];
    char* pNext = nullptr;
    char* pKey = nullptr;
    char* pValue = nullptr;

    while (!feof(file)) {
        if (getline(&pBuf, &len, file) == -1) {
           break;    		
	}
        pBuf[len] = 0;

	pNext = ParseLine(pBuf, &pKey, '='); 
	if (pNext) {
           ++pNext;
	   if (ParseLine(pNext, &pValue, ';')) {
               SZValue szValue(pValue);               
	       _map[pKey] = szValue;
	   }
	}
    } 
    fclose(file);
    return 0;
}

void SZConfig::WriteString(const char* name, const char* value)
{
    TSzMap::iterator it = _map.find(name);
    if (it != _map.end()) {
	it->second.Copy(value);
        return;
    }
    SZValue szValue((char*)value);
    _map[name] = szValue;
}

void SZConfig::WriteInt(const char* name, int value)
{
    char szBuf[16];
    int n = sprintf(szBuf, "%d", value);
    szBuf[n] = '\0';

    TSzMap::iterator it = _map.find(name);
    if (it != _map.end()) {
        it->second.Copy(szBuf);
        return;
    }
    SZValue szValue(szBuf);
    _map[name] = szValue;
}

void SZConfig::Remove(const char* name)
{
    TSzMap::iterator it = _map.find(name);
    if (it != _map.end()) {
        it->second.Clean();
        _map.erase(it);
    }
}

int SZConfig::Flush()
{
    FILE* file = fopen(_sfilename.c_str(), "wb+");
    if (file == nullptr) {
        printf("%s open %s error=%s\n", __func__, _sfilename.c_str(), strerror(errno));
        return -1;
    }

    if (_map.empty()){
        printf("%s  error map empty\n", __func__);
	fclose(file);
        return 0;
    }
    
    const char* name = nullptr;
    char* value = nullptr;
    char szBuf[256];
    int n = 0;

    TSzMap::iterator it = _map.begin();
    while (it != _map.end()) {
        name = it->first.c_str();
	value = it->second.data;
	n = sprintf(szBuf,"%s=%s;\n", name, value);
        fwrite(szBuf, n, 1, file);
	it++;    
    }

    fclose(file);
    return 0;
}

const char* SZConfig::ReadString(const char* key, const char* defval)
{
    TSzMap::iterator it = _map.find(key);
    if (it != _map.end()) {
        SZValue& value = it->second;
        return value.data;
    }
    return defval;
}

int SZConfig::ReadInt(const char* key, int defval)
{
    TSzMap::iterator it = _map.find(key);
    if (it != _map.end()) {
        SZValue& value = it->second;
        return atoi(value.data);
    }
    return defval;
}

double SZConfig::ReadDouble(const char* key, double defval)
{
    TSzMap::iterator it = _map.find(key);
    if (it != _map.end()) {
        SZValue& value = it->second;
	return atof(value.data);
    }
    return defval;
}

int SZConfig::ReadArray(const char* key, std::vector<std::string>& vecValue)
{
    TSzMap::iterator it = _map.find(key);
    if (it != _map.end()) {
        SZValue& value = it->second;
        return ParseArray(value.data, vecValue);
    }
    return 0;
}

int SZConfig::ParseArray(char* buf, std::vector<std::string>& vecValue)
{
    char* p = buf;	
    char* p1 = nullptr;
    int len = 0;

    while (*p == '\0')
	++p;    

    while (*buf != 0) {
        if (*buf == ',') {
            p1 = buf;		
	    --buf;
            while (*buf == '\0')
	       --buf;	    
            
	    len = buf - p;
	    if (len > 0) {
                std::string str;
	        str.assign(p, len + 1);
	        vecValue.push_back(str);
            }
	    else {
                return vecValue.size();
	    }

	    while (*p1 == ',') 
               ++p1;

	    buf = ++p1;
	    while (*buf == '\0')
               ++buf;
	    p = buf;
	    continue;
        } 
        ++buf;
    }

    while (*buf == '\0' || *buf == ',')
       --buf;

    len = buf - p;
    if (len > 0) {
        std::string str;
        str.assign(p, buf - p + 1);
        vecValue.push_back(str);
    }
    return vecValue.size();
}

char* SZConfig::ParseLine(char* buf, char** token, char flag)
{
    char *p = nullptr;	
    while (*buf != 0) {
        if (*buf == ' ' || *buf == '\t')
            ++buf;
        else 
            break;
    }
    
    if (*buf == '#' || *buf == '\n')
        return nullptr;
    
    *token = buf;

    while (*buf != 0) {
        if (*buf == flag)
 	    break;     
        ++buf;
    }    
    p = buf;

    if (*buf == flag) {
        --buf; 	    
        while (*buf == ' ')
	    --buf;     
    }
    else 
        return nullptr;

    ++buf;
    *buf = 0;
    
    return p;
}

void SZConfig::GetDataStr(std::string& str)
{
    if (_map.size() == 0)
    {
        str = " "; 
        return; 	
    }	

    char buf[128];
    TSzMap::iterator it = _map.begin();
    while (it != _map.end()) {
        sprintf(buf, "%s=%s\n", it->first.c_str(), it->second.data); 
	str.append(buf);
	++it;
    }
}
