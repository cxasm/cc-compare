#pragma once
#include <qobject.h>
#include<QVector>
#include<QMap>
#include <QFuture>

#include <functional>
#include "rcglobal.h"
#include "Encode.h"
#include "AbstractCompare.h"

class BlockUserData;
class QFile;


const int LEFT = 0;
const int RIGHT = 1;

//对比bin二进制文件。
const int MAX_BIN_SIZE = 1024 * 1024 * 10; //最大10M

typedef void(* CALL_FUNC)(void *, uchar *, int);

typedef struct UnequalCharsPosInfo_ {
	int start;
	int length;
}UnequalCharsPosInfo;

//用于比较已知左右内容和LCS的情况下的情况。
//把相等和不等的字符都标记出来
typedef struct textCmpInfo_ {
	bool equal;
	QString text;
}TextCmpInfo;

typedef struct blockCmpResult_ {

	//key 行号，该行的对比信息，把一行中相等、不等的写入value
	//注意要使用引用的方法获取原始，否则效率低下
	QMap<int, QVector<TextCmpInfo>* > lineCmpInfo;

	~blockCmpResult_()
	{
		for (QMap<int, QVector<TextCmpInfo>* >::iterator it = lineCmpInfo.begin(); it != lineCmpInfo.end(); ++it)
		{
			delete *it;
		}
	}
}BlockCmpResult;

typedef struct BinLcsResult_
{
	bool isCmpCancel;

	int cmpMode; // 0 lcs 1 byteOne-by-One
	//不分块的结果
	int lcsLength;
	uchar* lcsContents;

	//如果是分块，则下面三个是分块的lcs结果
	bool isBlockCmp;
	int blockSize;
	QVector<uchar*> lcsDatas;
	QVector<int> lcsSize;

	uchar* leftStart;
	uchar* rightStart;

	qint64 leftSize;
	qint64 rightSize;

	QVector<BinSectionNode> leftBinOut;
	QVector<BinSectionNode> rightBinOut;

	QVector<BinUnequalPos> leftUnequalPos;
	QVector<BinUnequalPos> rightUnequalPos;

	//对于右边的文字显示，不能设置为红色，这部分需要从不等块中剔除，当做纯信息来显示
	QVector<BinUnequalPos> leftSkipUnequalPos;
	QVector<BinUnequalPos> rightSkipUnequalPos;

	QString leftPrintContents;
	QString rightPrintContents;

	qint64 leftStartPos;
	int leftCmpLen;
	qint64 rightStartPos;
	int rightCmpLen;

	BinLcsResult_()
	{
		lcsContents = nullptr;
		leftStart = nullptr;
		rightStart = nullptr;
		lcsLength = 0;
		leftSize = 0;
		rightSize = 0;
		isCmpCancel = false;
		isBlockCmp = false;
		blockSize = 8192;
		cmpMode = 0;

		leftStartPos = -1;
		leftCmpLen = 0;
		rightStartPos = -1;
		rightCmpLen = 0;
	}
	~BinLcsResult_()
	{
		release();
	}

	void release()
	{
		if (lcsContents != nullptr)
		{
			delete[]lcsContents;
			lcsContents = nullptr;

			leftStart = nullptr;
			rightStart = nullptr;
		}
		leftStartPos = -1;
		leftCmpLen = 0;
		rightStartPos = -1;
		rightCmpLen = 0;

		isCmpCancel = false;
		lcsLength = 0;
		isBlockCmp = false;
		blockSize = 8192;
		leftBinOut.clear();
		rightBinOut.clear();
		leftUnequalPos.clear();
		rightUnequalPos.clear();
		leftSkipUnequalPos.clear();
		rightSkipUnequalPos.clear();
		leftPrintContents.clear();
		rightPrintContents.clear();

		for (int i = 0; i < lcsDatas.size(); ++i)
		{
			delete[] lcsDatas[i];
		}

		lcsDatas.clear();
		lcsSize.clear();
	}

	BinLcsResult_& operator=(const BinLcsResult_ & other)
	{
		if (this == &other)
		{
			return *this;
		}

		isCmpCancel = other.isCmpCancel;
		leftSize = other.leftSize;
		rightSize = other.rightSize;
		lcsLength = other.lcsLength;
		isBlockCmp = other.isBlockCmp;
		lcsSize = other.lcsSize;
		cmpMode = other.cmpMode;

		leftStartPos = other.leftStartPos;
		leftCmpLen = other.leftCmpLen;
		rightStartPos = other.rightStartPos;
		rightCmpLen = other.rightCmpLen;

		return *this;
	}

}BinLcsResult;

const int EMPTY_FILE = 0;
const int SCAN_SUCCESS = 1;

class CmpareMode;

