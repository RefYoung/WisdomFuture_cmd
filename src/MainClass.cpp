#include "MainClass.h"
MainClass::MainClass(QObject *parent)
    : QObject{parent}
{
    init();//调用程序初始化函数
}

//程序初始化函数,请勿在在此函数调用前,调用任何其他自定义方法
void MainClass::init()
{
    m_updateLogHours=-1;//默认不打印日志信息
    m_dbClearDays=-1;//默认不清理插入信息表

    bool loadState=loadConfig(g_configFilePath);//加载配置文件
    if(loadState==false){
        qDebug()<<u8"未找到配置文件,已创建空配置文件,请配置好后再次启动程序.";
        createConfig(g_configFilePath);
        return;
    }

    //加载配置文件成功
    qDebug()<<Qt::endl<<u8"启动时间:"+QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    qDebug()<<u8"配置文件加载成功.";

    //初始化数据库连接
    m_db=QSqlDatabase::addDatabase("QMYSQL");
    m_db.setHostName(m_databaseAddress);
    m_db.setPort(m_databasePort.toInt());
    m_db.setDatabaseName(m_databaseName);
    m_db.setUserName(m_userName);
    m_db.setPassword(m_userPswd);

    //初始化http请求管家
    m_networkManager=new QNetworkAccessManager(this);

    if(m_db.open()){
        qDebug()<<u8"数据库已连接!";
    }else{
        qDebug()<<u8"数据库连接失败.错误信息:"<<m_db.lastError().text();
        exit(0);
    }

    m_tcpServer=new QTcpServer(this);
    //初始化监听对象
    if(m_tcpServer->listen(QHostAddress::Any,m_serverPort.toInt())){
        qDebug()<<u8"打开监听成功!";
    }else{
        qDebug()<<u8"打开监听失败,请检查配置.错误信息:"<<m_tcpServer->errorString();
        exit(0);
    }

    qDebug()<<u8"WebSocket地址:"<<m_webSocketUrl;
    qDebug()<<u8"服务器地址:"<<m_tcpServer->serverAddress().toString();
    qDebug()<<u8"服务器端口:"<<m_tcpServer->serverPort();
    qDebug()<<u8"数据库地址:"<<m_db.hostName();
    qDebug()<<u8"数据库端口:"<<m_db.port();
    qDebug()<<u8"数据库名称:"<<m_db.databaseName();
    qDebug()<<u8"数据库用户名:"<<m_db.userName();

    if(m_updateLogHours<=0){
        qDebug()<<u8"连接统计周期: 不打印.";
    }else{
        qDebug()<<u8"连接统计周期: "+QString::number(m_updateLogHours)+" h";
    }

    if(m_dbClearDays<=0){
        qDebug()<<u8"日志清理周期: 不清理.";
    }else{
        qDebug()<<u8"日志清理周期: "+QString::number(m_dbClearDays)+" 天前.";
    }

    //绑定信号槽函数,当监听到新连接时调用新连接处理函数
    connect(m_tcpServer,SIGNAL(newConnection()),this,SLOT(onNewTcpSocketComming()));
    connect(&m_timer1,SIGNAL(timeout()),this,SLOT(onTimer1Out()));
    connect(&m_timer2,SIGNAL(timeout()),this,SLOT(onTimer2Out()));

    //如果打印日志信息时间间隔大于0才周期性打印日志信息
    if(m_updateLogHours>0){
        m_timer1.setInterval(m_updateLogHours*60*60*1000);
        m_timer1.start();
    }

    //如果清除插入日志信息的时间长度成员变量大于0,才周期性清理插入日志信息(1天1次).
    if(m_dbClearDays>0){
        m_timer2.setInterval(24*60*60*1000);
        m_timer2.start();
    }

    // connect(&m_websocket,SIGNAL(connected()),this,SLOT(onWebSocketConnected()));
    // connect(&m_websocket,SIGNAL(connected()),this,SLOT(onWebSocketDisConnected()));
    connect(&m_websocket,SIGNAL(textMessageReceived(QString)),this,SLOT(onWebSocketReadyRead(QString)));

    //周期性检查websocket连接
    m_websocket.open(QUrl(m_webSocketUrl));
    m_timer3.setInterval(60*1000);
    connect(&m_timer3,SIGNAL(timeout()),this,SLOT(onTimer3Out()));
    m_timer3.start();

    //周期性读取grm服务器数据,时间周期为11分钟
    m_timer4.setInterval(11*60*1000);
    connect(&m_timer4,SIGNAL(timeout()),this,SLOT(onTImer4Out()));
    m_timer4.start();

    //初始化grm参数名列表
    m_orderList.append(u8"仓1温度过高报警");
    m_orderList.append(u8"仓2温度过高报警");
    m_orderList.append(u8"仓3温度过高报警");
    m_orderList.append(u8"仓1搅拌机故障");
    m_orderList.append(u8"仓1加热器故障");
    m_orderList.append(u8"仓2搅拌机故障");
    m_orderList.append(u8"仓2加热器故障");
    m_orderList.append(u8"仓3搅拌机故障");
    m_orderList.append(u8"仓3加热器故障");
    m_orderList.append(u8"发酵仓控制系统急停");
    m_orderList.append(u8"仓1超重报警");
    m_orderList.append(u8"仓2超重报警");
    m_orderList.append(u8"仓3超重报警");
}

