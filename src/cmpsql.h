#pragma once
#include <QString>
#include "progresswin.h"

static QString CMP_ZOOM_VALUE = "cmpzoom"; //放大倍数。文件对比的放大缩小视图值。

class CmpSql
{
public:
	CmpSql();
	virtual ~CmpSql();

	static QString selectDataPath();

	static void init();

	static bool getUpdateSetTime(QString & value);

	static QString getKeyValueFromSets(QString & key);

	static bool updataKeyValueFromSets(const QString & key, QString & value);

	static void addKeyValueToSets(QString  key, QString  value);

	static QString getKeyValueFromLongSets(QString& key);

	static bool updataKeyValueFromLongSets(QString& key, QString& value);

	static void addKeyValueToLongSets(QString key, QString& value);

	static int getKeyValueFromNumSets(const QString& key, int defvalut =0);

	static bool updataKeyValueFromNumSets(const QString& key, int value);

	static void addKeyValueToNumSets(QString key, int value);

	static void begin();

	static void commit();

	static void close();

	static void initSetUpdataIndex();

	static void initSetNoticeWinShow();

	static void updateSetTime();

	static bool isDbExist()
	{
		return s_isExistDb;
	}

private:
	static void initSetUpdataTime();
	static bool s_isExistDb;
	static int s_reference;
};

