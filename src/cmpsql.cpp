#include "cmpsql.h"


#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtDebug>
#include <QCoreApplication>
#include <QSqlRecord>
#include <QDate>
#include <QFile>
#include <QStorageInfo>
#include <RealCompare.h>
#include <QSettings>


static QSqlDatabase s_database;

//数据库文件是否存在。如果初始化失败，则不存在
bool CmpSql::s_isExistDb = false;

int CmpSql::s_reference = 0;

CmpSql::CmpSql()
{
}


CmpSql::~CmpSql()
{
}

//因为WIN10的C盘没有写权限，我们选择一个可以写的目录
QString CmpSql::selectDataPath()
{
#if defined(Q_OS_WIN)
	QString settingDir = QString("ccompare/test");
	QSettings qs(QSettings::IniFormat, QSettings::UserScope, settingDir);
	QString qsSavePath = qs.fileName();
	QFileInfo fi(qsSavePath);
	QString dbPath = fi.dir().absolutePath();

	QString ret;
	bool exist = false;

	QDir dir(dbPath);

	if (!dir.exists())
	{
		if (dir.mkpath(dir.absolutePath()))
		{
			ret = dbPath;
			exist = true;
		}
	}
	else
	{
		ret = dbPath;
		exist = true;
	}

	//理论上一定有，如果没有，则下面多半也会错误的
	if (!exist)
	{
		{
			QString name = QDir::home().dirName();
			if (name.isEmpty())
			{
				return ret;
			}

			QString dbPath = QString("c:/Users/%1/.ccompare").arg(name);
			QDir dir(dbPath);

			if (!dir.exists())
			{
				if (dir.mkpath(dir.absolutePath()))
				{
					ret = dbPath;
}
			}
			else
			{
				ret = dbPath;
			}
		}
	}
#if 0
	QStringList volumesList;
	volumesList << "E:/" << "D:/";

	QString existVolume;
	QString ret;
	bool exist = false;

	for (QString path : volumesList)
	{
		QDir dir(path);
		if (dir.exists())
		{
			existVolume = path;
			exist = true;
			break;
		}
	}

	qDebug() << existVolume;

	if (exist)
	{
		QString dbPath = existVolume;
		dbPath.append("Program Files/CC Compare");

		QDir dir(dbPath);

		if (!dir.exists())
		{
			if (dir.mkpath(dir.absolutePath()))
			{
				ret = dbPath;
			}
		}
		else
		{
			ret = dbPath;
		}
		
	}
#endif


#elif defined(Q_OS_MAC)

    QString ret;
    QString name = QDir::home().dirName();
    if (name.isEmpty())
    {
	return ret;
}

    QString dbPath = QString("/Users/%1/Applications/com.hmja.ccompare").arg(name);

    QDir dir(dbPath);

    if (!dir.exists())
    {
        if (dir.mkpath(dir.absolutePath()))
        {
            ret = dbPath;
        }
    }
    else
    {
        ret = dbPath;
    }
#else
    QString ret;
    QString name = QDir::home().dirName();
    if (name.isEmpty())
    {
        return ret;
    }

    QString dbPath = QString("/home/%1/com.hmja.ccompare").arg(name);

    QDir dir(dbPath);

    if (!dir.exists())
    {
        if (dir.mkpath(dir.absolutePath()))
        {
            ret = dbPath;
        }
    }
    else
    {
        ret = dbPath;
    }
#endif
    return ret;
}