//加载配置文件
//参数1:QString filename  表示需要加载的配置文件的路径
bool MainClass::loadConfig(QString filename)
{
    QFile file(filename);
    if(!file.exists()){
        return false;
    }
    QSettings setting(filename,QSettings::IniFormat);
    setting.setIniCodec("UTF-8");

    //服务器部分
    // m_serverAddress=setting.value("server/hostAddress").toString();
    m_serverPort=setting.value("server/hostPort").toString();

    //数据库部分
    m_databaseAddress=setting.value("database/hostAddress").toString();
    m_databasePort=setting.value("database/hostPort").toString();
    m_databaseName=setting.value("database/databaseName").toString();
    m_userName=setting.value("database/databaseUserName").toString();
    m_userPswd=setting.value("database/databasePswd").toString();
    m_webSocketUrl=setting.value("database/webSocketUrl").toString();

    //日志更新部分
    if(setting.contains("log/updateHours")){
        m_updateLogHours=setting.value("log/updateHours").toInt();
    }
    if(setting.contains("log/dbClearDays")){
        m_dbClearDays=setting.value("log/dbClearDays").toInt();
    }


    return true;
}


//创建配置文件
//参数1:QString filePath 表示需要创建的配置文件的路径
void MainClass::createConfig(QString filePath)
{
    QSettings setting(filePath,QSettings::IniFormat);
    setting.setIniCodec("UTF-8");

    //服务器部分
    // setting.setValue("server/hostAddress",m_serverAddress);
    setting.setValue("server/hostPort",m_serverPort);

    //数据库部分
    setting.setValue("database/hostAddress",m_databaseAddress);
    setting.setValue("database/hostPort",m_databasePort);
    setting.setValue("database/databaseName",m_databaseName);
    setting.setValue("database/databaseUserName",m_userName);
    setting.setValue("database/databasePswd",m_userPswd);
    setting.setValue("database/m_webSocketUrl",m_webSocketUrl);


    //日志更新部分
    setting.setValue("log/updateHours",m_updateLogHours);
    setting.setValue("log/dbClearDays",m_dbClearDays);
}