typedef struct ThreadFileCmpParameter_ {
	QString leftPath;
	QString rightPath;

	uchar* leftText;
	uchar* rightText;

	int leftLens;
	int rightLens;

	CmpareMode *resultCmpObj;

	ThreadFileCmpParameter_(uchar* lp, int ls, uchar* rp, int rs)
	{
		leftText = lp;
		rightText = rp;
		leftLens = ls;
		rightLens = rs;
		resultCmpObj = nullptr;
	}
	ThreadFileCmpParameter_(QString leftPath_, QString rightPath_)
	{
		leftPath = leftPath_;
		rightPath = rightPath_;
		resultCmpObj = nullptr;
	}

}ThreadFileCmpParameter;

class CmpareMode :public QObject
{
	Q_OBJECT
public:
	CmpareMode(QString left, QString right);
	CmpareMode(uchar * leftContent, const int leftLens, uchar * rightContent, const int rightLens);
	virtual ~CmpareMode();

	static void lineCmpLcs(const int lineNum, QVector<LineFileInfo>& linesInfo, QChar * lcsSrc, int lcsLens, QVector<TextCmpInfo>& result);

	static CODE_ID readLineFromFile(uchar * m_fileFpr, const int fileLength, const CODE_ID fileCode, QList<LineFileInfo>& lineInfoVec, QList<LineFileInfo>& blankLineInfoVec, int mode, int& maxLineSize);
	static CODE_ID judgeFinalTextCode(CODE_ID code, bool isExistUnKownCode, bool isExistGbk, bool isExistUtf8);
	static CODE_ID readLineFromFile(uchar * m_fileFpr, const int fileLength, const CODE_ID fileCode, QList<LineFileInfo>& lineInfoVec,int& maxLineSize, int& charsNums, bool &isMaybeHexFile);
	void getCodeSkip(CODE_ID & leftCode, CODE_ID & rightCode, int & leftSkip, int & rightSkip);

	static CODE_ID scanFileRealCode(QString filePath,int scanLineNum=200);
	static CODE_ID scanFileOutPut(CODE_ID code, QString filePath, QList<LineFileInfo>& outputLineInfoVec, int & maxLineSize, int & charsNums, bool &isHexFile);

	void setCmpMode(int mode);

	void setCmpTextCode(CODE_ID leftCode, CODE_ID rightCode);

	void releaseFile();

	static QFuture<ThreadFileCmpParameter*> commitAsyncTask(std::function<ThreadFileCmpParameter*(ThreadFileCmpParameter*)> fun, ThreadFileCmpParameter * parameter);

	bool isFileTextEqual(AbstractCompare *blockCompare);

	bool work(AbstractCompare *blockCompare);
	void workBin(BinLcsResult * blockCompare);

	void cmp(BinLcsResult *blockCompare);
	void cmpLcsLine(QByteArray * lcsLine, int lcsLens);
	inline QList<LineFileInfo>* getLeftLineInfo();
	inline QList<LineFileInfo>* getRightLineInfo();

	/*inline QVector<LineFileInfo>* getLeftBlankLineInfo();
	inline QVector<LineFileInfo>* getRightBlankLineInfo();*/

	//void outputBlocks(std::function<void(const BlocksInfo&, uchar*, QVector<LineFileInfo>&, const BlocksInfo&, uchar*, QVector<LineFileInfo>&)>);

	void getLines(RC_DIRECTION direction, QList<LineFileInfo>& lines);

	void mergeBlockFromLine();

	void getBlockInfo(int direcion, QList<BlocksInfo>* &equalList, QList<BlocksInfo>* &unEqualList, uchar * &filePtr);

	void getResult(QStringList *& leftContent, QStringList *& rightContent);

	void getUnqealPosList(QVector<UnequalCharsPosInfo>* &left, QVector<UnequalCharsPosInfo>* &right);

	void outputBlocks(AbstractCompare * blockCompare);

	void getLinesExternInfo(QList<BlockUserData*>*& leftExternBlockInfo, QList<BlockUserData*>*& rightExternBlockInfo);

	static CODE_ID getTextFileEncodeType(uchar* fileFpr, int fileLength, QString filePath="", bool isCheckHead = true);
	static bool isUnicodeLeBomFile(uchar* fileFpr, int fileLength);
	static CODE_ID getTextFileEncodeType(QString filePath, int scanLineNum, QByteArray* bytes= nullptr);

	bool cmpSkipEmptyLine();
signals:
	void outputMsg(int code, QString msg);
	void moveStep();
private:
#if 0
	//忽略完全空白的行，即只有换行符号，没有其他文字
	void createLineFileInfoIgnoreEmptyLine(QVector<LineFileInfo>&lineInfoVec, QVector<LineFileInfo>&blankLineInfoVec, uchar * fileData, quint32 fileLens, int ignoreLineEndNums=0);
#endif
	//static CODE_ID readLineFromFile(QFile &fileHandle, QList<LineFileInfo>& lineInfoVec, QList<LineFileInfo>& blankLineInfoVec);
	//static CODE_ID readLineFromFile(uchar* m_fileFpr, const int fileLength, QList<LineFileInfo>&lineInfoVec, QList<LineFileInfo>&blankLineInfoVec);

