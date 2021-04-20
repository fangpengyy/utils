#include "buffile.h"
#include "szconf.h"

#include <time.h>
#include <sched.h>



void* OnWrite(void* p)
{
     BufFile* pbuffile = (BufFile*)p;
     char buf[1024];
     int i =0;
     int n = 0;
     BufIndx* bufindex = nullptr;

     while(1) {
        n =  sprintf(buf, " test %d time=%d\n", i++, (int)time(0));
        
	pbuffile->Write(bufindex, buf , n);

//	printf("%s", buf);

//        sleep(1);
	usleep(10000);
     }

}

bool GetExecPath(std::string& path)
{
    char szPath[256] = {0};
    if (readlink("/proc/self/exe", szPath, sizeof(szPath)) <= 0)
        return false;

    char* p1 = strrchr(szPath, '/');
    if (p1 == nullptr)
        return false;
    *(++p1) = '\0';
    path = szPath;
    return true;
}


int test_conf()
{
    std::string path;
    if(! GetExecPath(path)) {
       printf(" path error!");
       return 0;
    }

    std::string filename = path;
    filename += "test.cfg";
    SZConfig szfile;

    if (szfile.Open(filename.c_str()) != 0){
        printf(" Open %s error!", filename.c_str());
        return 0;
    }

    std::string str;
    str.reserve(128);
    szfile.GetDataStr(str);
    printf("%s\n", str.c_str());

    /*
    int id = szfile.ReadInt("id");
    const char*name = szfile.ReadString("name");
    printf("id=%d, name=%s\n", id, name);

    std::vector<std::string> vecBuf;
    szfile.ReadArray("kk", vecBuf);
    for (int i = 0; i < vecBuf.size(); ++i) {
       printf("%s\n", vecBuf[i].c_str());
    }
    */
}

int main()
{
    while (1)
    {	   
        printf(" 1:test_conf; 2: test_buffile; \n input:");	
        char ch = getchar();

        switch(ch)
        {
	    case '1': 
            {	
	       test_conf();
               break;
	    }
       

	    case '2':
	    {
                 BufFile bfile;
                 bfile.Open("./", "ff.txt", 1, 1024, 10);

                 pthread_t th;
                 pthread_create(&th, nullptr, OnWrite, &bfile);
                 pthread_detach(th);

                 pthread_t th1;
                 pthread_create(&th1, nullptr, OnWrite, &bfile);
                 pthread_detach(th1);
                 break;
            }

	    case '3':
	    {
                int min_v = sched_get_priority_min (SCHED_RR); //SCHED_OTHER);
                int max_v = sched_get_priority_max (SCHED_RR); //SCHED_OTHER);  
		printf("min_v=%d max_v=%d\n",  min_v, max_v);   

                int policy;
                struct sched_param param;
                pthread_getschedparam(pthread_self(),&policy,&param);
               
	                
	
		if(policy == SCHED_OTHER)
                   printf("SCHED_OTHER  %d\n", policy);
                if(policy == SCHED_RR)
                    printf("SCHED_RR %d \n", policy);
                if(policy==SCHED_FIFO)
                    printf("SCHED_FIFO %d\n", policy);

		pthread_attr_t attr,attr1,attr2;
                 pthread_attr_init(&attr2);
                param.sched_priority = 51;
                pthread_attr_setschedpolicy(&attr2,SCHED_RR);

	      	pthread_attr_setschedparam(&attr2,&param);
                pthread_attr_setinheritsched(&attr2,PTHREAD_EXPLICIT_SCHED);

		//pthread_create(&ppid1,&attr2,(void *)Thread1,NULL);

		break;    
	    }

	    case 'q':
	    {
	       printf("\nexit\n");
	       return 0;
	    }
        }
   }

    printf("over\n");
    return 0;
}
