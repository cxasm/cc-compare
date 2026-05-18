#pragma once
#include <QtGlobal>
#include <QObject>
#include <QVector>

#include "CmpareMode.h"

class Lcs:public QObject
{
	Q_OBJECT
public:
	Lcs(uchar * strA, uchar * strB, int lenA, int lenB);
	static int getLcsLength(int m, int n, uchar * strA, uchar * strB);
	virtual ~Lcs();
	uchar* cmp(int &lens);

	static int getCmpTotalStep(int leftSize, int rightSize);

	void cmp(int blockSize, QVector<uchar*>& lcsDatas, QVector<int>& lcsSize);

	//设置外部参数，里面需要判断释放取消
	void setParameter(BinLcsResult* para);
signals:
	void reportStep(int code, QString text);

private:
	int * findRow(int m, int n, uchar *strA, uchar *strB, bool Reverse = false);

#ifdef _DEBUG
	//递归版本只做算法参考，不做发布
	uchar *H_LCS(int m, int n, uchar * strA, uchar *strB, int &reLen);
#endif

	uchar *H_LCS1(int m, int n, uchar * strA, uchar *strB, int &reLen);

	bool find(uchar* begin, uchar *end, uchar d);

	int m_lenA;
	int m_lenB;

	uchar *m_strA;
	uchar *m_strB;
	
	BinLcsResult* m_binLcsResult;
};