//计算CRC16校验码
//参数一是需要计算计算的字节数组
//参数二是字节数组的长度
uint16_t MainClass::calcCrc16(const QByteArray &data, uint16_t count)
{
    uint16_t crc = 0xFFFF;
    uint8_t ch;

    for (int i = 0; i < count; i++) {
        ch = (unsigned char )(data[i]);
        crc ^= ch;

        for (int j = 0; j < 8; j++) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

//用于计算给定数据的CRC8校验值
unsigned char MainClass::calcCrc8(const QByteArray& arr,int len)
{
    uint8_t uCRC = 0x00;//CRC寄存器
    int j=0;
    for(int num=0;num<len;num++){
        uCRC = (arr[j++])^uCRC;//把数据与8位的CRC寄存器的8位相异或，结果存放于CRC寄存器。
        for(uint8_t x=0;x<8;x++){	//循环8次
            if(uCRC&0x80){	//判断最低位为：“1”
                uCRC = uCRC<<1;	//先左移
                uCRC = uCRC^0x07;	//再与多项式0x07异或
            }else{	//判断最低位为：“0”
                uCRC = uCRC<<1;	//右移
            }
        }
    }
    return uCRC;//返回CRC校验值
}

EquCodeAndId MainClass::queryEquIdAndTable(QString site_code,QString category_id)
{
    EquCodeAndId eca;
    QSqlQuery query(m_db);
    int res = query.exec(QString("select * from zh_wcinspection_equipment where site_code='%1' and category_id=%2;").arg(site_code,category_id));
    if(res){
        query.first();
        if(query.isValid()){
            eca.id=query.value("id").toString();
            eca.table=query.value("table").toString();
        }
    }
    return eca;
}


void MainClass::postGetSidAndDataServerAddress(QString equipment_id,QString account,QString pswd,QString tableName)
{
    QNetworkRequest request;
    QUrl url("http://www.yunplc.com:7080/exlog");
    request.setRawHeader("Content-Type","text/plain");
    request.setUrl(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, true);
    QNetworkReply* reply = m_networkManager->post(request,QString("GRM=%1\r\nPASS=%2").arg(account,pswd).toUtf8());
    reply->setProperty("equipment_id",equipment_id);
    reply->setProperty("table",tableName);
    reply->setParent(this);

    connect(reply,SIGNAL(finished()),this,SLOT(onPostGetSidAndDataServerAddressFinished()));
}

//释放所有套接字
int MainClass::releaseAll()
{
    int count=m_tcpSockets.count();
    for(auto i=m_tcpSockets.begin();i!=m_tcpSockets.end();++i){
        i.value()->close();
        i.value()->deleteLater();
    }
    m_tcpSockets.clear();
    return count;
}

//当有新连接到来时调用此槽函数
void MainClass::onNewTcpSocketComming()
{
    for(;m_tcpServer->hasPendingConnections();){
        QTcpSocket* _socket=m_tcpServer->nextPendingConnection();
        _socket->setProperty("key",QHostAddress(_socket->peerAddress().toIPv4Address()).toString()+":"+QString::number(_socket->peerPort()));
        m_tcpSockets[_socket->property("key").toString()]=_socket;
        _socket->setProperty("heartCount",0);
        qDebug()<<"["+QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")+"]"+_socket->property("key").toString()+"已连接.";
        connect(_socket,SIGNAL(disconnected()),this,SLOT(onSocketDisconnected()));//绑定信号和槽函数,当套接字断开时释放该套接字资源
        connect(_socket,SIGNAL(readyRead()),this,SLOT(onSocketReadyRead()));//绑定信号槽,当套接字有可读信息时调用信息处理函数

    }

    //检查所有套接字连接状态,如果不是连接或正在连接状态则断开
    for(auto item=m_tcpSockets.begin();item!=m_tcpSockets.end();++item){

        if(item.value()->state()==QAbstractSocket::UnconnectedState||item.value()->state()==QAbstractSocket::ClosingState){
            qDebug()<<u8"已释放无效套接字:"<<item.key();
            m_tcpSockets.erase(item);
            item.value()->close();
            item.value()->deleteLater();

        }
    }
}

//当有套接字连接断开时调用此槽函数
void MainClass::onSocketDisconnected()
{
    QTcpSocket* _socket=static_cast<QTcpSocket*>(sender());
    qDebug()<<"["+QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")+"]"+_socket->property("key").toString()+"已断开连接.";
    m_tcpSockets.remove(_socket->property("key").toString());//从套接字管理列表中移除该套接字
    _socket->close();//关闭套接字
    _socket->deleteLater();//释放套接字资源
}

