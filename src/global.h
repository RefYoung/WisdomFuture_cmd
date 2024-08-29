#ifndef GLOBAL_H
#define GLOBAL_H

#include <QString>

#define g_configFilePath "config.ini"   //程序配置文件

//每个工控机连接有心跳包计数的属性heartCount
//1*60*7,工控机大概一分钟上传7条心跳包,使1个小时只保留一条正常心跳包,但保留所有有报警信息的心跳包
#define g_heartMaxCount 420

struct DataServer{
    QString address;//数据服务器地址
    QString sid;//数据服务sid
};

struct EquCodeAndId{
    QString id;
    QString table;
};

#endif // GLOBAL_H