//20220402这里隐含了一个前置条件：数据库句柄是在主线程创建的，最好不要在子线程中调用。
//避免因为跨线程访问和多线程冲突访问引发的闪退问题。所以最好数据库的读写都在主线程中进行
void CmpSql::init()
{
	++s_reference;

	//如果已经初始化过了，则直接返回
	if (s_isExistDb)
	{
		return;
	}

	bool initOk = true;

	QString dbDir = selectDataPath();

	QString dbPath;

	if (dbDir.isEmpty())
	{
		dbPath = "options.db";
	}
	else
	{
		dbPath = QString("%1/options.db").arg(dbDir);
	}

	if (QSqlDatabase::contains(dbPath))
	{
		s_database = QSqlDatabase::database(dbPath);
	}
	else
	{
		s_database = QSqlDatabase::addDatabase("QSQLITE", dbPath);
		s_database.setDatabaseName(dbPath);
	}
	
	if (!s_database.open())
	{
		initOk = false;
		qDebug() << "Error: Failed to connect database." << s_database.lastError();
	}

	QSqlQuery sql_query(s_database);

	QString create_sql;

	//一张配置表,以key-value作为键值对保存参数
	create_sql = "create table if not exists sets (id CHARACTER(20) primary key, value CHARACTER(256))";
	if (!sql_query.exec(create_sql))
	{
		initOk = false;
		qDebug() << "Error: Fail to create table." << sql_query.lastError();
	}
	else
	{
		QString key = "signdate";
		//QString date = QDate::currentDate().toString(QString("yyyy/M/d"));
		//不写今天的时间，否则第一次运行，无条件不会发送心跳。
		//直接写一个过去的时间，让第一次运行，总是要签到
		addKeyValueToSets(key, "2022/2/20");

		addKeyValueToSets("skipdir", ".svn:.vs");

		addKeyValueToSets("url", "0");

		addKeyValueToSets("mac", "0");

		addKeyValueToSets("skipext", ".sln:.vcxproj");

		addKeyValueToSets("skipprefix", "ui_");

	}

	//一张配置表,以key-value作为键值对保存参数
	create_sql = "create table if not exists longsets (id CHARACTER(32) primary key, value varchar(10240))";
	if (!sql_query.exec(create_sql))
	{
		initOk = false;
		qDebug() << "Error: Fail to create table." << sql_query.lastError();
	}
	else
	{
		QString str;

		addKeyValueToLongSets("recentdir", str);
		addKeyValueToLongSets("recentfile", str);
		addKeyValueToLongSets("recentopenfile", str);
	}

	//一张配置表,以key-value作为键值对保存参数
	create_sql = "create table if not exists numsets (id CHARACTER(20) primary key, value INT)";
	if (!sql_query.exec(create_sql))
	{
		initOk = false;
		qDebug() << "Error: Fail to create table." << sql_query.lastError();
	}
	else
	{
		//默认给1，因为当数据库没有时或错误时，可能返回0，0可以代表错误。
		int userTimes = 1;
		addKeyValueToNumSets("utimes", userTimes);

		//是否对比隐藏文件
		addKeyValueToNumSets("cmphidefile", 0);

		//对比所有类型文件1; 2 只对比已知支持的文件类型
		addKeyValueToNumSets("cmpallfile", 1);

		//网络回复回来的消息id
		addKeyValueToNumSets("msgid", 1);

		addKeyValueToNumSets("serverip", 0);

		//hex下lcs和一对一对比模式
		addKeyValueToNumSets("hexmode", 1);

		//是否高亮不同处背景
		addKeyValueToNumSets("hexhigh", 1);

		//当前版本
		addKeyValueToNumSets("version", version_num);

		//目录对比模式，0慢1快。默认慢速
		addKeyValueToNumSets("dirmode", 0);

		//跳过目录。默认不跳过0 不跳过 1 跳过
		addKeyValueToNumSets("skipdir", 0);

		//跳过文件后缀。默认不跳过0 不跳过 1 跳过
		addKeyValueToNumSets("skipext", 0);

		//跳过文件后缀。默认不跳过0 不跳过 1 跳过
		addKeyValueToNumSets("skipprefix", 0);

		//默认字体
		addKeyValueToNumSets("fontsize", 10);

		//处理子目录。1处理，2不处理。不使用0.因为许多时候0是读取不到的默认值
		addKeyValueToNumSets("childdirs", 1);

		addKeyValueToNumSets("cmpmode", 2);

		addKeyValueToNumSets(CMP_ZOOM_VALUE, 100);

	}

	s_isExistDb = initOk;
	
}

