#pragma once
#include <QString>
#include <qobject.h>
#include <QVector>
#include "blockuserdata.h"


#if defined(CC_EXPORTDLL)
#define CC_EXPORT __declspec(dllexport)
#else
#define CC_EXPORT __declspec(dllimport)
#endif

typedef struct UnequalCharsPosInfo_ {
	int start;
	int length;
}UnequalCharsPosInfo;

class CC_EXPORT CCmp : public QObject
{
	Q_OBJECT
public:
	CCmp(QObject* parent=nullptr);
	virtual ~CCmp();

	void setCmpMode(int mode);// 0 默认值,忽略行前行号空白字符 1:只忽略行尾空白字符 2:忽略行前中尾的所有空白
	void setCmpParameter(bool isBlankLineDoMatch, int lineMatchEqualRata);//对比参数。isBlankLineDoMatch 空行是否参与比较默认true，lineMatchEqualRata 行认定为相等的相似率默认50

	//对比同步文件
	void compareSyncFile(QString leftPath, QString rightPath);

	//对比文件，对比结果会以cmpResult的信号输出
	QObject* compareFile(QString leftPath, QString rightPath);

signals:
	void cmpFinished(int resultType, QStringList* leftContents, QStringList* rightContents, QVector<UnequalCharsPosInfo>* leftUnequalInfo, QVector<UnequalCharsPosInfo>* rightUnequalInfo, \
		const QList<BlockUserData*>* leftBlockData, const QList<BlockUserData*>* rightBlockData);

private:
	int m_mode;
	bool m_isBlankLineDoMatch;
	int m_lineMatchEqualRata;
};

