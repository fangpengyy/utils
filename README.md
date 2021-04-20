# utils


### 读取配置文件

```
    szcon.h szconf.cpp
    配置文件格式，见代码或test/test.cfg

```

###  日志文件（块写入）

```
    buffile.h buffile.cpp

``` 

### 测试

```
    在test目录执行：
    g++ -o main  main.cpp ../src/szconf.cpp ../src/buffile.cpp  -I../include -lpthread

```