//当套接字可读时调用此槽函数
void MainClass::onSocketReadyRead()
{
    QTcpSocket* _socket=static_cast<QTcpSocket*>(sender());
    QByteArray data=_socket->readAll();
    // qDebug()<<data.toHex(' ');

    if(data.size()<11){//如果包长不够则退出函数
        _socket->write("Insufficient data length,at least 11 bytes.");
        return;
    }

    QByteArray head,tail;
    head.append(0x7E);
    head.append(0x7E);
    tail.append(0x0A);
    tail.append(0x0D);

    if(!data.startsWith(head)||!data.endsWith(tail)){
        _socket->write("The packet header or packet tail check fails.");
        return;//检验包头包尾
    }
    uint16_t orgSize=QString(data.mid(3,1).toHex()+data.mid(2,1).toHex()).toInt(0,16);

    if(data.size()!=orgSize){//如果包长不够则退出函数
        _socket->write("The data frame length is not equal to the calculated length.");
        return;
    }
    uint16_t crc=calcCrc16(data.left(data.size()-4),data.size()-4);//计算校验位

    if((crc&0xFF)!=data.mid(data.size()-4,1).toHex().toInt(0,16)){//校验低位数据
        _socket->write("Data verification fails.");
        return;
    }
    if((crc>>8)!=data.mid(data.size()-3,1).toHex().toInt(0,16)){//校验高位数据
        _socket->write("Data verification fails.");
        return;
    }

    // qDebug()<<"["+QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")+"]"<<data.toHex(' ').toUpper();

    QByteArray packgData=data.mid(7,data.size()-11);//获取数据包

    //获取功能码
    uint16_t funCode=(data.at(5)<<8)+(data.at(4)&0xff);

    if(funCode==0x0600){//设备上传编号
        if(packgData.size()!=10)return;//如果设备编号数据长度不够则退出函数

        //绑定下位机套接字和其唯一编号
        _socket->setProperty("site_code",QString(packgData));

        //发送响应内容
        QByteArray sendData;
        sendData.append(head);
        sendData.append(0x0B);
        sendData.append((1-1));
        sendData.append(0x81);
        sendData.append(0x06);
        sendData.append(data.at(6));
        uint16_t crc=calcCrc16(sendData,sendData.size());
        sendData.append(crc&0xFF);
        sendData.append(crc>>8);
        sendData.append(tail);

        //发送数据
        _socket->write(sendData);

    }else if(funCode==0x0601){//设备上传状态数据
        //如果数据包长度不够则退出函数
        if(packgData.size()!=56){
            _socket->write("funCode:0x0601,The data length cannot be 56.");
            return;
        }

        QJsonObject wsSendObj;
        wsSendObj.insert("site_code",_socket->property("site_code").toString());
        int totalCnt=16;
        int idlepitCount=0;//空闲厕所数量
        int pitCount=0;//厕所总数
        QJsonArray wsSendArray;

        bool warnFlag=false;//报警标志,如果该心跳包有报警信息则则为真

        for(int i=0;i<totalCnt;++i){
            uint8_t data=packgData.at(i);

            if((data>>7)==1){
                //发送到websocket
                if(((data>>1)&0b111111)>0){
                    wsSendObj.insert("location",i+1);
                    wsSendObj.insert("type",QString::number(data,2));
                    m_websocket.sendTextMessage(QJsonDocument(wsSendObj).toJson());
                    warnFlag=true;
                }

                pitCount++;
                if((data&0x1)==0){
                    idlepitCount++;
                }
            }
        }

        uint32_t numuser_allCnt=QString(packgData.mid(19,1).toHex()+packgData.mid(18,1).toHex()+packgData.mid(17,1).toHex()+packgData.mid(16,1).toHex()).toULong(0,16);
        uint32_t numuser_todayCnt=QString(packgData.mid(23,1).toHex()+packgData.mid(22,1).toHex()+packgData.mid(21,1).toHex()+packgData.mid(20,1).toHex()).toULong(0,16);
        uint32_t water_conservation_allCnt=QString(packgData.mid(27,1).toHex()+packgData.mid(26,1).toHex()+packgData.mid(25,1).toHex()+packgData.mid(24,1).toHex()).toULong(0,16);
        uint32_t water_conservation_todayCnt=QString(packgData.mid(31,1).toHex()+packgData.mid(30,1).toHex()+packgData.mid(29,1).toHex()+packgData.mid(28,1).toHex()).toULong(0,16);
        uint32_t rce_allCnt=QString(packgData.mid(35,1).toHex()+packgData.mid(34,1).toHex()+packgData.mid(33,1).toHex()+packgData.mid(32,1).toHex()).toULong(0,16);
        uint32_t rec_todayCnt=QString(packgData.mid(39,1).toHex()+packgData.mid(38,1).toHex()+packgData.mid(37,1).toHex()+packgData.mid(36,1).toHex()).toULong(0,16);
        uint16_t tempCnt=QString(packgData.mid(41,1).toHex()+packgData.mid(40,1).toHex()).toInt(0,16);
        uint16_t humCnt=QString(packgData.mid(43,1).toHex()+packgData.mid(42,1).toHex()).toInt(0,16);
        uint16_t hsulfideCnt=QString(packgData.mid(45,1).toHex()+packgData.mid(44,1).toHex()).toInt(0,16);
        uint16_t ammoniaCnt=QString(packgData.mid(47,1).toHex()+packgData.mid(46,1).toHex()).toInt(0,16);
        uint16_t smellCnt=QString(packgData.mid(49,1).toHex()+packgData.mid(48,1).toHex()).toInt(0,16);
        uint16_t particlesCnt=QString(packgData.mid(51,1).toHex()+packgData.mid(50,1).toHex()).toInt(0,16);

        QString time=QString::number((QDateTime::fromString("1970-01-01 00:00:00","yyyy-MM-dd HH:mm:ss").secsTo(QDateTime::currentDateTime().addDays(-1).addSecs(16*60*60))));


        //空调
        uint8_t kontTiaoData=packgData[52];
        QString doorStatus=QString::number((kontTiaoData>>0)&0b1);
        QString lightStatus=QString::number((kontTiaoData>>1)&0b1);
        QString crushKongTiao=QString::number((kontTiaoData>>2)&0b1);
        QString kongTiaoStatus=QString::number((kontTiaoData>>3)&0b1);
        QString tempNeiji=QString::number(packgData[53]);
        QString kongTiaoMode=QString::number(packgData[54]);
        QString kongTiaoSpeed=QString::number(packgData[55]);



        if(kongTiaoMode=="1")kongTiaoMode=u8"制冷";
        else if(kongTiaoMode=="2")kongTiaoMode=u8"制热";
        else if(kongTiaoMode=="3")kongTiaoMode=u8"除湿";
        else if(kongTiaoMode=="4")kongTiaoMode=u8"送风";
        else if(kongTiaoMode=="5")kongTiaoMode=u8"自动";

        if(kongTiaoSpeed=="1")kongTiaoSpeed=u8"低风档";
        else if(kongTiaoSpeed=="2")kongTiaoSpeed=u8"中风档";
        else if(kongTiaoSpeed=="3")kongTiaoSpeed=u8"高风档";
        else if(kongTiaoSpeed=="4")kongTiaoSpeed=u8"自动";

        //向空调表写数据
        QSqlQuery _query(m_db);
        EquCodeAndId eca=queryEquIdAndTable(_socket->property("site_code").toString(),"5");
        _query.exec(QString("select id from zh_wcinspection_%1 where equipment_id=%2;").arg(eca.table,eca.id));
        _query.first();
        if(_query.isValid()) _query.exec(QString("update zh_wcinspection_%1 set updatetime=%2,"
                                "control_1=%3,control_2=%4,control_3='%5',control_4='%6',control_5=%7 where equipment_id=%8;")
                            .arg(eca.table,time,kongTiaoStatus,tempNeiji,kongTiaoMode,kongTiaoSpeed,crushKongTiao,eca.id));
        else _query.exec(QString("insert into zh_wcinspection_%1(equipment_id,createtime,updatetime,control_1,control_2,control_3,control_4,control_5) "
                                "value(%2,%3,%4,%5,%6,'%7','%8',%9);")
                            .arg(eca.table,eca.id,time,time,kongTiaoStatus,tempNeiji,kongTiaoMode,kongTiaoSpeed,crushKongTiao));

        //向大门表写数据
        eca=queryEquIdAndTable(_socket->property("site_code").toString(),"2");
        _query.exec(QString("select id from zh_wcinspection_%1 where equipment_id=%2;").arg(eca.table,eca.id));
        _query.first();
        if(_query.isValid()) _query.exec(QString("update zh_wcinspection_%1 set updatetime=%2,"
                                "control_1=%3 where equipment_id=%4;")
                            .arg(eca.table,time,doorStatus,eca.id));
        else _query.exec(QString("insert into zh_wcinspection_%1(equipment_id,createtime,updatetime,control_1) "
                                "value(%2,%3,%4,%5);")
                            .arg(eca.table,eca.id,time,time,doorStatus));

        //向照明表写数据
        eca=queryEquIdAndTable(_socket->property("site_code").toString(),"1");
        _query.exec(QString("select id from zh_wcinspection_%1 where equipment_id=%2;").arg(eca.table,eca.id));
        _query.first();
        if(_query.isValid()) _query.exec(QString("update zh_wcinspection_%1 set updatetime=%2,"
                                "control_1=%3 where equipment_id=%4;")
                            .arg(eca.table,time,lightStatus,eca.id));
        else _query.exec(QString("insert into zh_wcinspection_%1(equipment_id,createtime,updatetime,control_1) "
                                "value(%2,%3,%4,%5);")
                            .arg(eca.table,eca.id,time,time,lightStatus));


        //向心跳包写入数据
        QString site_code=_socket->property("site_code").toString();
        QString idlepit=QString::number(idlepitCount)+",";//空闲厕所数量
        QString pit=QString::number(pitCount)+",";//厕所总数量
        QString numuser_today=QString::number(numuser_todayCnt)+",";
        QString numuser_all=QString::number(numuser_allCnt)+",";
        QString water_conservation_today=QString::number(water_conservation_todayCnt/100.0,'f',2)+",";
        QString water_conservation_all=QString::number(water_conservation_allCnt/100.0,'f',2)+",";
        QString rec_today=QString::number(rec_todayCnt/100.0,'f',2)+",";
        QString rce_all=QString::number(rce_allCnt/100.0,'f',2)+",";
        QString temp=QString::number(tempCnt*0.1)+",";
        QString hum=QString::number(humCnt*0.1)+",";
        QString ammonia=QString::number(ammoniaCnt*0.001)+",";
        QString smell=QString::number(smellCnt*0.001)+",";
        QString hsulfide=QString::number(hsulfideCnt*0.001)+",";
        QString air_quality="";//保留
        QString particles=QString::number(particlesCnt)+",";
        QString status="1,";//保留

        //插入日志表
        //心跳包计数
        int heartCount=_socket->property("heartCount").toInt();
        heartCount++;
        if(heartCount>=g_heartMaxCount){
            _query.exec("insert into zh_wcinspection_message_log(site_code,idlepit,pit,numuser_today,numuser_all,water_conservation_today,water_conservation_all,rce_today,rce_all,temp,hum,ammonia,smell,hsulfide,air_quality,particles,createtime) "
                        " values('"+site_code+"',"+idlepit+pit+numuser_today+numuser_all+water_conservation_today+water_conservation_all+rec_today+rce_all+temp+hum+ammonia+smell+hsulfide+"'"+air_quality+"',"+particles+time+");");
            heartCount=0;
        }else if(warnFlag){
            _query.exec("insert into zh_wcinspection_message_log(site_code,idlepit,pit,numuser_today,numuser_all,water_conservation_today,water_conservation_all,rce_today,rce_all,temp,hum,ammonia,smell,hsulfide,air_quality,particles,createtime) "
                        " values('"+site_code+"',"+idlepit+pit+numuser_today+numuser_all+water_conservation_today+water_conservation_all+rec_today+rce_all+temp+hum+ammonia+smell+hsulfide+"'"+air_quality+"',"+particles+time+");");

        }
        _socket->setProperty("heartCount",heartCount);

        _query.exec("update zh_wcinspection_message set idlepit="+idlepit+"pit="+pit+"numuser_today="+numuser_today+"numuser_all="+numuser_all+"water_conservation_today="+water_conservation_today+"water_conservation_all="+water_conservation_all+"rce_today="+rec_today+"rce_all="+rce_all+
                "temp="+temp+"hum="+hum+"ammonia="+ammonia+"smell="+smell+"hsulfide="+hsulfide+"air_quality='"+air_quality+"',particles="+particles+"status="+status+"updatetime="+time+" where site_code='"+site_code+"';");


        //发送响应内容
        QByteArray sendData;
        sendData.append(head);
        sendData.append(0x0B);
        sendData.append((1-1));
        sendData.append(0x81);
        sendData.append(0x06);
        sendData.append(data.at(6));
        uint16_t crc=calcCrc16(sendData,sendData.size());
        sendData.append(crc&0xFF);
        sendData.append(crc>>8);
        sendData.append(tail);

        //发送数据
        _socket->write(sendData);

    }
    else if(funCode==0x0681){//工控机反馈
        if(data.size()!=11){//如果包长不对则退出函数
            _socket->write("funCode:0x0681,The data length cannot be 11.");
            return;
        }

        //插入site_code
        QByteArray _siteCode=_socket->property("site_code").toByteArray();
        for(;_siteCode.size()<10;){
            _siteCode.append(1-1);
        }
        data.insert(7,_siteCode);

        QByteArray arr;
        arr.append(0x15);
        data.replace(2,1,arr);
        uint16_t crc=calcCrc16(data.left(data.size()-4),data.size()-4);
        arr.clear();
        arr.append(crc&0xff);
        arr.append(crc>>8);
        data.replace(17,2,arr);

        m_websocket.sendTextMessage(data.toHex());
        // qDebug()<<"sned:"<<data;
    }
}

