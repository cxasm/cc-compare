#pragma once
#include <QMap>
#include <QVector>
#include "rcglobal.h"


typedef struct blockCmpPairInfo_ {
	int flag; //1完全相同，可能需要再分块 //2 一般匹配，部分相等，无需再分配 //3 没有匹配，理解为新增
	int lessBlockLineNums; //不是左右，而是多少。总是用少的去匹配多的。对比方的行号
	int moreBlockLineNums;//可以理解为被对比方的行号

	blockCmpPairInfo_(int flag_, int lessBlockLineNums_ = 0, int moreBlockLineNums_ = 0)
	{
		flag = flag_;
		lessBlockLineNums = lessBlockLineNums_;
		moreBlockLineNums = moreBlockLineNums_;
	}

}BlockCmpPairInfo;

typedef struct blockCmpPairResult_ {
	int direction; // 0 右边是少的 //1左边是少的 //2相对且只有1行，此时直接是右对比左，即直接认定少的是右边
	QMap<int, BlockCmpPairInfo> data; //int key 是少的一边匹配行号码
}BlockCmpPairResult;





enum BlockType {
	REAL_DATA = 1,
	PAD_DATA,
};

//块结构。每一块有多行组成。目前只有不相等的内容才使用该结构。因为相等的直接输出了，不需要使用该块
typedef struct BlockNode_ {
	int type; //类型。1：实际数据块 2：对齐块
	int alignRowCount; //如果是对齐块，则给出对齐块的长度。只要对齐块有效
	//int blockStartNum; //块号码。注意块号和行号可能不是一一对应的，因为有中间对齐块，所以块号要比行号多
	//int blockLens; //块长度。块包含的行数，而不是字符数
	QVector<LineNode> lineNodes; //如果是实际数据块，给出各行的实际数据
	
}BlockNode;

typedef struct lineFileInfo_ {
	qint32 lineNums; //行号码
	bool isLcsExist;//是否属于lcsline的一部分
	bool isEmptyLine; //是否是空白行，只包含换行符的行
	int code; //该行的字符编码
	int lineEndFormat; //行尾：见RC_LINE_FORM
	QByteArray md4;
	QString unicodeStr; //这个是包含行尾的换行符的
	lineFileInfo_()
	{
		isLcsExist = false;
		isEmptyLine = false;
		code = UNKOWN;
		lineEndFormat = UNKNOWN_LINE;
	}
}LineFileInfo;

class AbstractCompare
{
public:
	AbstractCompare();
	virtual ~AbstractCompare();

	void setMatchParameter(bool blankMatch, int matchEqualRata)
	{
		m_isBlankLineDoMatch = blankMatch;
		m_lineMatchEqualRata = matchEqualRata;
	}

	//0个，1个，2个
	int getEmptyFileStatus()
	{
		return m_isEmptyFile;
	}

	void setEmptyFileStatus(int v)
	{
		m_isEmptyFile = v;
	}

	//处理不相等的多行，将多行对比后合并为一个内部块结构
	virtual void blockCmpLcs(const BlocksInfo & leftBlockInfo, QList<LineFileInfo> &leftLinesInfo, \
		const BlocksInfo & rightBlockInfo , QList<LineFileInfo> &rightLinesInfo, \
		QVector<BlockNode> &leftBlocks, QVector<BlockNode> &rightBlocks) = 0;
	//处理相等的行，将多行合并为一个内部块结构
	virtual void createBlockWithMultiEqualLines(const BlocksInfo & leftBlockInfo, QList<LineFileInfo> &leftLinesInfo, \
		const BlocksInfo & rightBlockInfo, QList<LineFileInfo> &rightLinesInfo, \
		QVector<BlockNode> &leftBlocks, QVector<BlockNode> &rightBlocks) = 0;

	virtual void createBlockWithOneLine(int leftLineNum, int rightLineNum, QList<LineFileInfo>& leftLinesInfo, QList<LineFileInfo>& rightLinesInfo, \
		QVector<BlockNode>& leftBlocks, QVector<BlockNode>& rightBlocks) = 0;

	volatile bool *m_isCancel;
private:
	AbstractCompare(const AbstractCompare &other);

protected:
	//空行是否参与行匹配
	bool m_isBlankLineDoMatch; //默认 true
	int m_isEmptyFile; //是否是空文件导致不需要比较
	int m_lineMatchEqualRata; //行认定匹配的相似率。100为尽可能的匹配最大相似行。90为超过90%则认定匹配，后续类似
};

