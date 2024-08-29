#ifndef MAINCLASS_H
#define MAINCLASS_H

#include <QObject>
#include <QDebug>
#include <QSettings>
#include <QFile>
#include "global.h"
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonParseError>
#include <QDateTime>
#include <QTimer>
#include <QWebSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

class MainClass : public QObject
{
    Q_OBJECT

    //私有槽函数部分
private slots:
    void onNewTcpSocketComming();//当有新连接到来时调用此槽函数
    void onSocketDisconnected();//当有套接字连接断开时调用此槽函数
    void onSocketReadyRead();//当套接字可读时调用此槽函数
    void onTimer1Out();//当m_timer1超时时调用此函数
    void onTimer2Out();//当m_timer2超时时调用此函数
    void onTimer3Out();//当m_timer3超时时调用此函数
    void onTImer4Out();//当m_timer4超时时调用此函数
    void onWebSocketConnected();//当websocket连接成功时调用此函数
    void onWebSocketDisConnected();//当websocket连接断开时调用此函数
    void onWebSocketReadyRead(QString);//当websock有信息可读时调用此函数
    void onPostGetSidAndDataServerAddressFinished();//当获取数据服务器地址和sid完成时调用此函数
    void onGetValueFinished();//当获取grm数据完成时调用此函数

public:
    explicit MainClass(QObject *parent = nullptr);

    //私有成员部分
    //服务器监听私有成员
    QTcpServer* m_tcpServer;//服务器监听对象
    QString m_serverAddress;//服务器监听地址
    QString m_serverPort;//服务器监听端口

    //数据库部分私有成员
    QSqlDatabase m_db;//数据库连接
    QSqlTableModel* m_tableModel;//sql数据模型
    QString m_databaseAddress;//主机地址
    QString m_databasePort;//主机端口
    QString m_databaseName;//数据库名
    QString m_userName;//用户名
    QString m_userPswd;//用户密码
    QTimer m_timer1;//用于定期检验程序状态的驱动器
    QTimer m_timer2;//用于周期性删除插入表数据的定时器
    QTimer m_timer3;//用于当websocket连接断开时周期性尝试连接服务器的定时器
    QTimer m_timer4;//用于周期性读取grm云服务数据
    int m_updateLogHours;//更新日志所用的时间周期
    int m_dbClearDays;//删除多少天前的插入信息
    QString m_webSocketUrl;//websocket地址
    QWebSocket m_websocket;//websock连接管理对象
    QNetworkAccessManager* m_networkManager;//http请求管理
    QMap<QString,QTcpSocket*> m_tcpSockets;//通过套接字地址套接字指针建立联系
    QStringList m_orderList;//grm参数名列表

    //私有函数部分
    void init();//程序初始化函数,请勿在在此函数调用前,调用任何其他自定义方法
    bool loadConfig(QString filePath);//加载配置文件
    void createConfig(QString filePath);//创建配置文件
    uint16_t calcCrc16(const QByteArray& data, uint16_t count);//计算CRC16校验码
    unsigned char calcCrc8(const QByteArray& data,int len);//计算crc8校验码
    EquCodeAndId queryEquIdAndTable(QString site_code,QString category_id);//通过厕所号设备site_code查询设备id和table,空调使用


    void postGetSidAndDataServerAddress(QString equipment_id,QString account,QString pswds,QString tableName);//获取数据服务器地址和sid

public:
    //公有方法部分
    int releaseAll();//释放所有套接字

signals:
};

#endif // MAINCLASS_H