void MainClass::onTimer1Out()
{
    QString str="["+QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")+"]";
    qDebug()<<str+u8"当前连接数量: "+QString::number(m_tcpSockets.count());

}

void MainClass::onTimer2Out()
{

    QString str="["+QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")+"]";
    QSqlQuery query(m_db);
    bool flag = false;
    QString qStr="delete from zh_wcinspection_message_log where createtime<"+QString::number((QDateTime::fromString("1970-01-01 00:00:00","yyyy-MM-dd HH:mm:ss").secsTo(QDateTime::currentDateTime().addDays(-m_dbClearDays))))+";";
    QString qStr2="delete from zh_wcinspection_jiangjiecao_log where createtime<"+QString::number((QDateTime::fromString("1970-01-01 00:00:00","yyyy-MM-dd HH:mm:ss").secsTo(QDateTime::currentDateTime().addDays(-m_dbClearDays))))+";";

    //工控机心跳包
    flag = query.exec(qStr);
    if(flag){
        qDebug()<<str+u8"清除工控机心跳包日志"+QString::number(m_dbClearDays)+u8"天前记录.受影响行数:"+QString::number(query.numRowsAffected());
    }else{
        qDebug()<<str+u8"清除工控机心跳包日志"+QString::number(m_dbClearDays)+u8"天前记录.清除结果:false";
    }

    //降解槽日志
    flag = query.exec(qStr2);
    if(flag){
        qDebug()<<str+u8"清除降解槽日志"+QString::number(m_dbClearDays)+u8"天前记录.受影响行数:"+QString::number(query.numRowsAffected());
    }else{
        qDebug()<<str+u8"清除降解槽日志"+QString::number(m_dbClearDays)+u8"天前记录.清除结果:false";
    }
    // qDebug()<<u8"sql语句:"+qStr;
    // qDebug()<<u8"错误信息:"+query.lastError().text()+"\n\n\n\n";
}

