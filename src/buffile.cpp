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



//test out debug info

#define DEF_DEBUG   0




void SLink::Put(void * p)
{
    if (p == nullptr)
        return;

    Link* link = (Link*)p;	
    link->next = nullptr;

    def_lock(spin);
    tail->next = link;
    tail = link;

    ++count;
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
    if (p == nullptr)
       return;

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

   if (_arrbuf_indx) {
       free(_arrbuf_indx);
       _arrbuf_indx = nullptr;
       _buf_count = 0;
   }
}

int BufFile::Open(const char* path, const char* filename, int id, int buf_size, int buf_count)
{
    memcpy(_path, path, sizeof(_path));
    memcpy(_filename, filename, sizeof(_filename));

    _buf_count = buf_count;
    _buf_size = buf_size;   
    _pbuf = (char*)malloc(buf_size * buf_count);
  
    _arrbuf_indx = (BufIndx*)malloc(sizeof(BufIndx) * _buf_count);
    
    for (int i = 0; i < _buf_count; ++i) {
        _arrbuf_indx[i].index = i;
	_arrbuf_indx[i].buf_size = buf_size;
	_arrbuf_indx[i].pbuf = &_pbuf[buf_size * i];
        _arrbuf_indx[i].w_pos = 0;

	_idle_link.Put((void*)(_arrbuf_indx + i));
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
    BufIndx* buf_indx = nullptr;

    do {	
        p = _idle_link.Pop();
        if (p == nullptr) {
	    if (++i % 100)
                usleep(10);
	}
	    
    } while (p == nullptr);
    
    if (p) {
        buf_indx = (BufIndx*)p;
        buf_indx->sectime = time(0);
    }

    return buf_indx;
}

int BufFile::Write(BufIndx*& bufindex, char* buf, uint32_t size)
{
    if (!_enable)
       return -1;
   
_lb1:   
    if (bufindex == nullptr) {
        bufindex = GetIdleBuf();
	_work_link.Put(bufindex);    	
    }

    def_lock(_work_link.spin);
    int ret = bufindex->Write(buf, size);

    if (ret == -1) {
        //bufindex full
        if (_work_link.Remove(bufindex) == 0) 
            _store_link.Put(bufindex);    
        def_unlock(_work_link.spin);

	bufindex = nullptr;
	goto _lb1;
    }

    def_unlock(_work_link.spin);
    return 0;
}

void BufFile::Close()
{
#if (DEF_DEBUG == 1)	
    printf("BufFile::Close\n");	
#endif    
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

	BufIndx* bufindex = (BufIndx*)_store_link.Pop();
	if (bufindex) {
            bufindex->Save(file);
            w_count++;
            _idle_link.Put(bufindex);
#if (DEF_DEBUG == 1)
	    printf("save count=%d\n", w_count);
#endif
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
#if (DEF_DEBUG == 1)
		printf("delay w_count=%d\n", w_count);
#endif     
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

#if (DEF_DEBUG == 1)     
    printf(" %s exit\n", __func__);  
#endif    
    return nullptr;
}