//初始化更新时间配置 updatatime - value
void CmpSql::initSetUpdataTime()
{
	QSqlQuery sql_query(s_database);

	QDate now = QDate::currentDate();
	QString date = now.toString(QString("yyyy/M/d"));

	//没有ignore时，如果存在记录，则插入会失败；如果带ignore时，如果存在记录，插入时会忽略。正好起到第一次无条件插入的效果
	QString insert_sql = QString("insert or ignore into sets(id, value) values ('%1', '%2')").arg("updatatime").arg(date);
	if (!sql_query.exec(insert_sql))
	{
		qDebug() << "Error: Fail to insert table." << sql_query.lastError();
	}
}


//初始化更新当前序号配置 updataindex - value
void CmpSql::initSetUpdataIndex()
{
	QSqlQuery sql_query(s_database);

	//没有ignore时，如果存在记录，则插入会失败；如果带ignore时，如果存在记录，插入时会忽略。正好起到第一次无条件插入的效果
	QString insert_sql = QString("insert or ignore into sets(id, value) values ('%1', '%2')").arg("updataindex").arg(0);
	if (!sql_query.exec(insert_sql))
	{
		qDebug() << "Error: Fail to insert table." << sql_query.lastError();
	}
}

//初始化更新当前序号配置 msgwinshow - value 有消息是是否弹出对话框
void CmpSql::initSetNoticeWinShow()
{
	QSqlQuery sql_query(s_database);

	//没有ignore时，如果存在记录，则插入会失败；如果带ignore时，如果存在记录，插入时会忽略。正好起到第一次无条件插入的效果
	QString insert_sql = QString("insert or ignore into sets(id, value) values ('%1', '%2')").arg("msgwinshow").arg(0);
	if (!sql_query.exec(insert_sql))
	{
		qDebug() << "Error: Fail to insert table." << sql_query.lastError();
	}
}

//更新 设置的更新时间到今天的时间
void CmpSql::updateSetTime()
{
	QSqlQuery sql_query(s_database);

	QDate now = QDate::currentDate();
	QString date = now.toString(QString("yyyy/M/d"));

	QString update_sql = QString("update sets set value = '%1' where id = '%2'").arg(date).arg("updatatime");
	if (!sql_query.exec(update_sql))
	{
		qDebug() << sql_query.lastError();
	}
}


//获取上次配置中更新股票的时间
bool CmpSql::getUpdateSetTime(QString &value)
{
	QSqlQuery sql_query(s_database);
	QString cmd = QString("select * from sets where id ='updatatime'");
	if (!sql_query.exec(cmd))
	{
		qDebug() << sql_query.lastError();
		return false;
	}
	else
	{
		if (sql_query.first())
		{
			value = sql_query.value(1).toString();
			return true;
		}
	}
	return false;
}


//写一个总的获取配置的接口，避免以后每个字段都需要写一个读写接口
QString CmpSql::getKeyValueFromSets(QString &key)
{
	QString ret;

	QSqlQuery sql_query(s_database);
	QString cmd = QString("select * from sets where id ='%1'").arg(key);
	if (!sql_query.exec(cmd))
	{
		qDebug() << sql_query.lastError();
	}
	else
	{
		if (sql_query.first())
		{
			ret = sql_query.value(1).toString();
		}
	}
	return ret;
}

bool CmpSql::updataKeyValueFromSets(const QString &key, QString &value)
{
	QSqlQuery sql_query(s_database);

	QString update_sql = QString("update sets set value = '%1' where id = '%2'").arg(value).arg(key);
	if (!sql_query.exec(update_sql))
	{
		qDebug() << sql_query.lastError();
		return false;
	}
	return true;
}


//第一次加一条记录，用于初始化
void CmpSql::addKeyValueToSets(QString key, QString value)
{
	QSqlQuery sql_query(s_database);

	//没有ignore时，如果存在记录，则插入会失败；如果带ignore时，如果存在记录，插入时会忽略。正好起到第一次无条件插入的效果
	QString insert_sql = QString("insert or ignore into sets(id, value) values ('%1', '%2')").arg(key).arg(value);
	if (!sql_query.exec(insert_sql))
	{
		qDebug() << "Error: Fail to insert table." << sql_query.lastError();
	}
}

