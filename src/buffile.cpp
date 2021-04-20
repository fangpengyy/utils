#include "buffile.h"
#include <stdlib.h>





#define def_init(locker) {\
    locker = 0;\
}

#define def_lock(locker) {\
    while (__sync_lock_test_and_set(&(locker),1)) {}\
}

#define def_unlock(locker) {\
    __sync_lock_release(&(locker));\
}


void SLink::Put(void * p)
{
    Link* link = (Link*)p;	
    def_lock(spin);
    link->next = nullptr;
    tail->next = link;
    tail = link;
    count;
    def_unlock(spin);
}

void* SLink::Pop()
{
    Link* link = nullptr;
    def_lock(spin);
    link = (Link*)head.next;
    if (link) {
        head.next = link->next;
        if (head.next == nullptr) 
	    tail = &head;	
	--count;
    } 
    def_unlock(spin);
    return link;
}

//---------------

DLink::DLink()
{
    head = nullptr;
    tail = nullptr;
    spin = 0;
}

void DLink::Put(void * p)
{
    Link* link = (Link*)p;
    link->next = nullptr;
    link->prev = nullptr;

    def_lock(spin);
    if (tail == nullptr) {
        head = link;
        tail = link;
    }
    else {	    
        tail->next = link;
        link->prev = tail;
        tail = link;
    }

    def_unlock(spin);
}

int DLink::Remove(void* p)
{
    Link* link = (Link*)p;	

    if (head == link) {
        head = (Link*)link->next;
        if (head == nullptr)
            tail = nullptr;
        else {
            head->prev = nullptr;
	}	 
    }
    else {
        if (tail == p) {
	    tail = (Link*)link->prev;	
	    if (tail == nullptr)
	        head = nullptr;
            else	    
	        tail->next = nullptr;
	}
        else {
            Link* prev = (Link*)link->prev;
            Link* next = (Link*)link->next;
            if (prev)
	        prev->next = link->next;
	    else
		return -1;    
	    if (next)
	        next->prev = link->prev;
	    else
		return -2;    
	}	
    }
    return 0;
}


//---------

BufFile::BufFile()
{

}

BufFile::~BufFile()
{
   if (_pbuf) {
       free(_pbuf);
       _pbuf = nullptr;
   }

   if (_useable_buf) {
       delete[] _useable_buf;
       _useable_buf = nullptr;
   }
}

int BufFile::Open(const char* path, const char* filename, int id, int buf_size, int buf_count)
{
    memcpy(_path, path, sizeof(_path));
    memcpy(_filename, filename, sizeof(_filename));

    _buf_count = buf_count;
    _buf_size = buf_size;   
    _pbuf = (char*)malloc(buf_size * buf_count);
    _useable_buf = new BufIndx[_buf_count];
    
    for (int i = 0; i < _buf_count; ++i) {
        _useable_buf[i].index = i;
	_useable_buf[i].buf_size = buf_size;
	_useable_buf[i].pbuf = &_pbuf[buf_size * i];
        _useable_buf[i].w_pos = 0;
	_idle_link.Put(&_useable_buf[i]);
    }

    _id = id;
    _w_buf_index = 0;
    _enable = true;
    _stop = false;

    int ret = pthread_create(&_th, nullptr, OnWriteFile, this);
    if (ret != 0) {
        return -1;
    }

    return 0;
}


BufIndx* BufFile::GetIdleBuf()
{
    int i = 0;	
    void* p = nullptr;	
    do {	
       p = _idle_link.Pop();
       if (p == nullptr) {
           if (++i % 100)
              usleep(10);
	    
	   continue;
       }
    } while (0);
    ((BufIndx*)p)->sectime = time(0);
    return (BufIndx*)p;
}

void BufFile::Put(BufIndx* p)
{
    _useable_link.Put(p);
}


int BufFile::Write(BufIndx*& bufindex, char* buf, uint32_t size)
{
    if (!_enable)
       return -1;
    
_lb1:   
    if (bufindex == nullptr) {
        bufindex =  GetIdleBuf();
        _work_link.Put(bufindex);
        //printf("get _idle_link \n");	
    }

    if (bufindex == nullptr) {
	printf(" bufindex == nullptr line=%d ", __LINE__);    
        return -2;
    }

    def_lock(_work_link.spin);
    int ret = bufindex->Write(buf, size);

    if (ret == -1) {
        if (_work_link.Remove(bufindex) == 0) 
            _useable_link.Put(bufindex);    
        def_unlock(_work_link.spin);

	bufindex = nullptr;
	//printf("put _useable_link \n");
	goto _lb1;
    }

    def_unlock(_work_link.spin);
    return 0;
}

void BufFile::Close()
{
    _stop = true;
    pthread_join(_th, nullptr);
}


void* BufFile::OnWriteFile(void* param)
{
    BufFile* pBufFile = (BufFile*)param;
    return pBufFile->DoWrite();
}

void* BufFile::DoWrite()
{
     int n = 0;	
     int w_count = -1;
     char szfile[512];
     FILE* file = nullptr;
     int tick = 0;
     uint32_t last_time = time(0);

     while(1) {	
        if (w_count == -1 || w_count > 1000000){
            sprintf(szfile, "%s%s-%d-%d", _path, _filename, _id, n);
            n++;
	    if (file) 
	        fclose(file);
            file = fopen(szfile, "wb");
            if (file == nullptr) {
                _enable = false;
		return nullptr;
	    }
            tick = 0;
	    w_count = 0;
        }

	BufIndx* bufindex = (BufIndx*)_useable_link.Pop();
	if (bufindex) {
            bufindex->Save(file);
            w_count++;
            _idle_link.Put(bufindex);

	   // printf("save count=%d\n", w_count);
	    continue;
	}


        if (time(0) - last_time > 10) {
	    def_lock(_work_link.spin);
	    Link* node = (Link*)_work_link.head;

            while (node != nullptr) {
                BufIndx* bufindex = (BufIndx*)node;
	        bufindex->Save(file);
                w_count++;
                node = (Link*)node->next;      

	//	printf("delay w_count=%d\n", w_count);
            }	
	    def_unlock(_work_link.spin);
	    last_time = time(0);
        }

        if (++tick % 100 == 0) {
            usleep(100);
	}

	if (_stop){
            _enable = false;		
	    if (file)
                fclose(file);
	    break;  
	}   
    }

    return nullptr;
}