void MainClass::onTimer3Out()
{
    //如果websocket断开则尝试重新连接
    if(m_websocket.state()!=QAbstractSocket::ConnectedState){
        m_websocket.open(QUrl(m_webSocketUrl));
    }

    //如果数据库连接断开则尝试重新连接
    if(!m_db.isOpen()){
        m_db.open();
        QString str="["+QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")+"]";
        qDebug()<<str+u8"WebSocket未连接,正在尝试连接.";
    }
}

void MainClass::onTImer4Out()
{
    QSqlQuery query(m_db),childQuery(m_db);
    query.exec(QString("select * from zh_wcinspection_equipment where category_id=4;"));
    for(query.first();query.isValid();query.next()){
        childQuery.exec(QString("select control_3,control_4 from zh_wcinspection_%1 where equipment_id=%2;").arg(query.value("table").toString(),query.value("id").toString()));
        childQuery.first();
        postGetSidAndDataServerAddress(query.value("id").toString(),childQuery.value("control_3").toString(),childQuery.value("control_4").toString(),query.value("table").toString());//更新session
    }
}

void MainClass::onWebSocketConnected()
{
    m_timer3.stop();
}

void MainClass::onWebSocketDisConnected()
{
    m_timer3.setInterval(5000);
    m_timer3.start();
}