//写一个总的获取配置的接口，避免以后每个字段都需要写一个读写接口
QString CmpSql::getKeyValueFromLongSets(QString& key)
{
	QString ret;

	QSqlQuery sql_query(s_database);
	QString cmd = QString("select * from longsets where id ='%1'").arg(key);
	if (!sql_query.exec(cmd))
	{
		qDebug() << sql_query.lastError();
	}
	else
	{
		if (sql_query.first())
		{
			ret = sql_query.value(1).toString();
		}
	}
	return ret;
}

bool CmpSql::updataKeyValueFromLongSets(QString& key, QString& value)
{
	if (value.size() > 10240)
	{
		return false;
	}

	QSqlQuery sql_query(s_database);

	QString update_sql = QString("update longsets set value = '%1' where id = '%2'").arg(value).arg(key);
	if (!sql_query.exec(update_sql))
	{
		qDebug() << sql_query.lastError();
		return false;
	}
	return true;
}


//第一次加一条记录，用于初始化
void CmpSql::addKeyValueToLongSets(QString key, QString& value)
{
	if (value.size() > 4096)
	{
		return;
	}

	QSqlQuery sql_query(s_database);

	//没有ignore时，如果存在记录，则插入会失败；如果带ignore时，如果存在记录，插入时会忽略。正好起到第一次无条件插入的效果
	QString insert_sql = QString("insert or ignore into longsets(id, value) values ('%1', '%2')").arg(key).arg(value);
	if (!sql_query.exec(insert_sql))
	{
		qDebug() << "Error: Fail to insert table." << sql_query.lastError();
	}
}

//写一个总的获取配置的接口，避免以后每个字段都需要写一个读写接口.0做默认值，最后不用0做值
int CmpSql::getKeyValueFromNumSets(const QString& key, int defvalut)
{
	int ret = 0;

	QSqlQuery sql_query(s_database);
	QString cmd = QString("select * from numsets where id ='%1'").arg(key);
	if (!sql_query.exec(cmd))
	{
		qDebug() << sql_query.lastError();
		ret = defvalut;
	}
	else
	{
		if (sql_query.first())
		{
			ret = sql_query.value(1).toInt();
		}
	}
	return ret;
}

bool CmpSql::updataKeyValueFromNumSets(const QString& key, int value)
{

	QSqlQuery sql_query(s_database);

	QString update_sql = QString("update numsets set value = %1 where id = '%2'").arg(value).arg(key);
	if (!sql_query.exec(update_sql))
	{
		qDebug() << sql_query.lastError();
		return false;
	}
	return true;
}


//第一次加一条记录，用于初始化
void CmpSql::addKeyValueToNumSets(QString key, int value)
{

	QSqlQuery sql_query(s_database);

	//没有ignore时，如果存在记录，则插入会失败；如果带ignore时，如果存在记录，插入时会忽略。正好起到第一次无条件插入的效果
	QString insert_sql = QString("insert or ignore into numsets(id, value) values ('%1', %2)").arg(key).arg(value);
	if (!sql_query.exec(insert_sql))
	{
		qDebug() << "Error: Fail to insert table." << sql_query.lastError();
	}
}


/* 多条语句插入时，用一个begin commit包围，可以减少事务的开销 */
void CmpSql::begin()
{
	QSqlQuery sql_query(s_database);
	QString insert_sql = "begin";
	if (!sql_query.exec(insert_sql))
	{
		qDebug() << "Error: Fail to create table." << sql_query.lastError();
	}
}

void CmpSql::commit()
{
	QSqlQuery sql_query(s_database);
	QString insert_sql = "commit";
	if (!sql_query.exec(insert_sql))
	{
		qDebug() << "Error: Fail to create table." << sql_query.lastError();
	}
}

void CmpSql::close()
{
	if (s_reference > 0)
	{
		--s_reference;

		if (s_reference == 0)
		{
		s_database.close();
		}
	}
}
