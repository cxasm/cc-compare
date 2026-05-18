#pragma once

#include "CmpareMode.h"
#include "AbstractCompare.h"
#include "rcglobal.h"


#define DEAL_SIGNAL_ANSI

class BlockCompare :public AbstractCompare
{
public:
	BlockCompare();
	virtual ~BlockCompare();
	//处理不相等的块。结果也是一个个小的块。
	virtual void blockCmpLcs(const BlocksInfo & leftBlockInfo, QList<LineFileInfo>& leftLinesInfo, \
		const BlocksInfo & rightBlockInfo, QList<LineFileInfo>& rightLinesInfo, \
		QVector<BlockNode> &leftBlocks, QVector<BlockNode> &rightBlocks);
	virtual void createBlockWithMultiEqualLines(const BlocksInfo & leftBlockInfo, QList<LineFileInfo>& leftLinesInfo, const BlocksInfo & rightBlockInfo, QList<LineFileInfo>& rightLinesInfo, QVector<BlockNode>& leftBlocks, QVector<BlockNode>& rightBlocks);

	static void fillCmpLineResutl(bool status, const QString & leftLineContent, const QString & rightLineContent, LineNode & outputLeft, LineNode & outputRight);

private:

	bool cmpLine(int leftLineNum, int rightLineNum, QList<LineFileInfo>& leftLinesInfo, QList<LineFileInfo>& rightLinesInfo, LineNode & outputLeft, LineNode & outputRight);
	void lessCmpMore(const BlocksInfo & lessBlockInfo, QList<LineFileInfo>& lessLinesInfo, \
		const BlocksInfo & moreBlockInfo, QList<LineFileInfo>& moreLinesInfo, \
		QVector<BlockNode>& leftBlocks, QVector<BlockNode>& rightBlocks);

	void lineCmpLcs(const int lineNum, QChar * lineSrc, QChar * lcsSrc, int lcsLens, QVector<SectionNode>& result);
	
	void createBlockWithOneLine(int leftLineNum, int rightLineNum, QList<LineFileInfo> &leftLinesInfo, QList<LineFileInfo> &rightLinesInfo,   QVector<BlockNode>& leftBlocks, QVector<BlockNode>& rightBlocks);
	
	void createBlockWithMultiNoEqualLines(const BlocksInfo & lessBlockInfo,  \
		QList<LineFileInfo>& lessLinesInfo, const BlocksInfo & moreBlockInfo, QList<LineFileInfo>& moreLinesInfo, \
		QVector<BlockNode>& lessBlocks, QVector<BlockNode>& moreBlocks);

	void dealLineTail(QVector<SectionNode>& result);
	inline void addSectionNode(QVector<SectionNode>& result, SectionNode & node);
};