void MainClass::onWebSocketReadyRead(QString str)
{

    QByteArray data;
    for(int i=0;i<str.size();i+=2){
        data.append(str.mid(i,2).toInt(0,16));
    }

    if(data.size()<21){//如果包长不够则退出函数
        // m_websocket.sendTextMessage("Insufficient data length,at least 11 bytes.");
        return;
    }

    QByteArray head,tail;
    head.append(0x7E);
    head.append(0x7E);
    tail.append(0x0A);
    tail.append(0x0D);

    if(!data.startsWith(head)||!data.endsWith(tail)){
        // m_websocket.sendTextMessage("The packet header or packet tail check fails.");
        return;//检验包头包尾
    }
    uint16_t orgSize=QString(data.mid(3,1).toHex()+data.mid(2,1).toHex()).toInt(0,16);

    if(data.size()!=orgSize){//如果包长不够则退出函数
        // _socket->sendTextMessage("The data frame length is not equal to the calculated length.");
        return;
    }
    uint16_t crc=calcCrc16(data.left(data.size()-4),data.size()-4);//计算校验位


    if((crc&0xFF)!=data.mid(data.size()-4,1).toHex().toInt(0,16)){//校验低位数据
        // _socket->sendTextMessage("Data verification fails.");
        return;
    }
    if((crc>>8)!=data.mid(data.size()-3,1).toHex().toInt(0,16)){//校验高位数据
        // _socket->sendTextMessage("Data verification fails.");
        return;
    }

    QByteArray packgData=data.mid(17,data.size()-21);//获取数据包

    QString _siteCode=data.mid(7,10);//获取设备编号

    for(auto _targetSocketSiteCode=m_tcpSockets.begin();_targetSocketSiteCode!=m_tcpSockets.end();_targetSocketSiteCode++){
        if(_targetSocketSiteCode.value()->property("site_code").toString()==_siteCode){
            _targetSocketSiteCode.value()->write(packgData);
            break;
        }
    }
}

