#pragma once
#include <QtGlobal>
#include <qobject.h>

class LcsLine:public QObject
{
	Q_OBJECT
public:
	LcsLine(QByteArray * strA, QByteArray * strB, int lenA, int lenB);
	virtual ~LcsLine();
	QByteArray* cmp(int &lens);

	void setCancelFlag(volatile bool* isCancel)
	{
		m_isCancel = isCancel;
	}

	bool isCancel()
	{
		if (m_isCancel != nullptr)
		{
			return *m_isCancel;
		}
		return false;
	}

	int getLcsLength(int m, int n, QByteArray* strA, QByteArray* strB);


signals:
	void outMsg(int code, QString msg);

private:

	int * findRow(int m, int n, QByteArray *strA, QByteArray *strB, bool Reverse = false);

	QByteArray *H_LCS1(int m, int n, QByteArray * strA, QByteArray *strB, int &reLen);

#ifdef _DEBUG
	QByteArray * H_LCS(int m, int n, QByteArray * strA, QByteArray *strB, int &reLen);
#endif

	bool find(QByteArray* begin, QByteArray *end, QByteArray d);

	int m_lenA;
	int m_lenB;

	QByteArray *m_strA;
	QByteArray *m_strB;

	volatile bool* m_isCancel;

	int m_curStep;
	int m_leftStep;
};