	int preScanFile(AbstractCompare *blockCompare);

	void cmpDateAndLcsLine(QList<LineFileInfo>&lineInfoVec, int offset, QByteArray * lcsLine, int lcsLens, RC_DIRECTION direction, QMap<int, EqualLineInfo> & enqualLineInfoVec);
	void cmpByLine(AbstractCompare *blockCompare);
	bool isEqualByLine(AbstractCompare* blockCompare);
	void outputBlock(int direction, QVector<BlockNode>& blocks, bool isEqualBlock);
	void cmpAllBlockInfo();

	static bool recognizeTextCode(QByteArray & text, LineFileInfo & lineInfo, QString & outUnicodeText);

	CODE_ID getTextFileEncodeType(RC_DIRECTION dir);

	quint32 static readLineFromFileWithUnicodeLe(uchar* m_fileFpr, const int fileLength, QList<LineFileInfo>& lineInfoVec, QList<LineFileInfo>& blankLineInfoVec,int mode, int &maxLineSize);
	void outputBinBlock(BinLcsResult * blockCompare);
	int outputBinBlock(int leftSize, int rightSize, int leftOffet, int rightOffet, uchar * leftStr, uchar * rightStr, QVector<BinUnequalPos>& leftUnequalPos, QVector<BinUnequalPos>& rightUnequalPos, QVector<BinUnequalPos>& leftSkipUnequalPos, QVector<BinUnequalPos>& rightSkipUnequalPos, QString & leftPrintContents, QString & rightPrintContents, int & curLeftOffset, int & curRightOffset);
	void outputBinBlock(int lcsLength, int leftSize, int rightSize, int leftOffet, int rightOffet, uchar * lcsContents, uchar * leftStr, uchar * rightStr, QVector<BinUnequalPos>& leftUnequalPos, QVector<BinUnequalPos>& rightUnequalPos, QVector<BinUnequalPos>& leftSkipUnequalPos, QVector<BinUnequalPos>& rightSkipUnequalPos, QString & leftPrintContents, QString & rightPrintContents, int & curLeftOffset, int & curRightOffset);
	void outputOneByOneByte(BinLcsResult * blockCompare);
	void outputOneByOneByteWithPos(BinLcsResult * blockCompare);
	void outputBinMultiBlock(BinLcsResult * blockCompare);
	void oneByteOneCmp(BinLcsResult * blockCompare);
private:
	QFile *m_leftFile;
	QFile *m_rightFile;

	RC_LINE_FORM m_leftFileFormat;
	RC_LINE_FORM m_rightFileFormat;

	uchar * m_leftFpr;
	uchar * m_rightFpr;

	QByteArray * m_lcsLine;

	//左右文件的非空白行索引信息
	QList<LineFileInfo> m_leftLineInfoVec;
	QList<LineFileInfo> m_rightLineInfoVec;

	//左右文件的空白行索引信息
	QList<LineFileInfo> m_leftBlankLineInfoVec;
	QList<LineFileInfo> m_rightBlankLineInfoVec;

	QList<BlockUserData*>m_leftExternBlockInfo;
	QList<BlockUserData*>m_rightExternBlockInfo;


	//lcs中相当的内容，分配对应左右文件中的索引对
	QMap<int,EqualLineInfo> m_enqualLineInfoVec;

	QList<BlocksInfo> m_leftNoEqualBlockInfoVec;
	QList<BlocksInfo> m_rightNoEqualBlockInfoVec;

	//左右相等块的索引信息。相当块是比相当行更大的单位，多个连续相同的行即组合为一个块
	QList<BlocksInfo> m_leftEqualBlockInfoVec;
	QList<BlocksInfo> m_rightEqualBlockInfoVec;

	int m_leftLinesNums;
	int m_rightLinesNums;

	//最终以什么结尾 :0/r 1/r/n 2 没有
	RC_LINE_FORM m_leftEndWithChar;
	RC_LINE_FORM m_rightEndWithChar;


	//处理编码事宜
	CODE_ID m_leftCode;
	CODE_ID m_rightCode;
	int m_leftSkip;
	int m_rightSkip;

	QStringList* m_leftContent;
	QStringList* m_rightContent;

	QVector<UnequalCharsPosInfo> m_leftUnqealPosList;
	QVector<UnequalCharsPosInfo> m_rightUnqealPosList;

	int m_leftCurPos;
	int m_rightCurPos;

	//0 忽略行前行后空白 1 忽略行尾 2 忽略所有空白
	int m_mode;

	QString m_leftFilePath;
	QString m_rightFilePath;
#if 0
	QVector<BlockInfo> m_leftBlockInfoVec;
	QVector<BlockInfo> m_rightBlockInfoVec;
#endif

	qint64 m_leftContentLength;
	qint64 m_rightContentLength;

};