void MainClass::onPostGetSidAndDataServerAddressFinished()
{
    QNetworkReply* reply=static_cast<QNetworkReply*>(sender());

    if(reply==nullptr)return;

    if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200 || reply->error()!=QNetworkReply::NoError)return;
    QString equipment_id=reply->property("equipment_id").toString();
    QString tableName=reply->property("table").toString();
    QString dataStr=QString::fromUtf8(reply->readAll());
    reply->close();
    reply->deleteLater();
    QStringList dataList=dataStr.split("\r\n",Qt::SkipEmptyParts);
    if(dataList.count()<3)return;
    if(dataList.at(0)!="OK"){
        qDebug()<<"["+QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")+"]"<<u8"登录失败,设备id:"+equipment_id;
        return;
    }

    QString address=dataList.at(1);
    address.remove(0,5);;
    QString sid=dataList.at(2);
    sid.remove(0,4);


    //读数据
    QNetworkRequest request;
    request.setRawHeader("Content-Type","text/plain");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, true);
    request.setUrl(QUrl(QString("http://%1/exdata?SID=%2&OP=R").arg(address,sid)));
    QString body;
    body+="13";
    for(int i=0;i<m_orderList.count();++i){
        body+="\r\n"+m_orderList.at(i);
    }
    QNetworkReply* sendReply=m_networkManager->post(request,body.toUtf8());
    sendReply->setParent(this);
    sendReply->setProperty("equipment_id",equipment_id);
    sendReply->setProperty("table",tableName);
    connect(sendReply,SIGNAL(finished()),this,SLOT(onGetValueFinished()));

}

void MainClass::onGetValueFinished()
{
    QNetworkReply* reply=static_cast<QNetworkReply*>(sender());

    if(reply==nullptr)return;

    if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200 || reply->error()!=QNetworkReply::NoError)return;

    QString dataStr=QString::fromUtf8(reply->readAll());
    QString equipment_id=reply->property("equipment_id").toString();
    QString tableName=reply->property("table").toString();
    reply->close();
    reply->deleteLater();
    QStringList dataList=dataStr.split("\r\n",Qt::SkipEmptyParts);
    // //打印调试信息
    // for(int i=0;i<dataList.count();++i){
    //     qDebug()<<dataList.at(i);
    // }

    if(dataList.count()<3)return;
    if(dataList.at(0)!="OK"){
        qDebug()<<"["+QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")+"]"<<u8"获取云服务数据失败,设备id:"+equipment_id;
        return;
    }
    if(dataList.count()<m_orderList.count()+2)return;

    //入库
    QSqlQuery query(m_db);
    QString time=QString::number((QDateTime::fromString("1970-01-01 00:00:00","yyyy-MM-dd HH:mm:ss").secsTo(QDateTime::currentDateTime().addDays(-1).addSecs(16*60*60))));
    QString result;
    for(int i=0;i<m_orderList.count();++i){
        if(dataList.at(i+2).toInt()==1){
            if(result.isEmpty()){
                result+=m_orderList.at(i);
            }else{
                result+=";"+m_orderList.at(i);
            }
        }
    }
    if(result.isEmpty())result=u8"正常";

    query.exec(QString("update zh_wcinspection_%1 set updatetime=%2,control_2='%3' where equipment_id=%4;").arg(tableName,time,result,equipment_id));
    query.exec(QString("insert into zh_wcinspection_%1_log(equipment_id,createtime,control_2,updatetime) values(%2,%3,'%4',%5);").arg(tableName,equipment_id,time ,result,time));
}
